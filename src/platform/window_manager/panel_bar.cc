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
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_string(panel_bar_image, "../assets/images/panel_bar_bg.png",
              "Image to use for the panel bar's background");
DEFINE_string(panel_anchor_image, "../assets/images/panel_anchor.png",
              "Image to use for anchors on the panel bar");

namespace window_manager {

using chromeos::NewPermanentCallback;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::tr1::shared_ptr;
using std::vector;

// Amount of padding to place between titlebars in the panel bar.
static const int kBarPadding = 1;

// Width of titlebars for collapsed panels.  Expanded panels' titlebars are
// resized to match the width of the content window.
static const int kCollapsedTitlebarWidth = 200;

// Amount of time to take for animations.
static const int kAnimMs = 150;

// Amount of time to take for expanding and collapsing panels.
static const int kPanelStateAnimMs = 150;

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

  if (win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL_CONTENT &&
      win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR)
    return false;

  DoInitialSetupForWindow(win);
  win->MapClient();
  return true;
}

void PanelBar::HandleWindowMap(Window* win) {
  CHECK(win);

  if (win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL_CONTENT &&
      win->type() != WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR)
    return;

  // Handle initial setup for existing windows for which we never saw a map
  // request event.
  if (!saw_map_request_)
    DoInitialSetupForWindow(win);

  switch (win->type()) {
    case WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR:
      // Don't do anything with panel titlebars when they're first
      // mapped; we'll handle them after we see the corresponding content
      // window.
      break;
    case WmIpc::WINDOW_TYPE_CHROME_PANEL_CONTENT: {
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
  if (!panel)
    return;

  vector<XWindow> input_windows;
  panel->GetInputWindows(&input_windows);
  for (vector<XWindow>::const_iterator it = input_windows.begin();
       it != input_windows.end(); ++it) {
    CHECK(panel_input_windows_.erase(*it) == 1);
  }

  if (dragged_panel_ == panel)
    HandlePanelDragComplete(win);
  if (anchor_panel_ == panel)
    DestroyAnchor();
  if (desired_panel_to_focus_ == panel)
    desired_panel_to_focus_ = NULL;

  // If this was a focused content window, then we need to try to find
  // another panel to focus.  We defer actually assigning the focus until
  // after we've fully dealt with the unmapped panel to avoid issues with
  // WindowManager::TakeFocus() calling PanelBar::TakeFocus() while we're
  // in an inconsistent state.
  bool need_to_assign_focus = panel->content_win()->focused();
  Panel* panel_to_focus = NULL;
  if (need_to_assign_focus)
    panel_to_focus = GetNearestExpandedPanel(panel);

  CHECK(panel_infos_.erase(panel) == 1);
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

  // Now assign the focus.
  if (need_to_assign_focus) {
    if (panel_to_focus) {
      // If we found a nearby panel, focus it.
      FocusPanel(panel_to_focus, false);  // remove_pointer_grab=false
    } else {
      // Failing that, let the WindowManager decide what to do with it.
      wm_->SetActiveWindowProperty(None);
      wm_->TakeFocus();
    }
  }

  if (num_panels() == 0 && is_visible_)
    SetVisibility(false);
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

bool PanelBar::HandleButtonPress(XWindow xid,
                                 int x, int y,
                                 int x_root, int y_root,
                                 int button,
                                 Time timestamp) {
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

  map<XWindow, Panel*>::iterator it = panel_input_windows_.find(xid);
  if (it != panel_input_windows_.end()) {
    it->second->HandleInputWindowButtonPress(xid, x, y, button, timestamp);
    return true;
  }

  // Otherwise, check if this was in a content window whose button presses
  // we've grabbed.  If so, give the focus to the panel.
  Window* win = wm_->GetWindow(xid);
  if (win) {
    Panel* panel = GetPanelByWindow(*win);
    if (panel) {
      if (win == panel->content_win()) {
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

bool PanelBar::HandleButtonRelease(XWindow xid,
                                   int x, int y,
                                   int x_root, int y_root,
                                   int button,
                                   Time timestamp) {
  map<XWindow, Panel*>::iterator it = panel_input_windows_.find(xid);
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
  map<XWindow, Panel*>::iterator it = panel_input_windows_.find(xid);
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
    case WmIpc::Message::WM_NOTIFY_PANEL_DRAGGED: {
      XWindow xid = msg.param(0);
      Window* win = wm_->GetWindow(xid);
      if (!win) {
        LOG(WARNING) << "Ignoring WM_NOTIFY_PANEL_DRAGGED message for unknown "
                     << "window " << XidStr(xid);
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
      if (!GetPanelInfoOrDie(panel)->is_expanded) {
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

    if (!GetPanelInfoOrDie(panel)->is_expanded)
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
    panel->content_win()->AddButtonGrab();
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
    Panel* panel = it->get();
    panel->MoveY(y_ - panel->total_height(), true, 0);
  }
  for (Panels::iterator it = collapsed_panels_.begin();
       it != collapsed_panels_.end(); ++it) {
    Panel* panel = it->get();
    panel->MoveY(y_ + height_ - panel->titlebar_height(), true, 0);
  }

  // ... and their X positions.
  PackCollapsedPanels();
  if (!expanded_panels_.empty()) {
    // Arbitrarily move the first panel onscreen, if it isn't already, and
    // then try to arrange all of the other panels to not overlap.
    Panel* fixed_panel = expanded_panels_[0].get();
    MoveExpandedPanelOnscreen(fixed_panel, kAnimMs);
    RepositionExpandedPanels(fixed_panel);
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


PanelBar::PanelInfo* PanelBar::GetPanelInfoOrDie(Panel* panel) {
  shared_ptr<PanelInfo> info =
      FindWithDefault(panel_infos_, panel, shared_ptr<PanelInfo>());
  CHECK(info.get());
  return info.get();
}

void PanelBar::DoInitialSetupForWindow(Window* win) {
  wm_->stacking_manager()->StackWindowAtTopOfLayer(
      win, StackingManager::LAYER_COLLAPSED_PANEL);
  win->MoveClientOffscreen();
}

Panel* PanelBar::CreatePanel(
    Window* content_win, Window* titlebar_win, bool expanded) {
  CHECK(content_win);
  CHECK(titlebar_win);

  VLOG(1) << "Adding " << (expanded ? "expanded" : "collapsed")
          << " panel with content window " << content_win->xid_str()
          << " and titlebar window " << titlebar_win->xid_str();

  // Determine the offscreen position from which we want the panel to
  // animate in.
  const int initial_right = expanded ?
      x_ + width_ - kBarPadding :
      x_ + width_ - collapsed_panel_width_ - kBarPadding;
  const int initial_y = y_ + height_;

  shared_ptr<Panel> panel(
      new Panel(wm_, content_win, titlebar_win, initial_right, initial_y));

  vector<XWindow> input_windows;
  panel->GetInputWindows(&input_windows);
  for (vector<XWindow>::const_iterator it = input_windows.begin();
       it != input_windows.end(); ++it) {
    CHECK(panel_input_windows_.insert(make_pair(*it, panel.get())).second);
  }

  shared_ptr<PanelInfo> info(new PanelInfo);
  info->is_expanded = false;
  info->snapped_right = initial_right;
  panel_infos_.insert(make_pair(panel.get(), info));

  if (!expanded) {
    ConfigureCollapsedPanel(panel.get());
    collapsed_panels_.insert(collapsed_panels_.begin(), panel);
    collapsed_panel_width_ += panel->titlebar_width() + kBarPadding;
  } else {
    collapsed_panels_.push_back(panel);
    ExpandPanel(panel.get(), false);  // create_anchor
  }

  return panel.get();
}

void PanelBar::ExpandPanel(Panel* panel, bool create_anchor) {
  CHECK(panel);
  PanelInfo* info = GetPanelInfoOrDie(panel);
  if (info->is_expanded) {
    LOG(WARNING) << "Ignoring request to expand already-expanded panel "
                 << panel->xid_str();
    return;
  }

  panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
  panel->SetTitlebarWidth(panel->content_width());
  panel->MoveY(y_ - panel->total_height(), true, kPanelStateAnimMs);
  panel->SetResizable(true);
  panel->SetContentShadowOpacity(1.0, kPanelStateAnimMs);
  panel->NotifyChromeAboutState(true);
  info->is_expanded = true;

  Panels::iterator it =
      FindPanelInVectorByWindow(collapsed_panels_, *(panel->content_win()));
  CHECK(it != collapsed_panels_.end());
  shared_ptr<Panel> ref = *it;
  collapsed_panels_.erase(it);
  InsertExpandedPanel(ref);
  RepositionExpandedPanels(panel);

  if (create_anchor)
    CreateAnchor(panel);
}

void PanelBar::CollapsePanel(Panel* panel) {
  CHECK(panel);
  PanelInfo* info = GetPanelInfoOrDie(panel);
  if (!info->is_expanded) {
    LOG(WARNING) << "Ignoring request to collapse already-collapsed panel "
                 << panel->xid_str();
    return;
  }

  // In case we need to focus another panel, find the nearest one before we
  // collapse this one.
  Panel* panel_to_focus = GetNearestExpandedPanel(panel);

  if (anchor_panel_ == panel)
    DestroyAnchor();

  ConfigureCollapsedPanel(panel);

  // Move the panel from 'expanded_panels_' to 'collapsed_panels_'.
  Panels::iterator it =
      FindPanelInVectorByWindow(expanded_panels_, *(panel->content_win()));
  CHECK(it != expanded_panels_.end());
  shared_ptr<Panel> ref = *it;
  expanded_panels_.erase(it);
  InsertCollapsedPanel(shared_ptr<Panel>(ref));
  PackCollapsedPanels();

  // Give up the focus if this panel had it.
  if (panel->content_win()->focused()) {
    desired_panel_to_focus_ = panel_to_focus;
    if (!TakeFocus()) {
      wm_->SetActiveWindowProperty(None);
      wm_->TakeFocus();
    }
  }
}

void PanelBar::ConfigureCollapsedPanel(Panel* panel) {
  panel->StackAtTopOfLayer(StackingManager::LAYER_COLLAPSED_PANEL);
  panel->SetTitlebarWidth(kCollapsedTitlebarWidth);
  panel->MoveY(y_ + height_ - panel->titlebar_height(),
               true, kPanelStateAnimMs);
  panel->SetResizable(false);
  // Hide the shadow so it's not peeking up at the bottom of the screen.
  panel->SetContentShadowOpacity(0, kPanelStateAnimMs);
  panel->NotifyChromeAboutState(false);

  PanelInfo* info = GetPanelInfoOrDie(panel);
  info->is_expanded = false;
}

void PanelBar::FocusPanel(Panel* panel, bool remove_pointer_grab) {
  CHECK(panel);
  panel->content_win()->RemoveButtonGrab();
  if (remove_pointer_grab)
    wm_->xconn()->RemovePointerGrab(true, CurrentTime);  // replay_events
  wm_->SetActiveWindowProperty(panel->content_win()->xid());
  panel->content_win()->TakeFocus(wm_->GetCurrentTimeFromServer());
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
    if ((*it)->titlebar_win() == &win || (*it)->content_win() == &win) {
      return it;
    }
  }
  return panels.end();
}

int PanelBar::GetPanelIndex(Panels& panels, const Panel& panel) {
  Panels::iterator it =
      FindPanelInVectorByWindow(panels, *(panel.const_content_win()));
  if (it == panels.end()) {
    return -1;
  }
  return it - panels.begin();
}

void PanelBar::StartDrag(Panel* panel) {
  if (dragged_panel_ == panel)
    return;

  if (dragged_panel_) {
    LOG(WARNING) << "Abandoning dragged panel " << dragged_panel_->xid_str()
                 << " in favor of " << panel->xid_str();
    if (GetPanelInfoOrDie(dragged_panel_)->is_expanded)
      RepositionExpandedPanels(dragged_panel_);
  }

  VLOG(2) << "Starting drag of panel " << panel->xid_str();
  dragged_panel_ = panel;

  panel->StackAtTopOfLayer(
      GetPanelInfoOrDie(panel)->is_expanded ?
        StackingManager::LAYER_DRAGGED_EXPANDED_PANEL :
        StackingManager::LAYER_DRAGGED_COLLAPSED_PANEL);

  if (!dragged_panel_event_coalescer_.IsRunning())
    dragged_panel_event_coalescer_.Start();
}

void PanelBar::StorePanelPosition(Window* win, int x, int y) {
  CHECK(win);
  VLOG(2) << "Got request to move panel " << win->xid_str()
          << " to (" << x << ", " << y << ")";

  dragged_panel_event_coalescer_.StorePosition(x, y);

  if (!dragged_panel_ || dragged_panel_->content_win() != win) {
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

  if (!dragged_panel_ || dragged_panel_->content_win() != win)
    return;

  PanelInfo* info = GetPanelInfoOrDie(dragged_panel_);
  if (info->is_expanded) {
    dragged_panel_->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
    // Tell the panel to move its client windows to match its composited
    // position.
    dragged_panel_->MoveX(dragged_panel_->right(), true, 0);
    RepositionExpandedPanels(dragged_panel_);
  } else {
    dragged_panel_->StackAtTopOfLayer(StackingManager::LAYER_COLLAPSED_PANEL);
    // Snap collapsed dragged panels to their correct position.
    dragged_panel_->MoveX(info->snapped_right, true, kAnimMs);
  }
  dragged_panel_ = NULL;

  if (dragged_panel_event_coalescer_.IsRunning())
    dragged_panel_event_coalescer_.Stop();
}

void PanelBar::MoveDraggedPanel() {
  if (!dragged_panel_)
    return;

  const int drag_x = dragged_panel_event_coalescer_.x();
  dragged_panel_->MoveX((wm_->wm_ipc_version() >= 1) ?
                          drag_x :
                          drag_x + dragged_panel_->titlebar_width(),
                        false, 0);

  // When an expanded panel is being dragged, we don't move the other
  // panels to make room for it until the drag is done.
  if (GetPanelInfoOrDie(dragged_panel_)->is_expanded)
    return;

  // For collapsed panels, we first find the position of the dragged panel.
  Panels::iterator dragged_it = FindPanelInVectorByWindow(
      collapsed_panels_, *(dragged_panel_->content_win()));
  CHECK(dragged_it != collapsed_panels_.end());

  // Next, check if the center of the panel has moved over another panel.
  const int center_x = (wm_->wm_ipc_version() >= 1) ?
                       drag_x - 0.5 * dragged_panel_->titlebar_width() :
                       drag_x + 0.5 * dragged_panel_->titlebar_width();
  Panels::iterator it = collapsed_panels_.begin();
  for (; it != collapsed_panels_.end(); ++it) {
    int snapped_left = 0, snapped_right = 0;
    if (it->get() == dragged_panel_) {
      // If we're comparing against ourselves, use our original position
      // rather than wherever we've currently been dragged by the user.
      PanelInfo* info = GetPanelInfoOrDie(dragged_panel_);
      snapped_left = info->snapped_right - dragged_panel_->titlebar_width();
      snapped_right = info->snapped_right;
    } else {
      snapped_left = (*it)->titlebar_x();
      snapped_right = (*it)->right();
    }
    if (center_x >= snapped_left && center_x < snapped_right)
      break;
  }

  // If it has, then we reorder the panels.
  if (it != collapsed_panels_.end() && it->get() != dragged_panel_) {
    if (it > dragged_it)
      rotate(dragged_it, dragged_it + 1, it + 1);
    else
      rotate(it, dragged_it, dragged_it + 1);
    PackCollapsedPanels();
  }
}

void PanelBar::PackCollapsedPanels() {
  collapsed_panel_width_ = 0;

  for (Panels::reverse_iterator it = collapsed_panels_.rbegin();
       it != collapsed_panels_.rend(); ++it) {
    Panel* panel = it->get();
    // TODO: PackCollapsedPanels() gets called in response to every move
    // message that we receive about a dragged panel.  Check that it's not
    // too inefficient to do all of these lookups vs. storing the panel
    // info alongside the Panel pointer in 'collapsed_panels_'.
    PanelInfo* info = GetPanelInfoOrDie(panel);

    info->snapped_right = x_ + width_ - collapsed_panel_width_ - kBarPadding;
    if (panel != dragged_panel_ && panel->right() != info->snapped_right)
      panel->MoveX(info->snapped_right, true, kAnimMs);

    collapsed_panel_width_ += panel->titlebar_width() + kBarPadding;
  }
}

void PanelBar::RepositionExpandedPanels(Panel* fixed_panel) {
  CHECK(fixed_panel);

  // First, find the index of the fixed panel.
  int fixed_index = GetPanelIndex(expanded_panels_, *fixed_panel);
  CHECK_LT(fixed_index, static_cast<int>(expanded_panels_.size()));

  // Next, check if the panel has moved to the other side of another panel.
  const int center_x = fixed_panel->content_center();
  for (size_t i = 0; i < expanded_panels_.size(); ++i) {
    Panel* panel = expanded_panels_[i].get();
    if (center_x <= panel->content_center() ||
        i == expanded_panels_.size() - 1) {
      if (panel != fixed_panel) {
        // If it has, then we reorder the panels.
        shared_ptr<Panel> ref = expanded_panels_[fixed_index];
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
    total_width += panel->content_width();
  }
  CHECK_NE(fixed_index, -1);
  int new_fixed_index = fixed_index;

  // Move panels over to the right of the fixed panel until all of the ones
  // on the left will fit.
  int avail_width = max(fixed_panel->content_x() - kBarPadding - x_,
                             0);
  while (total_width > avail_width) {
    new_fixed_index--;
    CHECK(new_fixed_index >= 0);
    total_width -= expanded_panels_[new_fixed_index]->content_width();
  }

  // Reorder the fixed panel if its index changed.
  if (new_fixed_index != fixed_index) {
    Panels::iterator it = expanded_panels_.begin() + fixed_index;
    shared_ptr<Panel> ref = *it;
    expanded_panels_.erase(it);
    expanded_panels_.insert(expanded_panels_.begin() + new_fixed_index, ref);
    fixed_index = new_fixed_index;
  }

  // Now find the width of the panels to the right, and move them to the
  // left as needed.
  total_width = 0;
  for (Panels::iterator it = expanded_panels_.begin() + fixed_index + 1;
       it != expanded_panels_.end(); ++it) {
    total_width += (*it)->content_width();
  }

  avail_width = max(x_ + width_ - (fixed_panel->right() + kBarPadding), 0);
  while (total_width > avail_width) {
    new_fixed_index++;
    CHECK_LT(new_fixed_index, static_cast<int>(expanded_panels_.size()));
    total_width -= expanded_panels_[new_fixed_index]->content_width();
  }

  // Do the reordering again.
  if (new_fixed_index != fixed_index) {
    Panels::iterator it = expanded_panels_.begin() + fixed_index;
    shared_ptr<Panel> ref = *it;
    expanded_panels_.erase(it);
    expanded_panels_.insert(expanded_panels_.begin() + new_fixed_index, ref);
    fixed_index = new_fixed_index;
  }

  // Finally, push panels to the left and the right so they don't overlap.
  int boundary = expanded_panels_[fixed_index]->content_x() - kBarPadding;
  for (Panels::reverse_iterator it =
         // Start at the panel to the left of 'new_fixed_index'.
         expanded_panels_.rbegin() +
         (expanded_panels_.size() - new_fixed_index);
       it != expanded_panels_.rend(); ++it) {
    Panel* panel = it->get();
    if (panel->right() > boundary) {
      panel->MoveX(boundary, true, kAnimMs);
    } else if (panel->content_x() < x_) {
      panel->MoveX(
          min(boundary, x_ + panel->content_width() + kBarPadding),
          true, kAnimMs);
    }
    boundary = panel->content_x() - kBarPadding;
  }

  boundary = expanded_panels_[fixed_index]->right() + kBarPadding;
  for (Panels::iterator it = expanded_panels_.begin() + new_fixed_index + 1;
       it != expanded_panels_.end(); ++it) {
    Panel* panel = it->get();
    if (panel->content_x() < boundary) {
      panel->MoveX(boundary + panel->content_width(), true, kAnimMs);
    } else if (panel->right() > x_ + width_) {
      panel->MoveX(max(boundary + panel->content_width(), width_ - kBarPadding),
                   true, kAnimMs);
    }
    boundary = panel->right() + kBarPadding;
  }
}

void PanelBar::InsertCollapsedPanel(shared_ptr<Panel> new_panel) {
  size_t index = 0;
  for (; index < collapsed_panels_.size(); ++index) {
    Panel* panel = collapsed_panels_[index].get();
    if (new_panel->titlebar_x() < panel->titlebar_x()) {
      break;
    }
  }
  collapsed_panels_.insert(collapsed_panels_.begin() + index, new_panel);
}

void PanelBar::InsertExpandedPanel(shared_ptr<Panel> new_panel) {
  size_t index = 0;
  for (; index < expanded_panels_.size(); ++index) {
    Panel* panel = expanded_panels_[index].get();
    if (new_panel->content_x() < panel->content_x()) {
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
      panel->titlebar_x(), y_, panel->titlebar_width(), height_,
      ButtonPressMask | LeaveWindowMask);
  anchor_panel_ = panel;

  anchor_actor_->Move(
      panel->titlebar_x() +
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
  if (!panel || !GetPanelInfoOrDie(panel)->is_expanded)
    return NULL;

  Panel* nearest_panel = NULL;
  int best_distance = kint32max;
  for (Panels::iterator it = expanded_panels_.begin();
       it != expanded_panels_.end(); ++it) {
    if (it->get() == panel)
      continue;

    int distance = kint32max;
    if ((*it)->right() <= panel->content_x())
      distance = panel->content_x() - (*it)->right();
    else if ((*it)->content_x() >= panel->right())
      distance = (*it)->content_x() - panel->right();
    else
      distance = abs((*it)->content_center() - panel->content_center());

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

void PanelBar::MoveExpandedPanelOnscreen(Panel* panel, int anim_ms) {
  CHECK(GetPanelInfoOrDie(panel)->is_expanded);
  if (panel->content_x() < x_)
    panel->MoveX(x_ + panel->content_width(), true, anim_ms);
  else if (panel->right() > x_ + width_)
    panel->MoveX(x_ + width_, true, anim_ms);
}

}  // namespace window_manager
