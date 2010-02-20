// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/panel_bar.h"

#include <algorithm>

#include <gflags/gflags.h>

#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer_registrar.h"
#include "window_manager/panel.h"
#include "window_manager/panel_manager.h"
#include "window_manager/pointer_position_watcher.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_string(panel_bar_image, "", "deprecated");
DEFINE_string(panel_anchor_image, "../assets/images/panel_anchor.png",
              "Image to use for anchors on the panel bar");

namespace window_manager {

using chromeos::NewPermanentCallback;
using std::find;
using std::make_pair;
using std::map;
using std::max;
using std::min;
using std::tr1::shared_ptr;
using std::vector;

const int PanelBar::kPixelsBetweenPanels = 3;
const int PanelBar::kShowCollapsedPanelsDistancePixels = 1;
const int PanelBar::kHideCollapsedPanelsDistancePixels = 30;
const int PanelBar::kHiddenCollapsedPanelHeightPixels = 3;

// Amount of time to take when arranging panels.
static const int kPanelArrangeAnimMs = 150;

// Amount of time to take when fading the panel anchor in or out.
static const int kAnchorFadeAnimMs = 150;

// Amount of time to take for expanding and collapsing panels.
static const int kPanelStateAnimMs = 150;

// Amount of time to take when animating a dropped panel sliding into the
// panel bar.
static const int kDroppedPanelAnimMs = 50;

// How many pixels away from the panel bar should a panel be dragged before
// it gets detached?
static const int kPanelDetachThresholdPixels = 50;

// How close does a panel need to get to the panel bar before it's attached?
static const int kPanelAttachThresholdPixels = 20;

// Amount of time to take when hiding or unhiding collapsed panels.
static const int kHideCollapsedPanelsAnimMs = 100;

// How long should we wait before showing collapsed panels when the user
// moves the pointer down to the bottom row of pixels?
static const int kShowCollapsedPanelsDelayMs = 200;

PanelBar::PanelBar(PanelManager* panel_manager)
    : panel_manager_(panel_manager),
      total_panel_width_(0),
      dragged_panel_(NULL),
      anchor_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1, ButtonPressMask)),
      anchor_panel_(NULL),
      anchor_actor_(wm()->clutter()->CreateImage(FLAGS_panel_anchor_image)),
      desired_panel_to_focus_(NULL),
      collapsed_panel_state_(COLLAPSED_PANEL_STATE_HIDDEN),
      show_collapsed_panels_input_xid_(
          wm()->CreateInputWindow(-1, -1, 1, 1,
                                  EnterWindowMask | LeaveWindowMask)),
      show_collapsed_panels_timer_id_(0),
      event_consumer_registrar_(
          new EventConsumerRegistrar(wm(), panel_manager)) {
  event_consumer_registrar_->RegisterForWindowEvents(anchor_input_xid_);
  event_consumer_registrar_->RegisterForWindowEvents(
      show_collapsed_panels_input_xid_);

  anchor_actor_->SetName("panel anchor");
  anchor_actor_->SetOpacity(0, 0);
  wm()->stage()->AddActor(anchor_actor_.get());
  wm()->stacking_manager()->StackActorAtTopOfLayer(
      anchor_actor_.get(), StackingManager::LAYER_PANEL_BAR_INPUT_WINDOW);

  // Stack the anchor input window above the show-collapsed-panels one so
  // we won't get spurious leave events in the former.
  wm()->stacking_manager()->StackXidAtTopOfLayer(
      show_collapsed_panels_input_xid_,
      StackingManager::LAYER_PANEL_BAR_INPUT_WINDOW);
  wm()->stacking_manager()->StackXidAtTopOfLayer(
      anchor_input_xid_, StackingManager::LAYER_PANEL_BAR_INPUT_WINDOW);
}

PanelBar::~PanelBar() {
  DisableShowCollapsedPanelsTimer();
  wm()->xconn()->DestroyWindow(anchor_input_xid_);
  anchor_input_xid_ = None;
  wm()->xconn()->DestroyWindow(show_collapsed_panels_input_xid_);
  show_collapsed_panels_input_xid_ = None;
}

WindowManager* PanelBar::wm() {
  return panel_manager_->wm();
}

void PanelBar::GetInputWindows(vector<XWindow>* windows_out) {
  CHECK(windows_out);
  windows_out->clear();
  windows_out->push_back(anchor_input_xid_);
  windows_out->push_back(show_collapsed_panels_input_xid_);
}

void PanelBar::AddPanel(Panel* panel, PanelSource source) {
  DCHECK(panel);

  shared_ptr<PanelInfo> info(new PanelInfo);
  info->snapped_right =
      wm()->width() - total_panel_width_ - kPixelsBetweenPanels;
  info->is_urgent = panel->content_win()->wm_hint_urgent();
  panel_infos_.insert(make_pair(panel, info));

  panels_.insert(panels_.begin(), panel);
  total_panel_width_ += panel->width() + kPixelsBetweenPanels;

  // If the panel is being dragged, move it to the correct position within
  // 'panels_' and repack all other panels.
  if (source == PANEL_SOURCE_DRAGGED)
    ReorderPanel(panel);

  panel->StackAtTopOfLayer(source == PANEL_SOURCE_DRAGGED ?
                           StackingManager::LAYER_DRAGGED_PANEL :
                           StackingManager::LAYER_STATIONARY_PANEL_IN_BAR);

  const int final_y = ComputePanelY(*panel, *(info.get()));

  // Now move the panel to its final position.
  switch (source) {
    case PANEL_SOURCE_NEW:
      // Make newly-created panels animate in from offscreen.
      panel->Move(info->snapped_right, wm()->height(), false, 0);
      panel->MoveY(final_y, true, kPanelStateAnimMs);
      break;
    case PANEL_SOURCE_DRAGGED:
      panel->MoveY(final_y, true, 0);
      break;
    case PANEL_SOURCE_DROPPED:
      panel->Move(info->snapped_right, final_y, true, kDroppedPanelAnimMs);
      break;
    default:
      NOTREACHED() << "Unknown panel source " << source;
  }

  panel->SetResizable(panel->is_expanded());

  // If this is a new panel or it was already focused (e.g. it was
  // focused when it got detached, and now it's being reattached),
  // call FocusPanel() to focus it if needed and update
  // 'desired_panel_to_focus_'.
  if (panel->is_expanded() &&
      (source == PANEL_SOURCE_NEW || panel->content_win()->focused())) {
    FocusPanel(panel, false, wm()->GetCurrentTimeFromServer());
  } else {
    panel->AddButtonGrab();
  }

  // If this is the only collapsed panel, we need to configure the input
  // window to watch for the pointer moving to the bottom of the screen.
  if (!panel->is_expanded() && GetNumCollapsedPanels() == 1)
    ConfigureShowCollapsedPanelsInputWindow(true);
}

void PanelBar::RemovePanel(Panel* panel) {
  DCHECK(panel);

  if (anchor_panel_ == panel)
    DestroyAnchor();
  if (dragged_panel_ == panel)
    dragged_panel_ = NULL;
  // If this was a focused content window, then let's try to find a nearby
  // panel to focus if we get asked to do so later.
  if (desired_panel_to_focus_ == panel)
    desired_panel_to_focus_ = GetNearestExpandedPanel(panel);

  bool was_collapsed = !panel->is_expanded();
  CHECK(panel_infos_.erase(panel) == 1);
  Panels::iterator it =
      FindPanelInVectorByWindow(panels_, *(panel->content_win()));
  if (it == panels_.end()) {
    LOG(WARNING) << "Got request to remove panel " << panel->xid_str()
                 << " but didn't find it in panels_";
    return;
  }

  total_panel_width_ -= ((*it)->width() + kPixelsBetweenPanels);
  panels_.erase(it);

  PackPanels(dragged_panel_);
  if (dragged_panel_)
    ReorderPanel(dragged_panel_);

  // If this was the last collapsed panel, move the input window offscreen.
  if (was_collapsed && GetNumCollapsedPanels() == 0)
    ConfigureShowCollapsedPanelsInputWindow(false);
}

bool PanelBar::ShouldAddDraggedPanel(const Panel* panel,
                                     int drag_x,
                                     int drag_y) {
  return drag_y + panel->total_height() >
         wm()->height() - kPanelAttachThresholdPixels;
}

void PanelBar::HandleInputWindowButtonPress(XWindow xid,
                                            int x, int y,
                                            int x_root, int y_root,
                                            int button,
                                            Time timestamp) {
  CHECK_EQ(xid, anchor_input_xid_);
  if (button != 1)
    return;

  // Destroy the anchor and collapse the corresponding panel.
  VLOG(1) << "Got button press in anchor window";
  Panel* panel = anchor_panel_;
  DestroyAnchor();
  if (panel)
    CollapsePanel(panel);
  else
    LOG(WARNING) << "Anchor panel no longer exists";
}

void PanelBar::HandleInputWindowPointerEnter(XWindow xid,
                                             int x, int y,
                                             int x_root, int y_root,
                                             Time timestamp) {
  if (xid == show_collapsed_panels_input_xid_) {
    VLOG(1) << "Got mouse enter in show-collapsed-panels window";
    if (x_root >= wm()->width() - total_panel_width_) {
      // If the user moves the pointer down quickly to the bottom of the
      // screen, it's possible that it could end up below a collapsed panel
      // without us having received an enter event in the panel's titlebar.
      // Show the panels immediately in this case.
      ShowCollapsedPanels();
    } else {
      // Otherwise, set up a timer to show the panels if we're not already
      // doing so.
      if (collapsed_panel_state_ != COLLAPSED_PANEL_STATE_SHOWN &&
          collapsed_panel_state_ != COLLAPSED_PANEL_STATE_WAITING_TO_SHOW) {
        collapsed_panel_state_ = COLLAPSED_PANEL_STATE_WAITING_TO_SHOW;
        DCHECK(show_collapsed_panels_timer_id_ == 0);
        show_collapsed_panels_timer_id_ =
            g_timeout_add(kShowCollapsedPanelsDelayMs,
                          &HandleShowCollapsedPanelsTimerThunk,
                          this);
      }
    }
  }
}

void PanelBar::HandleInputWindowPointerLeave(XWindow xid,
                                             int x, int y,
                                             int x_root, int y_root,
                                             Time timestamp) {
  if (xid == show_collapsed_panels_input_xid_) {
    VLOG(1) << "Got mouse leave in show-collapsed-panels window";
    if (collapsed_panel_state_ == COLLAPSED_PANEL_STATE_WAITING_TO_SHOW) {
      collapsed_panel_state_ = COLLAPSED_PANEL_STATE_HIDDEN;
      DisableShowCollapsedPanelsTimer();
    }
  }
}

void PanelBar::HandlePanelButtonPress(
    Panel* panel, int button, Time timestamp) {
  DCHECK(panel);
  VLOG(1) << "Got button press in panel " << panel->xid_str()
          << "; giving it the focus";
  // Get rid of the passive button grab, and then ungrab the pointer
  // and replay events so the panel will get a copy of the click.
  FocusPanel(panel, true, timestamp);  // remove_pointer_grab=true
}

void PanelBar::HandlePanelTitlebarPointerEnter(Panel* panel, Time timestamp) {
  DCHECK(panel);
  VLOG(1) << "Got pointer enter in panel " << panel->xid_str() << "'s titlebar";
  if (collapsed_panel_state_ != COLLAPSED_PANEL_STATE_SHOWN &&
      !panel->is_expanded()) {
    ShowCollapsedPanels();
  }
}

void PanelBar::HandlePanelFocusChange(Panel* panel, bool focus_in) {
  DCHECK(panel);
  if (!focus_in)
    panel->AddButtonGrab();
}

void PanelBar::HandleSetPanelStateMessage(Panel* panel, bool expand) {
  DCHECK(panel);
  if (expand)
    ExpandPanel(panel, true, kPanelStateAnimMs);
  else
    CollapsePanel(panel);
}

bool PanelBar::HandleNotifyPanelDraggedMessage(Panel* panel,
                                               int drag_x,
                                               int drag_y) {
  DCHECK(panel);

  VLOG(2) << "Notified about drag of panel " << panel->xid_str()
          << " to (" << drag_x << ", " << drag_y << ")";

  const int y_threshold =
      wm()->height() - panel->total_height() - kPanelDetachThresholdPixels;
  if (panel->is_expanded() && drag_y <= y_threshold)
    return false;

  if (dragged_panel_ != panel) {
    if (dragged_panel_) {
      LOG(WARNING) << "Abandoning dragged panel " << dragged_panel_->xid_str()
                   << " in favor of " << panel->xid_str();
      HandlePanelDragComplete(dragged_panel_);
    }

    VLOG(2) << "Starting drag of panel " << panel->xid_str();
    dragged_panel_ = panel;
    panel->StackAtTopOfLayer(StackingManager::LAYER_DRAGGED_PANEL);
  }

  dragged_panel_->MoveX((wm()->wm_ipc_version() >= 1) ?
                          drag_x :
                          drag_x + dragged_panel_->titlebar_width(),
                        false, 0);

  ReorderPanel(dragged_panel_);
  return true;
}

void PanelBar::HandleNotifyPanelDragCompleteMessage(Panel* panel) {
  DCHECK(panel);
  HandlePanelDragComplete(panel);
}

void PanelBar::HandleFocusPanelMessage(Panel* panel) {
  DCHECK(panel);
  if (!panel->is_expanded())
    ExpandPanel(panel, false, kPanelStateAnimMs);
  FocusPanel(panel, false, wm()->GetCurrentTimeFromServer());
}

void PanelBar::HandlePanelResize(Panel* panel) {
  DCHECK(panel);
  PackPanels(NULL);
}

void PanelBar::HandleScreenResize() {
  // Make all of the panels jump to their new Y positions first and then
  // repack them to animate them sliding to their new X positions.
  for (Panels::iterator it = panels_.begin(); it != panels_.end(); ++it) {
    Panel* panel = *it;
    const PanelInfo* info = GetPanelInfoOrDie(panel);
    (*it)->MoveY(ComputePanelY(*panel, *info), true, 0);
  }
  PackPanels(dragged_panel_);
  if (dragged_panel_)
    ReorderPanel(dragged_panel_);
}

void PanelBar::HandlePanelUrgencyChange(Panel* panel) {
  DCHECK(panel);
  const bool urgent = panel->content_win()->wm_hint_urgent();

  // Check that the hint has changed.
  PanelInfo* info = GetPanelInfoOrDie(panel);
  if (urgent == info->is_urgent)
    return;

  info->is_urgent = urgent;
  if (!panel->is_expanded()) {
    const int computed_y = ComputePanelY(*panel, *info);
    if (panel->titlebar_y() != computed_y)
      panel->MoveY(computed_y, true, kHideCollapsedPanelsAnimMs);
  }
}

bool PanelBar::TakeFocus() {
  Time timestamp = wm()->GetCurrentTimeFromServer();

  // If we already decided on a panel to focus, use it.
  if (desired_panel_to_focus_) {
    FocusPanel(desired_panel_to_focus_, false, timestamp);
    return true;
  }

  // Just focus the first expanded panel.
  for (Panels::iterator it = panels_.begin(); it != panels_.end(); ++it) {
    if ((*it)->is_expanded()) {
      FocusPanel(*it, false, timestamp);  // remove_pointer_grab=false
      return true;
    }
  }
  return false;
}


PanelBar::PanelInfo* PanelBar::GetPanelInfoOrDie(Panel* panel) {
  shared_ptr<PanelInfo> info =
      FindWithDefault(panel_infos_, panel, shared_ptr<PanelInfo>());
  CHECK(info.get());
  return info.get();
}

int PanelBar::GetNumCollapsedPanels() {
  int count = 0;
  for (Panels::const_iterator it = panels_.begin(); it != panels_.end(); ++it)
    if (!(*it)->is_expanded())
      count++;
  return count;
}

int PanelBar::ComputePanelY(const Panel& panel, const PanelInfo& info) {
  if (panel.is_expanded()) {
    return wm()->height() - panel.total_height();
  } else {
    if (CollapsedPanelsAreHidden() && !info.is_urgent)
      return wm()->height() - kHiddenCollapsedPanelHeightPixels;
    else
      return wm()->height() - panel.titlebar_height();
  }
}

void PanelBar::ExpandPanel(Panel* panel, bool create_anchor, int anim_ms) {
  CHECK(panel);
  if (panel->is_expanded()) {
    LOG(WARNING) << "Ignoring request to expand already-expanded panel "
                 << panel->xid_str();
    return;
  }

  panel->SetExpandedState(true);
  const PanelInfo* info = GetPanelInfoOrDie(panel);
  panel->MoveY(ComputePanelY(*panel, *info), true, anim_ms);
  panel->SetResizable(true);
  if (create_anchor)
    CreateAnchor(panel);

  if (GetNumCollapsedPanels() == 0)
    ConfigureShowCollapsedPanelsInputWindow(false);
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

  if (anchor_panel_ == panel)
    DestroyAnchor();

  panel->SetExpandedState(false);
  const PanelInfo* info = GetPanelInfoOrDie(panel);
  panel->MoveY(ComputePanelY(*panel, *info), true, kPanelStateAnimMs);
  panel->SetResizable(false);

  // Give up the focus if this panel had it.
  if (panel->content_win()->focused()) {
    desired_panel_to_focus_ = panel_to_focus;
    if (!TakeFocus()) {
      wm()->SetActiveWindowProperty(None);
      wm()->TakeFocus();
    }
  }

  if (GetNumCollapsedPanels() == 1)
    ConfigureShowCollapsedPanelsInputWindow(true);
}

void PanelBar::FocusPanel(Panel* panel,
                          bool remove_pointer_grab,
                          Time timestamp) {
  CHECK(panel);
  panel->RemoveButtonGrab(true);  // remove_pointer_grab
  wm()->SetActiveWindowProperty(panel->content_win()->xid());
  panel->content_win()->TakeFocus(timestamp);
  desired_panel_to_focus_ = panel;
}

Panel* PanelBar::GetPanelByWindow(const Window& win) {
  Panels::iterator it = FindPanelInVectorByWindow(panels_, win);
  return (it != panels_.end()) ? *it : NULL;
}

// static
PanelBar::Panels::iterator PanelBar::FindPanelInVectorByWindow(
    Panels& panels, const Window& win) {
  for (Panels::iterator it = panels.begin(); it != panels.end(); ++it)
    if ((*it)->titlebar_win() == &win || (*it)->content_win() == &win)
      return it;
  return panels.end();
}

void PanelBar::HandlePanelDragComplete(Panel* panel) {
  CHECK(panel);
  VLOG(2) << "Got notification that panel drag is complete for "
          << panel->xid_str();
  if (dragged_panel_ != panel)
    return;

  panel->StackAtTopOfLayer(StackingManager::LAYER_STATIONARY_PANEL_IN_BAR);
  panel->MoveX(GetPanelInfoOrDie(panel)->snapped_right,
               true, kPanelArrangeAnimMs);
  dragged_panel_ = NULL;

  if (collapsed_panel_state_ == COLLAPSED_PANEL_STATE_WAITING_TO_HIDE) {
    // If the user moved the pointer up from the bottom of the screen while
    // they were dragging the panel...
    int pointer_y = 0;
    wm()->xconn()->QueryPointerPosition(NULL, &pointer_y);
    if (pointer_y < wm()->height() - kHideCollapsedPanelsDistancePixels) {
      // Hide the panels if they didn't move the pointer back down again
      // before releasing the button.
      HideCollapsedPanels();
    } else {
      // Otherwise, keep showing the panels and start watching the pointer
      // position again.
      collapsed_panel_state_ = COLLAPSED_PANEL_STATE_SHOWN;
      StartHideCollapsedPanelsWatcher();
    }
  }
}

void PanelBar::ReorderPanel(Panel* fixed_panel) {
  DCHECK(fixed_panel);

  Panels::iterator src_it = find(panels_.begin(), panels_.end(), fixed_panel);
  DCHECK(src_it != panels_.end());
  const int src_position = src_it - panels_.begin();

  int dest_position = src_position;
  if (fixed_panel->right() < GetPanelInfoOrDie(fixed_panel)->snapped_right) {
    // If we're to the left of our snapped position, look for the furthest
    // panel whose midpoint has been passed by our left edge.
    for (int i = src_position - 1; i >= 0; --i) {
      Panel* panel = panels_[i];
      if (fixed_panel->content_x() <= panel->content_center())
        dest_position = i;
      else
        break;
    }
  } else {
    // Otherwise, do the same check with our right edge to the right.
    for (int i = src_position + 1; i < static_cast<int>(panels_.size()); ++i) {
      Panel* panel = panels_[i];
      if (fixed_panel->right() >= panel->content_center())
        dest_position = i;
      else
        break;
    }
  }

  if (dest_position != src_position) {
    Panels::iterator dest_it = panels_.begin() + dest_position;
    if (dest_it > src_it)
      rotate(src_it, src_it + 1, dest_it + 1);
    else
      rotate(dest_it, src_it, src_it + 1);
    PackPanels(fixed_panel);
  }
}

void PanelBar::PackPanels(Panel* fixed_panel) {
  total_panel_width_ = 0;

  for (Panels::reverse_iterator it = panels_.rbegin();
       it != panels_.rend(); ++it) {
    Panel* panel = *it;
    PanelInfo* info = GetPanelInfoOrDie(panel);

    info->snapped_right =
        wm()->width() - total_panel_width_ - kPixelsBetweenPanels;
    if (panel != fixed_panel && panel->right() != info->snapped_right)
      panel->MoveX(info->snapped_right, true, kPanelArrangeAnimMs);

    total_panel_width_ += panel->width() + kPixelsBetweenPanels;
  }
}

void PanelBar::CreateAnchor(Panel* panel) {
  int pointer_x = 0;
  wm()->xconn()->QueryPointerPosition(&pointer_x, NULL);

  const int width = anchor_actor_->GetWidth();
  const int height = anchor_actor_->GetHeight();
  const int x = min(max(static_cast<int>(pointer_x - 0.5 * width), 0),
                    wm()->width() - width);
  const int y = wm()->height() - height;

  wm()->ConfigureInputWindow(anchor_input_xid_, x, y, width, height);
  anchor_panel_ = panel;
  anchor_actor_->Move(x, y, 0);
  anchor_actor_->SetOpacity(1, kAnchorFadeAnimMs);

  // We might not get a LeaveNotify event*, so we also poll the pointer
  // position.

  // * If the mouse cursor has already been moved away before the anchor
  // input window gets created, the anchor never gets a mouse leave event.
  // Additionally, Chrome appears to be stacking its status bubble window
  // above all other windows, so we sometimes get a leave event as soon as
  // we slide a panel up.
  anchor_pointer_watcher_.reset(
      new PointerPositionWatcher(
          wm()->xconn(),
          NewPermanentCallback(this, &PanelBar::DestroyAnchor),
          false,  // watch_for_entering_target=false
          x, y, width, height));
}

void PanelBar::DestroyAnchor() {
  wm()->xconn()->ConfigureWindowOffscreen(anchor_input_xid_);
  anchor_actor_->SetOpacity(0, kAnchorFadeAnimMs);
  anchor_panel_ = NULL;
  anchor_pointer_watcher_.reset();
}

Panel* PanelBar::GetNearestExpandedPanel(Panel* panel) {
  if (!panel || !panel->is_expanded())
    return NULL;

  Panel* nearest_panel = NULL;
  int best_distance = kint32max;
  for (Panels::iterator it = panels_.begin(); it != panels_.end(); ++it) {
    if (*it == panel || !(*it)->is_expanded())
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
      nearest_panel = *it;
    }
  }
  return nearest_panel;
}

void PanelBar::ConfigureShowCollapsedPanelsInputWindow(bool move_onscreen) {
  VLOG(1) << (move_onscreen ? "Showing" : "Hiding") << " input window "
          << XidStr(show_collapsed_panels_input_xid_)
          << " for showing collapsed panels";
  if (move_onscreen) {
    wm()->ConfigureInputWindow(
        show_collapsed_panels_input_xid_,
        0, wm()->height() - kShowCollapsedPanelsDistancePixels,
        wm()->width(), kShowCollapsedPanelsDistancePixels);
  } else {
    wm()->xconn()->ConfigureWindowOffscreen(show_collapsed_panels_input_xid_);
  }
}

void PanelBar::StartHideCollapsedPanelsWatcher() {
  hide_collapsed_panels_pointer_watcher_.reset(
      new PointerPositionWatcher(
          wm()->xconn(),
          NewPermanentCallback(this, &PanelBar::HideCollapsedPanels),
          false,  // watch_for_entering_target=false
          0, wm()->height() - kHideCollapsedPanelsDistancePixels,
          wm()->width(), kHideCollapsedPanelsDistancePixels));
}

void PanelBar::ShowCollapsedPanels() {
  VLOG(1) << "Showing collapsed panels";
  DisableShowCollapsedPanelsTimer();
  collapsed_panel_state_ = COLLAPSED_PANEL_STATE_SHOWN;

  for (Panels::iterator it = panels_.begin(); it != panels_.end(); ++it) {
    Panel* panel = *it;
    if (panel->is_expanded())
      continue;
    const PanelInfo* info = GetPanelInfoOrDie(panel);
    const int computed_y = ComputePanelY(*panel, *info);
    if (panel->titlebar_y() != computed_y)
      panel->MoveY(computed_y, true, kHideCollapsedPanelsAnimMs);
  }

  ConfigureShowCollapsedPanelsInputWindow(false);
  StartHideCollapsedPanelsWatcher();
}

void PanelBar::HideCollapsedPanels() {
  VLOG(1) << "Hiding collapsed panels";
  DisableShowCollapsedPanelsTimer();

  if (dragged_panel_ && !dragged_panel_->is_expanded()) {
    // Don't hide the panels in the middle of the drag -- we'll do it in
    // HandlePanelDragComplete() instead.
    VLOG(1) << "Deferring hiding collapsed panels since collapsed panel "
            << dragged_panel_->xid_str() << " is currently being dragged";
    collapsed_panel_state_ = COLLAPSED_PANEL_STATE_WAITING_TO_HIDE;
    return;
  }

  collapsed_panel_state_ = COLLAPSED_PANEL_STATE_HIDDEN;
  for (Panels::iterator it = panels_.begin(); it != panels_.end(); ++it) {
    Panel* panel = *it;
    if (panel->is_expanded())
      continue;
    const PanelInfo* info = GetPanelInfoOrDie(panel);
    const int computed_y = ComputePanelY(*panel, *info);
    if (panel->titlebar_y() != computed_y)
      panel->MoveY(computed_y, true, kHideCollapsedPanelsAnimMs);
  }

  if (GetNumCollapsedPanels() > 0)
    ConfigureShowCollapsedPanelsInputWindow(true);
  hide_collapsed_panels_pointer_watcher_.reset();
}

void PanelBar::DisableShowCollapsedPanelsTimer() {
  if (show_collapsed_panels_timer_id_) {
    g_source_remove(show_collapsed_panels_timer_id_);
    show_collapsed_panels_timer_id_ = 0;
  }
}

gboolean PanelBar::HandleShowCollapsedPanelsTimer() {
  DCHECK_EQ(collapsed_panel_state_, COLLAPSED_PANEL_STATE_WAITING_TO_SHOW);
  // Clear the ID so that ShowCollapsedPanels() won't try to delete the
  // timer for us -- it'll be deleted automatically when we return FALSE.
  show_collapsed_panels_timer_id_ = 0;
  ShowCollapsedPanels();
  return FALSE;
}

}  // namespace window_manager
