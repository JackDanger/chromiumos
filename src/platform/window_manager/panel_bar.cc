// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/panel_bar.h"

#include <algorithm>

#include <gflags/gflags.h>

#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/panel.h"
#include "window_manager/shadow.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_string(panel_bar_image, "../assets/images/panel_bar_bg.png",
              "Image to use for the panel bar's background");
DEFINE_string(panel_anchor_image, "../assets/images/panel_anchor.png",
              "Image to use for anchors on the panel bar");

namespace window_manager {

using chromeos::NewPermanentCallback;

// Amount of padding to place between titlebars in the panel bar.
static const int kBarPadding = 1;

// Amount of time to take for animations.
static const int kAnimMs = 150;

// Frequency with which we should update the position of dragged panels.
static const int kDraggedPanelUpdateMs = 25;

PanelBar::PanelBar(WindowManager* wm, int x, int y, int width, int height)
    : wm_(wm),
      x_(x),
      y_(y),
      width_(width),
      height_(height),
      collapsed_panel_width_(0),
      bar_actor_(wm_->clutter()->CreateImage(FLAGS_panel_bar_image)),
      bar_shadow_(new Shadow(wm_->clutter())),
      dragged_panel_(NULL),
      dragged_panel_event_coalescer_(
          NewPermanentCallback(this, &PanelBar::MoveDraggedPanel),
          kDraggedPanelUpdateMs),
      anchor_input_win_(None),
      anchor_panel_(NULL),
      anchor_actor_(wm_->clutter()->CreateImage(FLAGS_panel_anchor_image)),
      desired_panel_to_focus_(NULL),
      is_visible_(false),
      saw_map_request_(false) {
  bar_actor_->SetVisibility(false);
  wm_->stage()->AddActor(bar_actor_.get());
  wm_->stacking_manager()->StackActorAtTopOfLayer(
      bar_actor_.get(), StackingManager::LAYER_PANEL_BAR);
  bar_actor_->SetName("panel bar");
  bar_actor_->SetSize(width_, height_);
  bar_actor_->Move(x_, y_ + height, 0);
  bar_actor_->SetVisibility(true);

  bar_shadow_->group()->SetName("shadow group for panel bar");
  bar_shadow_->SetOpacity(0, 0);
  wm_->stage()->AddActor(bar_shadow_->group());
  wm_->stacking_manager()->StackActorAtTopOfLayer(
      bar_shadow_->group(), StackingManager::LAYER_PANEL_BAR);
  bar_shadow_->Move(x_, y_ + height, 0);
  bar_shadow_->Resize(width_, height_, 0);
  bar_shadow_->Show();

  anchor_actor_->SetName("panel anchor");
  anchor_actor_->SetOpacity(0, 0);
  wm_->stage()->AddActor(anchor_actor_.get());
  wm_->stacking_manager()->StackActorAtTopOfLayer(
      anchor_actor_.get(), StackingManager::LAYER_PANEL_BAR);
}

PanelBar::~PanelBar() {
}

bool PanelBar::HandleWindowMapRequest(Window* win) {
  saw_map_request_ = true;

  if (win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL &&
      win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR)
    return false;

  DoInitialSetupForWindow(win);
  win->MapClient();
  return true;
}

void PanelBar::HandleWindowMap(Window* win) {
  CHECK(win);

  if (win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL &&
      win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR)
    return;

  // Handle initial setup for existing windows for which we never saw a map
  // request event.
  if (!saw_map_request_)
    DoInitialSetupForWindow(win);

  switch (win->type()) {
    case WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR:
      // Don't do anything with panel titlebars when they're first
      // mapped; we'll handle them after we see the corresponding panel.
      break;
    case WmIpc::WINDOW_TYPE_CHROME_PANEL: {
      if (win->type_params().empty()) {
        LOG(WARNING) << "Panel " << win->xid_str() << " is missing type "
                     << "parameter for titlebar window";
        break;
      }
      Window* titlebar = wm_->GetWindow(win->type_params().at(0));
      if (!titlebar) {
        LOG(WARNING) << "Unable to find titlebar "
                     << XidStr(win->type_params()[0])
                     << " for panel " << win->xid_str();
        break;
      }

      // TODO(derat): Make the second param required after Chrome has been
      // updated.
      bool expanded = win->type_params().size() >= 2 ?
          win->type_params().at(1) : false;
      Panel* panel = CreatePanel(win, titlebar, expanded);
      if (expanded)
        FocusPanel(panel, false);  // remove_pointer_grab=false
      break;
    }
    default:
      NOTREACHED() << "Unhandled window type " << win->type();
  }

  if (num_panels() > 0 && !is_visible_) {
    SetVisibility(true);
  }
}

void PanelBar::HandleWindowUnmap(Window* win) {
  CHECK(win);
  Panel* panel = GetPanelByWindow(*win);
  if (!panel) {
    return;
  }

  std::vector<XWindow> input_windows;
  panel->GetInputWindows(&input_windows);
  for (std::vector<XWindow>::const_iterator it = input_windows.begin();
       it != input_windows.end(); ++it) {
    CHECK(panel_input_windows_.erase(*it) == 1);
  }

  if (dragged_panel_ == panel) {
    HandlePanelDragComplete(win);
  }
  if (anchor_panel_ == panel) {
    DestroyAnchor();
  }
  if (desired_panel_to_focus_ == panel) {
    desired_panel_to_focus_ = NULL;
  }

  // If this was a focused panel window, then we need to give the focus
  // away to someone else.
  if (panel->panel_win()->focused()) {
    // Try to find another expanded panel to focus.
    Panel* panel_to_focus = GetNearestExpandedPanel(panel);
    if (panel_to_focus) {
      FocusPanel(panel_to_focus, false);  // remove_pointer_grab=false
    } else {
      // Failing that, let the WindowManager decide what to do with it.
      wm_->SetActiveWindowProperty(None);
      wm_->TakeFocus();
    }
  }

  Panels::iterator it = FindPanelInVectorByWindow(collapsed_panels_, *win);
  if (it != collapsed_panels_.end()) {
    collapsed_panel_width_ -= ((*it)->titlebar_width() + kBarPadding);
    collapsed_panels_.erase(it);
    PackCollapsedPanels();
  } else {
    it = FindPanelInVectorByWindow(expanded_panels_, *win);
    if (it != expanded_panels_.end()) {
      expanded_panels_.erase(it);
    } else {
      LOG(WARNING) << "Got panel " << panel->xid_str() << " for window "
                   << win->xid_str() << " but didn't find it in "
                   << "collapsed_panels_ or expanded_panels_";
    }
  }

  if (num_panels() == 0 && is_visible_) {
    SetVisibility(false);
  }
}

bool PanelBar::HandleWindowConfigureRequest(
    Window* win, int req_x, int req_y, int req_width, int req_height) {
  Panel* panel = GetPanelByWindow(*win);
  if (!panel)
    return false;

  // Ignore the request (we'll get strange behavior if we honor a resize
  // request from the client while the user is manually resizing the
  // panel).
  // TODO: This means that panels can't resize themselves, which isn't what
  // we want.  If the user is currently resizing the window, we might want
  // to save the panel's resize request and apply it afterwards.
  return true;
}

bool PanelBar::HandleButtonPress(
    XWindow xid, int x, int y, int button, Time timestamp) {
  // If the press was in the anchor window, destroy the anchor and
  // collapse the corresponding panel.
  if (xid == anchor_input_win_) {
    if (button != 1) {
      return true;
    }
    VLOG(1) << "Got button press in anchor window";
    Panel* panel = anchor_panel_;
    DestroyAnchor();
    if (panel) {
      CollapsePanel(panel);
    } else {
      LOG(WARNING) << "Anchor panel no longer exists";
    }
    return true;
  }

  std::map<XWindow, Panel*>::iterator it = panel_input_windows_.find(xid);
  if (it != panel_input_windows_.end()) {
    it->second->HandleInputWindowButtonPress(xid, x, y, button, timestamp);
    return true;
  }

  // Otherwise, check if this was in a panel window whose button presses
  // we've grabbed.  If so, give the focus to the panel.
  Window* win = wm_->GetWindow(xid);
  if (win) {
    Panel* panel = GetPanelByWindow(*win);
    if (panel) {
      if (win == panel->panel_win()) {
        VLOG(1) << "Got button press in panel " << panel->xid_str()
                << "; giving it the focus";
        // Get rid of the passive button grab, and then ungrab the pointer
        // and replay events so the panel will get a copy of the click.
        FocusPanel(panel, true);  // remove_pointer_grab=true
      }
      return true;
    }
  }

  return false;
}

bool PanelBar::HandleButtonRelease(
    XWindow xid, int x, int y, int button, Time timestamp) {
  std::map<XWindow, Panel*>::iterator it = panel_input_windows_.find(xid);
  if (it != panel_input_windows_.end()) {
    it->second->HandleInputWindowButtonRelease(xid, x, y, button, timestamp);
    return true;
  }
  return false;
}

bool PanelBar::HandlePointerLeave(XWindow xid, Time timestamp) {
  // TODO: There appears to be a bit of a race condition here.  If the
  // mouse cursor has already been moved away before the anchor input
  // window gets created, the anchor never gets a mouse leave event.  Find
  // some way to work around this.
  if (xid != anchor_input_win_) {
    return false;
  }
  VLOG(1) << "Got mouse leave in anchor window";
  DestroyAnchor();
  return true;
}

bool PanelBar::HandlePointerMotion(XWindow xid, int x, int y, Time timestamp) {
  std::map<XWindow, Panel*>::iterator it = panel_input_windows_.find(xid);
  if (it != panel_input_windows_.end()) {
    it->second->HandleInputWindowPointerMotion(xid, x, y);
    return true;
  }
  return false;
}

bool PanelBar::HandleChromeMessage(const WmIpc::Message& msg) {
  switch (msg.type()) {
    // TODO: This is getting long; move cases into individual methods.
    case WmIpc::Message::WM_SET_PANEL_STATE: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win) {
        LOG(WARNING) << "Ignoring WM_SET_PANEL_STATE message for unknown "
                     << "window " << XidStr(xid);
        return true;
      }
      Panel* panel = GetPanelByWindow(*win);
      if (!panel) {
        LOG(WARNING) << "Ignoring WM_SET_PANEL_STATE message for non-panel "
                     << "window " << win->xid_str();
        return true;
      }
      if (msg.param(1)) {
        ExpandPanel(panel, true);  // create_anchor
      } else {
        CollapsePanel(panel);
      }
      break;
    }
    case WmIpc::Message::WM_MOVE_PANEL: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win) {
        LOG(WARNING) << "Ignoring WM_MOVE_PANEL message for unknown window "
                     << XidStr(xid);
        return true;
      }
      int x = msg.param(1);
      int y = msg.param(2);
      StorePanelPosition(win, x, y);
      break;
    }
    case WmIpc::Message::WM_NOTIFY_PANEL_DRAG_COMPLETE: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win) {
        LOG(WARNING) << "Ignoring WM_NOTIFY_PANEL_DRAG_COMPLETE message for "
                     << "unknown window " << XidStr(xid);
        return true;
      }
      HandlePanelDragComplete(win);
      break;
    }
    case WmIpc::Message::WM_FOCUS_WINDOW: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win) {
        LOG(WARNING) << "Got WM_FOCUS_WINDOW message for unknown window "
                     << XidStr(xid);
        return false;
      }
      Panel* panel = GetPanelByWindow(*win);
      if (!panel) {
        // Not a panel -- maybe it's a top-level window.
        return false;
      }
      if (!panel->is_expanded()) {
        LOG(WARNING) << "Ignoring WM_FOCUS_WINDOW message for collapsed panel "
                     << panel->xid_str();
        return true;
      }
      FocusPanel(panel, false);  // remove_pointer_grab=false
      break;
    }
    default:
      return false;
  }
  return true;
}

bool PanelBar::HandleClientMessage(const XClientMessageEvent& e) {
  Window* win = wm_->GetWindow(e.window);
  if (!win)
    return false;

  Panel* panel = GetPanelByWindow(*win);
  if (!panel)
    return false;

  if (e.message_type == wm_->GetXAtom(ATOM_NET_ACTIVE_WINDOW)) {
    if (e.format != XConnection::kLongFormat)
      return true;
    VLOG(1) << "Got _NET_ACTIVE_WINDOW request to focus " << XidStr(e.window)
            << " (requestor says its currently-active window is "
            << XidStr(e.data.l[2]) << "; real active window is "
            << XidStr(wm_->active_window_xid()) << ")";

    if (!panel->is_expanded())
      ExpandPanel(panel, false);  // create_anchor=false
    FocusPanel(panel, false);  // remove_pointer_grab=false
    return true;
  }
  return false;
}

bool PanelBar::HandleFocusChange(XWindow xid, bool focus_in) {
  Window* win = wm_->GetWindow(xid);
  if (!win) {
    return false;
  }
  Panel* panel = GetPanelByWindow(*win);
  if (!panel) {
    return false;
  }

  if (!focus_in) {
    VLOG(1) << "Panel " << panel->xid_str() << " lost focus; adding "
            << "button grab";
    panel->panel_win()->AddButtonGrab();
  }
  return true;
}

void PanelBar::MoveAndResize(int x, int y, int width, int height) {
  x_ = x;
  y_ = y;
  width_ = width;
  height_ = height;

  bar_actor_->SetSize(width_, height_);
  bar_actor_->Move(x_, y_ + (is_visible_ ? 0 : height_), 0);
  bar_shadow_->Resize(width_, height_, 0);
  bar_shadow_->Move(x_, y_ + (is_visible_ ? 0 : height_), 0);

  // Update all of the panels' Y positions...
  for (Panels::iterator it = expanded_panels_.begin();
       it != expanded_panels_.end(); ++it) {
    (*it)->HandlePanelBarMove();
  }
  for (Panels::iterator it = collapsed_panels_.begin();
       it != collapsed_panels_.end(); ++it) {
    (*it)->HandlePanelBarMove();
  }

  // ... and their X positions.
  PackCollapsedPanels();
  if (!expanded_panels_.empty())
    RepositionExpandedPanels(expanded_panels_[0].get());
}

void PanelBar::StorePanelPosition(Window* win, int x, int y) {
  CHECK(win);
  VLOG(2) << "Got request to move panel " << win->xid_str()
          << " to (" << x << ", " << y << ")";

  dragged_panel_event_coalescer_.StorePosition(x, y);

  if (!dragged_panel_ || dragged_panel_->panel_win() != win) {
    Panel* panel = GetPanelByWindow(*win);
    if (!panel) {
      LOG(WARNING) << "Unable to store position for unknown panel "
                   << win->xid_str();
      return;
    }
    StartDrag(panel);
  }
}

void PanelBar::HandlePanelDragComplete(Window* win) {
  CHECK(win);
  VLOG(2) << "Got notification that panel drag is complete for "
          << win->xid_str();

  if (!dragged_panel_ || dragged_panel_->panel_win() != win) {
    return;
  }

  if (dragged_panel_->is_expanded()) {
    dragged_panel_->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
    RepositionExpandedPanels(dragged_panel_);
  } else {
    dragged_panel_->StackAtTopOfLayer(StackingManager::LAYER_COLLAPSED_PANEL);
    // Snap collapsed dragged panels to their correct position.
    dragged_panel_->Move(dragged_panel_->snapped_right(), kAnimMs);
  }
  dragged_panel_ = NULL;

  if (dragged_panel_event_coalescer_.IsRunning()) {
    dragged_panel_event_coalescer_.Stop();
  }
}

void PanelBar::MoveDraggedPanel() {
  if (!dragged_panel_) {
    return;
  }

  const int drag_x = dragged_panel_event_coalescer_.x();
  dragged_panel_->Move(drag_x + dragged_panel_->titlebar_width(), 0);

  // When an expanded panel is being dragged, we don't move the other
  // panels to make room for it until the drag is done.
  if (dragged_panel_->is_expanded()) {
    return;
  }

  // For collapsed panels, we first find the position of the dragged panel.
  Panels::iterator dragged_it = FindPanelInVectorByWindow(
      collapsed_panels_, *(dragged_panel_->panel_win()));
  CHECK(dragged_it != collapsed_panels_.end());

  // Next, check if the center of the panel has moved over another panel.
  const int center_x = drag_x + 0.5 * dragged_panel_->titlebar_width();
  Panels::iterator it = find_if(collapsed_panels_.begin(),
                                collapsed_panels_.end(),
                                PanelTitlebarContainsPoint(center_x));

  // If it has, then we reorder the panels.
  if (it != collapsed_panels_.end() && it->get() != dragged_panel_) {
    if (it > dragged_it) {
      rotate(dragged_it, dragged_it + 1, it + 1);
    } else {
      rotate(it, dragged_it, dragged_it + 1);
    }
    PackCollapsedPanels();
  }
}

bool PanelBar::TakeFocus() {
  // If we already decided on a panel to focus, use it.
  if (desired_panel_to_focus_) {
    FocusPanel(desired_panel_to_focus_, false);  // remove_pointer_grab=false
    return true;
  }

  // Just focus the first expanded panel.
  if (expanded_panels_.empty()) {
    return false;
  }
  FocusPanel(expanded_panels_[0].get(), false);  // remove_pointer_grab=false
  return true;
}


bool PanelBar::PanelTitlebarContainsPoint::operator()(
    std::tr1::shared_ptr<Panel>& p) {
  return (center_x_ >= p->snapped_titlebar_left() &&
          center_x_ < p->snapped_right());
}


void PanelBar::DoInitialSetupForWindow(Window* win) {
  wm_->stacking_manager()->StackWindowAtTopOfLayer(
      win, StackingManager::LAYER_COLLAPSED_PANEL);
  win->MoveClientOffscreen();
}

Panel* PanelBar::CreatePanel(
    Window* panel_win, Window* titlebar_win, bool expanded) {
  CHECK(panel_win);
  CHECK(titlebar_win);

  VLOG(1) << "Adding " << (expanded ? "expanded" : "collapsed")
          << " panel with panel window " << panel_win->xid_str()
          << " and titlebar window " << titlebar_win->xid_str();

  const int right = expanded ?
      x_ + width_ - kBarPadding :
      x_ + width_ - collapsed_panel_width_ - kBarPadding;
  std::tr1::shared_ptr<Panel> panel(
      new Panel(this, panel_win, titlebar_win, right));

  std::vector<XWindow> input_windows;
  panel->GetInputWindows(&input_windows);
  for (std::vector<XWindow>::const_iterator it = input_windows.begin();
       it != input_windows.end(); ++it) {
    CHECK(panel_input_windows_.insert(std::make_pair(*it, panel.get())).second);
  }

  if (!expanded) {
    panel->StackAtTopOfLayer(StackingManager::LAYER_COLLAPSED_PANEL);
    collapsed_panels_.insert(collapsed_panels_.begin(), panel);
    collapsed_panel_width_ += panel->titlebar_width() + kBarPadding;
  } else {
    panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
    collapsed_panels_.push_back(panel);
    ExpandPanel(panel.get(), false);  // create_anchor
  }

  return panel.get();
}

void PanelBar::ExpandPanel(Panel* panel, bool create_anchor) {
  CHECK(panel);
  if (panel->is_expanded()) {
    LOG(WARNING) << "Ignoring request to expand already-expanded panel "
                 << panel->xid_str();
    return;
  }

  Panels::iterator it =
      FindPanelInVectorByWindow(collapsed_panels_, *(panel->panel_win()));
  CHECK(it != collapsed_panels_.end());
  std::tr1::shared_ptr<Panel> ref = *it;

  if (create_anchor) {
    CreateAnchor(panel);
  }
  panel->SetState(true);
  collapsed_panels_.erase(it);
  InsertExpandedPanel(ref);
  RepositionExpandedPanels(panel);
}

void PanelBar::CollapsePanel(Panel* panel) {
  CHECK(panel);
  if (!panel->is_expanded()) {
    LOG(WARNING) << "Ignoring request to collapse already-collapsed panel "
                 << panel->xid_str();
    return;
  }

  // In case we need to focus another panel, find the nearest one before we
  // collapse this one.
  Panel* panel_to_focus = GetNearestExpandedPanel(panel);

  // Move the panel from 'expanded_panels_' to 'collapsed_panels_'.
  Panels::iterator it =
      FindPanelInVectorByWindow(expanded_panels_, *(panel->panel_win()));
  CHECK(it != expanded_panels_.end());
  std::tr1::shared_ptr<Panel> ref = *it;

  if (anchor_panel_ == panel) {
    DestroyAnchor();
  }

  panel->SetState(false);
  expanded_panels_.erase(it);
  InsertCollapsedPanel(std::tr1::shared_ptr<Panel>(ref));
  PackCollapsedPanels();

  // Give up the focus if this panel had it.
  if (panel->panel_win()->focused()) {
    desired_panel_to_focus_ = panel_to_focus;
    if (!TakeFocus()) {
      wm_->SetActiveWindowProperty(None);
      wm_->TakeFocus();
    }
  }
}

void PanelBar::FocusPanel(Panel* panel, bool remove_pointer_grab) {
  CHECK(panel);
  panel->panel_win()->RemoveButtonGrab();
  if (remove_pointer_grab)
    wm_->xconn()->RemovePointerGrab(true, CurrentTime);  // replay_events
  wm_->SetActiveWindowProperty(panel->panel_win()->xid());
  panel->panel_win()->TakeFocus(wm_->GetCurrentTimeFromServer());
  panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
  desired_panel_to_focus_ = panel;
}

Panel* PanelBar::GetPanelByWindow(const Window& win) {
  Panels::iterator it = FindPanelInVectorByWindow(collapsed_panels_, win);
  if (it != collapsed_panels_.end()) {
    return it->get();
  }

  it = FindPanelInVectorByWindow(expanded_panels_, win);
  if (it != expanded_panels_.end()) {
    return it->get();
  }

  return NULL;
}

PanelBar::Panels::iterator PanelBar::FindPanelInVectorByWindow(
    Panels& panels, const Window& win) {
  for (Panels::iterator it = panels.begin(); it != panels.end(); ++it) {
    if ((*it)->titlebar_win() == &win || (*it)->panel_win() == &win) {
      return it;
    }
  }
  return panels.end();
}

int PanelBar::GetPanelIndex(Panels& panels, const Panel& panel) {
  Panels::iterator it =
      FindPanelInVectorByWindow(panels, *(panel.const_panel_win()));
  if (it == panels.end()) {
    return -1;
  }
  return it - panels.begin();
}

void PanelBar::StartDrag(Panel* panel) {
  if (dragged_panel_ == panel) {
    return;
  }

  if (dragged_panel_) {
    LOG(WARNING) << "Abandoning dragged panel " << dragged_panel_->xid_str()
                 << " in favor of " << panel->xid_str();
    if (dragged_panel_->is_expanded()) {
      RepositionExpandedPanels(dragged_panel_);
    }
  }

  VLOG(2) << "Starting drag of panel " << panel->xid_str();
  dragged_panel_ = panel;

  panel->StackAtTopOfLayer(
      panel->is_expanded() ?
      StackingManager::LAYER_DRAGGED_EXPANDED_PANEL :
      StackingManager::LAYER_DRAGGED_COLLAPSED_PANEL);

  if (!dragged_panel_event_coalescer_.IsRunning())
    dragged_panel_event_coalescer_.Start();
}

void PanelBar::PackCollapsedPanels() {
  collapsed_panel_width_ = 0;

  for (Panels::reverse_iterator it = collapsed_panels_.rbegin();
       it != collapsed_panels_.rend(); ++it) {
    Panel* panel = it->get();

    int right = x_ + width_ - collapsed_panel_width_ - kBarPadding;
    panel->set_snapped_right(right);
    collapsed_panel_width_ += panel->titlebar_width() + kBarPadding;

    if (panel != dragged_panel_) {
      panel->Move(right, kAnimMs);
    }
  }
}

void PanelBar::RepositionExpandedPanels(Panel* fixed_panel) {
  CHECK(fixed_panel);

  // First, find the index of the fixed panel.
  int fixed_index = GetPanelIndex(expanded_panels_, *fixed_panel);
  CHECK_LT(fixed_index, static_cast<int>(expanded_panels_.size()));

  // Next, check if the panel has moved to the other side of another panel.
  const int center_x = fixed_panel->cur_panel_center();
  for (size_t i = 0; i < expanded_panels_.size(); ++i) {
    Panel* panel = expanded_panels_[i].get();
    if (center_x <= panel->cur_panel_center() ||
        i == expanded_panels_.size() - 1) {
      if (panel != fixed_panel) {
        // If it has, then we reorder the panels.
        std::tr1::shared_ptr<Panel> ref = expanded_panels_[fixed_index];
        expanded_panels_.erase(expanded_panels_.begin() + fixed_index);
        if (i < expanded_panels_.size()) {
          expanded_panels_.insert(expanded_panels_.begin() + i, ref);
        } else {
          expanded_panels_.push_back(ref);
        }
      }
      break;
    }
  }

  // Find the total width of the panels to the left of the fixed panel.
  int total_width = 0;
  fixed_index = -1;
  for (int i = 0; i < static_cast<int>(expanded_panels_.size()); ++i) {
    Panel* panel = expanded_panels_[i].get();
    if (panel == fixed_panel) {
      fixed_index = i;
      break;
    }
    total_width += panel->panel_width();
  }
  CHECK_NE(fixed_index, -1);
  int new_fixed_index = fixed_index;

  // Move panels over to the right of the fixed panel until all of the ones
  // on the left will fit.
  int avail_width = std::max(fixed_panel->cur_panel_left() - kBarPadding - x_,
                             0);
  while (total_width > avail_width) {
    new_fixed_index--;
    CHECK(new_fixed_index >= 0);
    total_width -= expanded_panels_[new_fixed_index]->panel_width();
  }

  // Reorder the fixed panel if its index changed.
  if (new_fixed_index != fixed_index) {
    Panels::iterator it = expanded_panels_.begin() + fixed_index;
    std::tr1::shared_ptr<Panel> ref = *it;
    expanded_panels_.erase(it);
    expanded_panels_.insert(expanded_panels_.begin() + new_fixed_index, ref);
    fixed_index = new_fixed_index;
  }

  // Now find the width of the panels to the right, and move them to the
  // left as needed.
  total_width = 0;
  for (Panels::iterator it = expanded_panels_.begin() + fixed_index + 1;
       it != expanded_panels_.end(); ++it) {
    total_width += (*it)->panel_width();
  }

  avail_width = std::max(x_ + width_ - (fixed_panel->cur_right() + kBarPadding),
                         0);
  while (total_width > avail_width) {
    new_fixed_index++;
    CHECK_LT(new_fixed_index, static_cast<int>(expanded_panels_.size()));
    total_width -= expanded_panels_[new_fixed_index]->panel_width();
  }

  // Do the reordering again.
  if (new_fixed_index != fixed_index) {
    Panels::iterator it = expanded_panels_.begin() + fixed_index;
    std::tr1::shared_ptr<Panel> ref = *it;
    expanded_panels_.erase(it);
    expanded_panels_.insert(expanded_panels_.begin() + new_fixed_index, ref);
    fixed_index = new_fixed_index;
  }

  // Finally, push panels to the left and the right so they don't overlap.
  int boundary = expanded_panels_[fixed_index]->cur_panel_left() - kBarPadding;
  for (Panels::reverse_iterator it =
         // Start at the panel to the left of 'new_fixed_index'.
         expanded_panels_.rbegin() +
         (expanded_panels_.size() - new_fixed_index);
       it != expanded_panels_.rend(); ++it) {
    Panel* panel = it->get();
    if (panel->cur_right() > boundary) {
      panel->Move(boundary, kAnimMs);
    } else if (panel->cur_panel_left() < x_) {
      panel->Move(std::min(boundary, x_ + panel->panel_width() + kBarPadding),
                  kAnimMs);
    }
    boundary = panel->cur_panel_left() - kBarPadding;
  }

  boundary = expanded_panels_[fixed_index]->cur_right() + kBarPadding;
  for (Panels::iterator it = expanded_panels_.begin() + new_fixed_index + 1;
       it != expanded_panels_.end(); ++it) {
    Panel* panel = it->get();
    if (panel->cur_panel_left() < boundary) {
      panel->Move(boundary + panel->panel_width(), kAnimMs);
    } else if (panel->cur_right() > x_ + width_) {
      panel->Move(
          std::max(boundary + panel->panel_width(), width_ - kBarPadding),
                   kAnimMs);
    }
    boundary = panel->cur_right() + kBarPadding;
  }
}

void PanelBar::InsertCollapsedPanel(std::tr1::shared_ptr<Panel> new_panel) {
  size_t index = 0;
  for (; index < collapsed_panels_.size(); ++index) {
    Panel* panel = collapsed_panels_[index].get();
    if (new_panel->cur_titlebar_left() < panel->cur_titlebar_left()) {
      break;
    }
  }
  collapsed_panels_.insert(collapsed_panels_.begin() + index, new_panel);
}

void PanelBar::InsertExpandedPanel(std::tr1::shared_ptr<Panel> new_panel) {
  size_t index = 0;
  for (; index < expanded_panels_.size(); ++index) {
    Panel* panel = expanded_panels_[index].get();
    if (new_panel->cur_panel_left() < panel->cur_panel_left()) {
      break;
    }
  }
  expanded_panels_.insert(expanded_panels_.begin() + index, new_panel);
}

void PanelBar::CreateAnchor(Panel* panel) {
  if (anchor_input_win_ != None) {
    LOG(WARNING) << "Destroying extra input window "
                 << XidStr(anchor_input_win_);
    wm_->xconn()->DestroyWindow(anchor_input_win_);
  }
  anchor_input_win_ = wm_->CreateInputWindow(
      panel->cur_titlebar_left(), y_, panel->titlebar_width(), height_);
  anchor_panel_ = panel;

  anchor_actor_->Move(
      panel->cur_titlebar_left() +
        0.5 * (panel->titlebar_width() - anchor_actor_->GetWidth()),
      y_ + 0.5 * (height_ - anchor_actor_->GetHeight()),
      0);  // anim_ms
  anchor_actor_->SetOpacity(1, kAnimMs);
}

void PanelBar::DestroyAnchor() {
  if (anchor_input_win_ != None) {
    wm_->xconn()->DestroyWindow(anchor_input_win_);
    anchor_input_win_ = None;
  }
  anchor_actor_->SetOpacity(0, kAnimMs);
  anchor_panel_ = NULL;
  PackCollapsedPanels();
}

Panel* PanelBar::GetNearestExpandedPanel(Panel* panel) {
  if (!panel || !panel->is_expanded()) {
    return NULL;
  }

  Panel* nearest_panel = NULL;
  int best_distance = kint32max;
  for (Panels::iterator it = expanded_panels_.begin();
       it != expanded_panels_.end(); ++it) {
    if (it->get() == panel) {
      continue;
    }

    int distance = kint32max;
    if ((*it)->cur_right() <= panel->cur_panel_left()) {
      distance = panel->cur_panel_left() - (*it)->cur_right();
    } else if ((*it)->cur_panel_left() >= panel->cur_right()) {
      distance = (*it)->cur_panel_left() - panel->cur_right();
    } else {
      distance = abs((*it)->cur_panel_center() - panel->cur_panel_center());
    }

    if (distance < best_distance) {
      best_distance = distance;
      nearest_panel = it->get();
    }
  }
  return nearest_panel;
}

void PanelBar::SetVisibility(bool visible) {
  if (is_visible_ == visible) {
    return;
  }

  if (visible) {
    bar_actor_->MoveY(y_, kAnimMs);
    bar_shadow_->MoveY(y_, kAnimMs);
    bar_shadow_->SetOpacity(1, kAnimMs);
  } else {
    bar_actor_->MoveY(y_ + height_, kAnimMs);
    bar_shadow_->MoveY(y_ + height_, kAnimMs);
    bar_shadow_->SetOpacity(0, kAnimMs);
  }
  is_visible_ = visible;
  wm_->HandlePanelBarVisibilityChange(visible);
}

}  // namespace window_manager
