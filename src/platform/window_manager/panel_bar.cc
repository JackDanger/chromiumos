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

// Amount of time to take when arranging panels.
static const int kPanelArrangeAnimMs = 150;

// Amount of time to take when fading the panel anchor in or out.
static const int kAnchorFadeAnimMs = 150;

// Amount of time to take when making the panel bar slide up or down.
static const int kBarSlideAnimMs = 150;

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
      anchor_input_xid_(
          wm_->CreateInputWindow(-1, -1, 1, 1,
                                 ButtonPressMask | LeaveWindowMask)),
      anchor_panel_(NULL),
      anchor_actor_(wm_->clutter()->CreateImage(FLAGS_panel_anchor_image)),
      desired_panel_to_focus_(NULL),
      is_visible_(false) {
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
  wm_->xconn()->DestroyWindow(anchor_input_xid_);
}

void PanelBar::GetInputWindows(vector<XWindow>* windows_out) {
  CHECK(windows_out);
  windows_out->clear();
  windows_out->push_back(anchor_input_xid_);
}

void PanelBar::AddPanel(Panel* panel, PanelSource source, bool expanded) {
  DCHECK(panel);

  // If this is a newly-created panel, determine the offscreen position
  // from which we want it to animate in.  (Dragged panels should just get
  // animated from wherever they were before.)
  if (source == PANEL_SOURCE_NEW) {
    const int initial_right = expanded ?
        x_ + width_ - kBarPadding :
        x_ + width_ - collapsed_panel_width_ - kBarPadding;
    const int initial_y = y_ + height_;
    panel->Move(initial_right, initial_y, true, 0);
  }

  vector<XWindow> input_windows;
  panel->GetInputWindows(&input_windows);
  for (vector<XWindow>::const_iterator it = input_windows.begin();
       it != input_windows.end(); ++it) {
    CHECK(panel_input_windows_.insert(make_pair(*it, panel)).second);
  }

  shared_ptr<PanelInfo> info(new PanelInfo);
  info->is_expanded = false;
  info->snapped_right = panel->right();
  panel_infos_.insert(make_pair(panel, info));

  if (!expanded) {
    ConfigureCollapsedPanel(panel);
    collapsed_panels_.insert(collapsed_panels_.begin(), panel);
    collapsed_panel_width_ += panel->titlebar_width() + kBarPadding;
  } else {
    panel->StackAtTopOfLayer(
        (source == PANEL_SOURCE_DRAGGED) ?
        StackingManager::LAYER_DRAGGED_EXPANDED_PANEL :
        StackingManager::LAYER_EXPANDED_PANEL);
    collapsed_panels_.push_back(panel);
    bool reposition_other_panels = (source != PANEL_SOURCE_DRAGGED);
    int anim_ms = 0;
    switch (source) {
      case PANEL_SOURCE_NEW: anim_ms = kPanelStateAnimMs; break;
      case PANEL_SOURCE_DRAGGED: anim_ms = 0; break;
      case PANEL_SOURCE_DROPPED: anim_ms = kDroppedPanelAnimMs; break;
      default: NOTREACHED() << "Unknown panel source " << source;
    }
    ExpandPanel(panel, false, reposition_other_panels, anim_ms);
  }

  // If this is a new panel or it was already focused (e.g. it was
  // focused when it got detached, and now it's being reattached),
  // call FocusPanel() to focus it if needed and update
  // 'desired_panel_to_focus_'.
  if (expanded &&
      (source == PANEL_SOURCE_NEW || panel->content_win()->focused())) {
    FocusPanel(panel, false, wm_->GetCurrentTimeFromServer());
  } else {
    panel->AddButtonGrab();
  }

  if (num_panels() > 0 && !is_visible_)
    SetVisibility(true);
}

void PanelBar::RemovePanel(Panel* panel) {
  DCHECK(panel);

  vector<XWindow> input_windows;
  panel->GetInputWindows(&input_windows);
  for (vector<XWindow>::const_iterator it = input_windows.begin();
       it != input_windows.end(); ++it) {
    CHECK(panel_input_windows_.erase(*it) == 1);
  }

  if (anchor_panel_ == panel)
    DestroyAnchor();
  if (dragged_panel_ == panel)
    dragged_panel_ = NULL;
  // If this was a focused content window, then let's try to find a nearby
  // panel to focus if we get asked to do so later.
  if (desired_panel_to_focus_ == panel)
    desired_panel_to_focus_ = GetNearestExpandedPanel(panel);

  CHECK(panel_infos_.erase(panel) == 1);
  Panels::iterator it = FindPanelInVectorByWindow(
      collapsed_panels_, *(panel->content_win()));
  if (it != collapsed_panels_.end()) {
    collapsed_panel_width_ -= ((*it)->titlebar_width() + kBarPadding);
    collapsed_panels_.erase(it);
    PackCollapsedPanels();
  } else {
    it = FindPanelInVectorByWindow(expanded_panels_, *(panel->content_win()));
    if (it != expanded_panels_.end()) {
      expanded_panels_.erase(it);
    } else {
      LOG(WARNING) << "Got request to remove panel " << panel->xid_str()
                   << " but didn't find it in collapsed_panels_ or "
                   << "expanded_panels_";
    }
  }

  if (num_panels() == 0 && is_visible_)
    SetVisibility(false);
}

bool PanelBar::ShouldAddDraggedPanel(const Panel* panel,
                                     int drag_x,
                                     int drag_y) {
  return drag_y + panel->total_height() > y_ - kPanelAttachThresholdPixels;
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

void PanelBar::HandleInputWindowPointerLeave(XWindow xid, Time timestamp) {
  CHECK_EQ(xid, anchor_input_xid_);

  // TODO: There appears to be a bit of a race condition here.  If the
  // mouse cursor has already been moved away before the anchor input
  // window gets created, the anchor never gets a mouse leave event.  Find
  // some way to work around this.
  VLOG(1) << "Got mouse leave in anchor window";
  DestroyAnchor();
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

void PanelBar::HandlePanelFocusChange(Panel* panel, bool focus_in) {
  DCHECK(panel);
  if (!focus_in)
    panel->AddButtonGrab();
}

void PanelBar::HandleSetPanelStateMessage(Panel* panel, bool expand) {
  DCHECK(panel);
  if (expand)
    ExpandPanel(panel, true, true, kPanelStateAnimMs);
  else
    CollapsePanel(panel);
}

bool PanelBar::HandleNotifyPanelDraggedMessage(Panel* panel,
                                               int drag_x,
                                               int drag_y) {
  DCHECK(panel);

  VLOG(2) << "Notified about drag of panel " << panel->xid_str()
          << " to (" << drag_x << ", " << drag_y << ")";

  PanelInfo* info = GetPanelInfoOrDie(panel);
  if (info->is_expanded &&
      drag_y <= y_ - panel->total_height() - kPanelDetachThresholdPixels) {
    return false;
  }

  if (dragged_panel_ != panel) {
    if (dragged_panel_) {
      LOG(WARNING) << "Abandoning dragged panel " << dragged_panel_->xid_str()
                   << " in favor of " << panel->xid_str();
      HandlePanelDragComplete(dragged_panel_);
    }

    VLOG(2) << "Starting drag of panel " << panel->xid_str();
    dragged_panel_ = panel;
    panel->StackAtTopOfLayer(
        GetPanelInfoOrDie(panel)->is_expanded ?
          StackingManager::LAYER_DRAGGED_EXPANDED_PANEL :
          StackingManager::LAYER_DRAGGED_COLLAPSED_PANEL);
  }

  dragged_panel_->MoveX((wm_->wm_ipc_version() >= 1) ?
                          drag_x :
                          drag_x + dragged_panel_->titlebar_width(),
                        false, 0);

  // When an expanded panel is being dragged, we don't move the other
  // panels to make room for it until the drag is done.
  if (info->is_expanded)
    return true;

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
    if (*it == dragged_panel_) {
      // If we're comparing against ourselves, use our original position
      // rather than wherever we've currently been dragged by the user.
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
  if (it != collapsed_panels_.end() && *it != dragged_panel_) {
    if (it > dragged_it)
      rotate(dragged_it, dragged_it + 1, it + 1);
    else
      rotate(it, dragged_it, dragged_it + 1);
    PackCollapsedPanels();
  }

  return true;
}

void PanelBar::HandleNotifyPanelDragCompleteMessage(Panel* panel) {
  DCHECK(panel);
  HandlePanelDragComplete(panel);
}

void PanelBar::HandleFocusPanelMessage(Panel* panel) {
  DCHECK(panel);
  if (!GetPanelInfoOrDie(panel)->is_expanded)
    ExpandPanel(panel, false, true, kPanelStateAnimMs);
  FocusPanel(panel, false, wm_->GetCurrentTimeFromServer());
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
    (*it)->MoveY(y_ - (*it)->total_height(), true, 0);
  }
  for (Panels::iterator it = collapsed_panels_.begin();
       it != collapsed_panels_.end(); ++it) {
    (*it)->MoveY(y_ + height_ - (*it)->titlebar_height(), true, 0);
  }

  // ... and their X positions.
  PackCollapsedPanels();
  if (!expanded_panels_.empty()) {
    // Arbitrarily move the first panel onscreen, if it isn't already, and
    // then try to arrange all of the other panels to not overlap.
    Panel* fixed_panel = expanded_panels_[0];
    MoveExpandedPanelOnscreen(fixed_panel, kPanelArrangeAnimMs);
    RepositionExpandedPanels(fixed_panel);
  }
}

bool PanelBar::TakeFocus() {
  Time timestamp = wm_->GetCurrentTimeFromServer();

  // If we already decided on a panel to focus, use it.
  if (desired_panel_to_focus_) {
    FocusPanel(desired_panel_to_focus_, false, timestamp);
    return true;
  }

  // Just focus the first expanded panel.
  if (expanded_panels_.empty()) {
    return false;
  }
  FocusPanel(expanded_panels_[0], false, timestamp);
  return true;
}


PanelBar::PanelInfo* PanelBar::GetPanelInfoOrDie(Panel* panel) {
  shared_ptr<PanelInfo> info =
      FindWithDefault(panel_infos_, panel, shared_ptr<PanelInfo>());
  CHECK(info.get());
  return info.get();
}

void PanelBar::ExpandPanel(Panel* panel,
                           bool create_anchor,
                           bool reposition_other_panels,
                           int anim_ms) {
  CHECK(panel);
  PanelInfo* info = GetPanelInfoOrDie(panel);
  if (info->is_expanded) {
    LOG(WARNING) << "Ignoring request to expand already-expanded panel "
                 << panel->xid_str();
    return;
  }

  panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
  panel->SetTitlebarWidth(panel->content_width());
  panel->MoveY(y_ - panel->total_height(), true, anim_ms);
  panel->SetResizable(true);
  panel->SetContentShadowOpacity(1.0, anim_ms);
  panel->NotifyChromeAboutState(true);
  info->is_expanded = true;

  Panels::iterator it =
      FindPanelInVectorByWindow(collapsed_panels_, *(panel->content_win()));
  CHECK(it != collapsed_panels_.end());
  collapsed_panels_.erase(it);
  InsertExpandedPanel(panel);
  if (reposition_other_panels)
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
  expanded_panels_.erase(it);
  InsertCollapsedPanel(panel);
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

void PanelBar::FocusPanel(Panel* panel,
                          bool remove_pointer_grab,
                          Time timestamp) {
  CHECK(panel);
  panel->RemoveButtonGrab(true);  // remove_pointer_grab
  wm_->SetActiveWindowProperty(panel->content_win()->xid());
  panel->content_win()->TakeFocus(timestamp);
  panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
  desired_panel_to_focus_ = panel;
}

Panel* PanelBar::GetPanelByWindow(const Window& win) {
  Panels::iterator it = FindPanelInVectorByWindow(collapsed_panels_, win);
  if (it != collapsed_panels_.end())
    return *it;

  it = FindPanelInVectorByWindow(expanded_panels_, win);
  if (it != expanded_panels_.end())
    return *it;

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

void PanelBar::HandlePanelDragComplete(Panel* panel) {
  CHECK(panel);
  VLOG(2) << "Got notification that panel drag is complete for "
          << panel->xid_str();

  if (dragged_panel_ != panel)
    return;

  PanelInfo* info = GetPanelInfoOrDie(panel);
  if (info->is_expanded) {
    panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
    // Tell the panel to move its client windows to match its composited
    // position.
    panel->MoveX(panel->right(), true, 0);
    RepositionExpandedPanels(panel);
  } else {
    panel->StackAtTopOfLayer(StackingManager::LAYER_COLLAPSED_PANEL);
    // Snap collapsed dragged panels to their correct position.
    panel->MoveX(info->snapped_right, true, kPanelArrangeAnimMs);
  }
  dragged_panel_ = NULL;
}

void PanelBar::PackCollapsedPanels() {
  collapsed_panel_width_ = 0;

  for (Panels::reverse_iterator it = collapsed_panels_.rbegin();
       it != collapsed_panels_.rend(); ++it) {
    Panel* panel = *it;
    // TODO: PackCollapsedPanels() gets called in response to every move
    // message that we receive about a dragged panel.  Check that it's not
    // too inefficient to do all of these lookups vs. storing the panel
    // info alongside the Panel pointer in 'collapsed_panels_'.
    PanelInfo* info = GetPanelInfoOrDie(panel);

    info->snapped_right = x_ + width_ - collapsed_panel_width_ - kBarPadding;
    if (panel != dragged_panel_ && panel->right() != info->snapped_right)
      panel->MoveX(info->snapped_right, true, kPanelArrangeAnimMs);

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
    Panel* panel = expanded_panels_[i];
    if (center_x <= panel->content_center() ||
        i == expanded_panels_.size() - 1) {
      if (panel != fixed_panel) {
        // If it has, then we reorder the panels.
        expanded_panels_.erase(expanded_panels_.begin() + fixed_index);
        if (i < expanded_panels_.size()) {
          expanded_panels_.insert(expanded_panels_.begin() + i, fixed_panel);
        } else {
          expanded_panels_.push_back(fixed_panel);
        }
      }
      break;
    }
  }

  // Find the total width of the panels to the left of the fixed panel.
  int total_width = 0;
  fixed_index = -1;
  for (int i = 0; i < static_cast<int>(expanded_panels_.size()); ++i) {
    Panel* panel = expanded_panels_[i];
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
  int avail_width = max(fixed_panel->content_x() - kBarPadding - x_, 0);
  while (total_width > avail_width) {
    new_fixed_index--;
    CHECK(new_fixed_index >= 0);
    total_width -= expanded_panels_[new_fixed_index]->content_width();
  }

  // Reorder the fixed panel if its index changed.
  if (new_fixed_index != fixed_index) {
    Panels::iterator it = expanded_panels_.begin() + fixed_index;
    CHECK(*it == fixed_panel);
    expanded_panels_.erase(it);
    expanded_panels_.insert(expanded_panels_.begin() + new_fixed_index,
                            fixed_panel);
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
    CHECK(*it == fixed_panel);
    expanded_panels_.erase(it);
    expanded_panels_.insert(expanded_panels_.begin() + new_fixed_index,
                            fixed_panel);
    fixed_index = new_fixed_index;
  }

  // Finally, push panels to the left and the right so they don't overlap.
  int boundary = expanded_panels_[fixed_index]->content_x() - kBarPadding;
  for (Panels::reverse_iterator it =
         // Start at the panel to the left of 'new_fixed_index'.
         expanded_panels_.rbegin() +
         (expanded_panels_.size() - new_fixed_index);
       it != expanded_panels_.rend(); ++it) {
    Panel* panel = *it;
    if (panel->right() > boundary) {
      panel->MoveX(boundary, true, kPanelArrangeAnimMs);
    } else if (panel->content_x() < x_) {
      panel->MoveX(
          min(boundary, x_ + panel->content_width() + kBarPadding),
          true, kPanelArrangeAnimMs);
    }
    boundary = panel->content_x() - kBarPadding;
  }

  boundary = expanded_panels_[fixed_index]->right() + kBarPadding;
  for (Panels::iterator it = expanded_panels_.begin() + new_fixed_index + 1;
       it != expanded_panels_.end(); ++it) {
    Panel* panel = *it;
    if (panel->content_x() < boundary) {
      panel->MoveX(boundary + panel->content_width(),
                   true,
                   kPanelArrangeAnimMs);
    } else if (panel->right() > x_ + width_) {
      panel->MoveX(max(boundary + panel->content_width(), width_ - kBarPadding),
                   true, kPanelArrangeAnimMs);
    }
    boundary = panel->right() + kBarPadding;
  }
}

void PanelBar::InsertCollapsedPanel(Panel* new_panel) {
  size_t index = 0;
  for (; index < collapsed_panels_.size(); ++index) {
    Panel* panel = collapsed_panels_[index];
    if (new_panel->titlebar_x() < panel->titlebar_x()) {
      break;
    }
  }
  collapsed_panels_.insert(collapsed_panels_.begin() + index, new_panel);
}

void PanelBar::InsertExpandedPanel(Panel* new_panel) {
  size_t index = 0;
  for (; index < expanded_panels_.size(); ++index) {
    Panel* panel = expanded_panels_[index];
    if (new_panel->content_x() < panel->content_x()) {
      break;
    }
  }
  expanded_panels_.insert(expanded_panels_.begin() + index, new_panel);
}

void PanelBar::CreateAnchor(Panel* panel) {
  wm_->ConfigureInputWindow(
      anchor_input_xid_,
      panel->titlebar_x(), y_,
      panel->titlebar_width(), height_);
  anchor_panel_ = panel;

  anchor_actor_->Move(
      panel->titlebar_x() +
        0.5 * (panel->titlebar_width() - anchor_actor_->GetWidth()),
      y_ + 0.5 * (height_ - anchor_actor_->GetHeight()),
      0);  // anim_ms
  anchor_actor_->SetOpacity(1, kAnchorFadeAnimMs);
}

void PanelBar::DestroyAnchor() {
  wm_->ConfigureInputWindow(anchor_input_xid_, -1, -1, 1, 1);
  anchor_actor_->SetOpacity(0, kAnchorFadeAnimMs);
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
    if (*it == panel)
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

void PanelBar::SetVisibility(bool visible) {
  if (is_visible_ == visible) {
    return;
  }

  if (visible) {
    bar_actor_->MoveY(y_, kBarSlideAnimMs);
    bar_shadow_->MoveY(y_, kBarSlideAnimMs);
    bar_shadow_->SetOpacity(1, kBarSlideAnimMs);
  } else {
    bar_actor_->MoveY(y_ + height_, kBarSlideAnimMs);
    bar_shadow_->MoveY(y_ + height_, kBarSlideAnimMs);
    bar_shadow_->SetOpacity(0, kBarSlideAnimMs);
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
