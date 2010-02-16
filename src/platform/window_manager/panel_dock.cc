// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/panel_dock.h"

#include <algorithm>

#include <gflags/gflags.h>

#include "window_manager/panel.h"
#include "window_manager/panel_manager.h"
#include "window_manager/shadow.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_string(panel_dock_background_image,
              "../assets/images/panel_dock_bg.png",
              "Image to use for panel dock backgrounds");

namespace window_manager {

using std::find;
using std::vector;

// Distance between the panel and the edge of the screen at which we detach it.
static const int kDetachThresholdPixels = 50;

// Distance between the panel and the edge of the screen at which we attach it.
static const int kAttachThresholdPixels = 20;

// Amount of time to take for sliding the dock background in or out when
// the dock is shown or hidden.
// TODO: This animation looks janky (there's a brief flash where the WM
// background image is visible), so we disable it for now.
static const int kBackgroundAnimMs = 0;

// Amount of time to take when fading a panel's shadow in or out as it's
// detached or attached.
static const int kPanelShadowAnimMs = 150;

// Amount of time to take when packing panels into the dock.
static const int kPackPanelsAnimMs = 150;

PanelDock::PanelDock(PanelManager* panel_manager, DockType type, int width)
    : panel_manager_(panel_manager),
      type_(type),
      x_(type == DOCK_TYPE_LEFT ? 0 : wm()->width() - width),
      y_(0),
      width_(width),
      height_(wm()->height()),
      dragged_panel_(NULL),
      bg_actor_(wm()->clutter()->CreateImage(
                    FLAGS_panel_dock_background_image)),
      bg_shadow_(new Shadow(wm()->clutter())),
      bg_input_xid_(wm()->CreateInputWindow(
                        -1, -1, 1, 1, ButtonPressMask|ButtonReleaseMask)) {
  wm()->stacking_manager()->StackXidAtTopOfLayer(
      bg_input_xid_, StackingManager::LAYER_PANEL_DOCK);

  const int bg_x = (type == DOCK_TYPE_LEFT) ? x_ - width_ : x_ + width_;
  bg_shadow_->group()->SetName("panel dock background shadow");
  wm()->stage()->AddActor(bg_shadow_->group());
  bg_shadow_->Resize(width_, height_, 0);
  bg_shadow_->Move(bg_x, y_, 0);
  bg_shadow_->SetOpacity(0, 0);
  bg_shadow_->Show();
  wm()->stacking_manager()->StackActorAtTopOfLayer(
      bg_shadow_->group(), StackingManager::LAYER_PANEL_DOCK);

  bg_actor_->SetName("panel dock background");
  wm()->stage()->AddActor(bg_actor_.get());
  bg_actor_->SetSize(width_, height_);
  bg_actor_->Move(bg_x, y_, 0);
  bg_actor_->SetVisibility(true);
  wm()->stacking_manager()->StackActorAtTopOfLayer(
      bg_actor_.get(), StackingManager::LAYER_PANEL_DOCK);
}

PanelDock::~PanelDock() {
  wm()->xconn()->DestroyWindow(bg_input_xid_);
  dragged_panel_ = NULL;
}

void PanelDock::GetInputWindows(std::vector<XWindow>* windows_out) {
  DCHECK(windows_out);
  windows_out->clear();
  windows_out->push_back(bg_input_xid_);
}

void PanelDock::AddPanel(Panel* panel, PanelSource source) {
  DCHECK(find(panels_.begin(), panels_.end(), panel) == panels_.end());
  panels_.push_back(panel);
  if (panels_.size() == static_cast<size_t>(1)) {
    wm()->ConfigureInputWindow(bg_input_xid_, x_, y_, width_, height_);
    bg_actor_->MoveX(x_, kBackgroundAnimMs);
    bg_shadow_->MoveX(x_, kBackgroundAnimMs);
    bg_shadow_->SetOpacity(1, kBackgroundAnimMs);
    panel_manager_->HandleDockVisibilityChange(this);
  }

  panel->StackAtTopOfLayer(
      source == PANEL_SOURCE_DRAGGED ?
        StackingManager::LAYER_DRAGGED_PANEL :
        StackingManager::LAYER_STATIONARY_PANEL_IN_DOCK);

  // Try to make the panel fit vertically within our dimensions.
  int panel_y = panel->titlebar_y();
  if (panel_y + panel->total_height() > y_ + height_)
    panel_y = y_ + height_ - panel->total_height();
  if (panel_y < y_)
    panel_y = y_;
  panel->Move(type_ == DOCK_TYPE_RIGHT ?  x_ + width_ : x_ + panel->width(),
              panel_y, true, 0);
  // TODO: Ideally, we would resize the panel here to match our width, but
  // that messes up the subsequent notification messages about the panel
  // being dragged -- some of them will be with regard to the panel's old
  // dimensions and others will be with regard to the new dimensions.
  // Instead, we defer resizing the panel until the drag is complete.

  if (panel->content_win()->focused()) {
    FocusPanel(panel, false, wm()->GetCurrentTimeFromServer());
  } else {
    panel->AddButtonGrab();
  }
}

void PanelDock::RemovePanel(Panel* panel) {
  if (dragged_panel_ == panel)
    dragged_panel_ = NULL;

  vector<Panel*>::iterator it = find(panels_.begin(), panels_.end(), panel);
  DCHECK(it != panels_.end());
  const int panel_pos = it - panels_.begin();
  panels_.erase(it);

  if (panels_.empty()) {
    const int bg_x = type_ == DOCK_TYPE_LEFT ? x_ - width_ : x_ + width_;
    wm()->xconn()->ConfigureWindowOffscreen(bg_input_xid_);
    bg_actor_->MoveX(bg_x, kBackgroundAnimMs);
    bg_shadow_->MoveX(bg_x, kBackgroundAnimMs);
    bg_shadow_->SetOpacity(0, kBackgroundAnimMs);
    panel_manager_->HandleDockVisibilityChange(this);
  } else {
    Panel* next_panel = (panel_pos < static_cast<int>(panels_.size())) ?
                        *(panels_.begin() + panel_pos) :
                        NULL;
    PackPanels(next_panel);
  }
}

bool PanelDock::ShouldAddDraggedPanel(const Panel* panel,
                                      int drag_x,
                                      int drag_y) {
  return (type_ == DOCK_TYPE_RIGHT) ?
         drag_x >= x_ + width_ - kAttachThresholdPixels :
         drag_x - panel->content_width() <= x_ + kAttachThresholdPixels;
}

void PanelDock::HandlePanelButtonPress(Panel* panel,
                                       int button,
                                       Time timestamp) {
  FocusPanel(panel, true, timestamp);
}

void PanelDock::HandlePanelFocusChange(Panel* panel, bool focus_in) {
  if (!focus_in)
    panel->AddButtonGrab();
}

void PanelDock::HandleSetPanelStateMessage(Panel* panel, bool expand) {
  VLOG(1) << "Got request to " << (expand ? "expand" : "collapse")
          << " panel " << panel->xid_str();
  if (expand)
    ExpandPanel(panel);
  else
    CollapsePanel(panel);
}

bool PanelDock::HandleNotifyPanelDraggedMessage(Panel* panel,
                                                int drag_x,
                                                int drag_y) {
  if (type_ == DOCK_TYPE_RIGHT) {
    if (drag_x <= x_ + width_ - kDetachThresholdPixels)
      return false;
  } else {
    if (drag_x - panel->content_width() >= x_ + kDetachThresholdPixels)
      return false;
  }

  if (dragged_panel_ != panel) {
    dragged_panel_ = panel;
    panel->StackAtTopOfLayer(StackingManager::LAYER_DRAGGED_PANEL);
    panel->SetShadowOpacity(1, kPanelShadowAnimMs);
  }

  if (drag_y + panel->total_height() > y_ + height_)
    drag_y = y_ + height_ - panel->total_height();
  if (drag_y < y_)
    drag_y = y_;
  panel->MoveY(drag_y, false, 0);

  return true;
}

void PanelDock::HandleNotifyPanelDragCompleteMessage(Panel* panel) {
  if (dragged_panel_ != panel)
    return;
  // Move client windows.
  panel->Move(panel->right(), panel->titlebar_y(), true, 0);
  if (panel->width() != width_) {
    panel->ResizeContent(
        width_, panel->content_height(),
        type_ == DOCK_TYPE_RIGHT ?
          Window::GRAVITY_NORTHEAST :
          Window::GRAVITY_NORTHWEST);
  }
  panel->SetShadowOpacity(0, kPanelShadowAnimMs);
  PackPanels(dragged_panel_);
  dragged_panel_ = NULL;
}

void PanelDock::HandleFocusPanelMessage(Panel* panel) {
  FocusPanel(panel, false, wm()->GetCurrentTimeFromServer());
}

void PanelDock::HandleScreenResize() {
  height_ = wm()->height();
  if (type_ == DOCK_TYPE_RIGHT)
    x_ = wm()->width() - width_;

  bool hidden = panels_.empty();

  // Move the background.
  int bg_x = x_;
  if (hidden)
    bg_x = (type_ == DOCK_TYPE_LEFT) ? x_ - width_ : x_ + width_;
  bg_actor_->SetSize(width_, height_);
  bg_actor_->Move(bg_x, y_, 0);
  bg_shadow_->Resize(width_, height_, 0);
  bg_shadow_->Move(bg_x, y_, 0);
  if (!hidden)
    wm()->ConfigureInputWindow(bg_input_xid_, x_, y_, width_, height_);

  // If we're on the right side of the screen, we need to move the panels.
  if (type_ == DOCK_TYPE_RIGHT) {
    for (vector<Panel*>::iterator it = panels_.begin();
         it != panels_.end(); ++it) {
      (*it)->MoveX(x_ + width_, true, 0);
    }
  }
}

WindowManager* PanelDock::wm() { return panel_manager_->wm(); }

void PanelDock::ExpandPanel(Panel* panel) {
  if (panel->is_expanded()) {
    LOG(WARNING) << "Ignoring request to expand already-expanded panel "
                 << panel->xid_str();
    return;
  }

  panel->SetExpandedState(true);
  PackPanels(panel);
}

void PanelDock::CollapsePanel(Panel* panel) {
  if (!panel->is_expanded()) {
    LOG(WARNING) << "Ignoring request to expand already-collapsed panel "
                 << panel->xid_str();
    return;
  }
  if (panel == panels_.back()) {
    VLOG(1) << "Ignoring request to collapse bottom panel " << panel->xid_str();
    return;
  }

  panel->SetExpandedState(false);
  PackPanels(panel);

  // If this panel was focused, find another one to focus instead.
  if (panel->content_win()->focused()) {
    Panel* new_panel_to_focus = GetNearestExpandedPanel(panel);
    if (new_panel_to_focus)
      FocusPanel(new_panel_to_focus, true, wm()->GetCurrentTimeFromServer());
    else
      wm()->TakeFocus();
  }
}

void PanelDock::PackPanels(Panel* starting_panel) {
  bool found_starting_panel = (starting_panel == NULL);
  int total_height = 0;
  Panel* prev_panel = NULL;

  for (vector<Panel*>::iterator it = panels_.begin();
       it != panels_.end(); ++it) {
    Panel* panel = *it;
    if (!found_starting_panel && panel == starting_panel)
      found_starting_panel = true;
    if (found_starting_panel) {
      if (prev_panel)
        panel->StackAbovePanel(
            prev_panel, StackingManager::LAYER_STATIONARY_PANEL_IN_DOCK);
      panel->MoveY(y_ + total_height, true, kPackPanelsAnimMs);
    }
    total_height += panel->is_expanded() ?
                    panel->total_height() :
                    panel->titlebar_height();
    prev_panel = panel;
  }

  // We stack panels relative to their siblings in the above loop so that
  // we won't get a bunch of flicker, but we need to handle the case where
  // there's only one initial panel separately (since we don't have
  // anything to stack it relative to).
  if (panels_.size() == static_cast<size_t>(1))
    panels_[0]->StackAtTopOfLayer(
        StackingManager::LAYER_STATIONARY_PANEL_IN_DOCK);
}

void PanelDock::FocusPanel(Panel* panel,
                           bool remove_pointer_grab,
                           Time timestamp) {
  DCHECK(panel);
  panel->RemoveButtonGrab(remove_pointer_grab);
  wm()->SetActiveWindowProperty(panel->content_win()->xid());
  panel->content_win()->TakeFocus(timestamp);
}

Panel* PanelDock::GetNearestExpandedPanel(Panel* panel) {
  DCHECK(panel);
  vector<Panel*>::iterator it = find(panels_.begin(), panels_.end(), panel);
  DCHECK(it != panels_.end());
  int panel_pos = it - panels_.begin();

  int nearest_pos = -1;
  for (int i = panel_pos - 1; i >= 0; --i) {
    if (panels_[i]->is_expanded()) {
      nearest_pos = i;
      break;
    }
  }
  for (int i = panel_pos + 1; i < static_cast<int>(panels_.size()); ++i) {
    if (panels_[i]->is_expanded()) {
      if (nearest_pos < 0 || i - panel_pos < panel_pos - nearest_pos)
        nearest_pos = i;
      break;
    }
  }

  if (nearest_pos < 0)
    return NULL;
  DCHECK(nearest_pos < static_cast<int>(panels_.size()));
  return panels_[nearest_pos];
}

};  // namespace window_manager
