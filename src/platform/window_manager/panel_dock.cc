// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/panel_dock.h"

#include "window_manager/panel.h"
#include "window_manager/window_manager.h"

namespace window_manager {

// Distance between the panel and the edge of the dock at which we detach it.
static const int kDetachThresholdPixels = 50;

// Distance between the panel and the edge of the dock at which we attach it.
static const int kAttachThresholdPixels = 20;

PanelDock::PanelDock(WindowManager* wm, DockPosition position)
    : wm_(wm),
      position_(position),
      dragged_panel_(NULL) {
}

PanelDock::~PanelDock() {
  dragged_panel_ = NULL;
}

void PanelDock::AddPanel(Panel* panel, PanelSource source, bool expanded) {
  panel->StackAtTopOfLayer(
      source == PANEL_SOURCE_DRAGGED ?
        StackingManager::LAYER_DRAGGED_EXPANDED_PANEL :
        StackingManager::LAYER_EXPANDED_PANEL);

  if (panel->content_win()->focused()) {
    FocusPanel(panel, false, wm_->GetCurrentTimeFromServer());
  } else {
    panel->AddButtonGrab();
  }
}

void PanelDock::RemovePanel(Panel* panel) {
  if (dragged_panel_ == panel)
    dragged_panel_ = NULL;
}

bool PanelDock::ShouldAddDraggedPanel(const Panel* panel,
                                      int drag_x,
                                      int drag_y) {
  if (position_ == DOCK_POSITION_LEFT) {
    return (drag_x - panel->content_width() <= kAttachThresholdPixels);
  } else if (position_ == DOCK_POSITION_RIGHT) {
    return (drag_x >= wm_->width() - kAttachThresholdPixels);
  } else {
    NOTREACHED() << "Unknown position " << position_;
    return false;
  }
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
}

bool PanelDock::HandleNotifyPanelDraggedMessage(Panel* panel,
                                                int drag_x,
                                                int drag_y) {
  if (position_ == DOCK_POSITION_LEFT) {
    if (drag_x - panel->content_width() >= kDetachThresholdPixels)
      return false;
  } else if (position_ == DOCK_POSITION_RIGHT) {
    if (drag_x <= wm_->width() - kDetachThresholdPixels)
      return false;
  } else {
    NOTREACHED() << "Unknown position " << position_;
    return false;
  }

  if (dragged_panel_ != panel) {
    dragged_panel_ = panel;
    panel->StackAtTopOfLayer(StackingManager::LAYER_DRAGGED_EXPANDED_PANEL);
  }

  if (position_ == DOCK_POSITION_LEFT) {
    panel->Move(panel->content_width(), drag_y, false, 0);
  } else if (position_ == DOCK_POSITION_RIGHT) {
    panel->Move(wm_->width(), drag_y, false, 0);
  }

  return true;
}

void PanelDock::HandleNotifyPanelDragCompleteMessage(Panel* panel) {
  if (dragged_panel_ != panel)
    return;
  // Move client windows.
  panel->Move(panel->right(), panel->titlebar_y(), true, 0);
  panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
  dragged_panel_ = NULL;
}

void PanelDock::HandleFocusPanelMessage(Panel* panel) {
  FocusPanel(panel, false, wm_->GetCurrentTimeFromServer());
}

void PanelDock::FocusPanel(Panel* panel,
                           bool remove_pointer_grab,
                           Time timestamp) {
  panel->RemoveButtonGrab(remove_pointer_grab);
  wm_->SetActiveWindowProperty(panel->content_win()->xid());
  panel->content_win()->TakeFocus(timestamp);
  panel->StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
}

};  // namespace window_manager
