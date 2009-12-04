// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/panel.h"
#include "window_manager/panel_bar.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace chromeos {

class PanelTest : public BasicWindowManagerTest {};

TEST_F(PanelTest, InputWindows) {
  XWindow titlebar_xid = CreateTitlebarWindow(200, 20);
  Window titlebar_win(wm_.get(), titlebar_xid);
  MockXConnection::WindowInfo* titlebar_info =
      xconn_->GetWindowInfoOrDie(titlebar_xid);

  XWindow panel_xid = CreatePanelWindow(200, 400, titlebar_xid, true);
  Window panel_win(wm_.get(), panel_xid);
  MockXConnection::WindowInfo* panel_info =
      xconn_->GetWindowInfoOrDie(panel_xid);

  // Create a panel and expand it.
  Panel panel(wm_->panel_bar_.get(), &panel_win, &titlebar_win, 0);
  panel.SetState(true);

  // Restack the panel and check that its titlebar is stacked above the
  // panel contents, and that the contents are above all of the input
  // windows used for resizing.
  panel.StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);
  EXPECT_LT(xconn_->stacked_xids().GetIndex(titlebar_xid),
            xconn_->stacked_xids().GetIndex(panel_xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(panel.top_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(panel.top_left_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(panel.top_right_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(panel.left_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(panel.right_input_xid_));

  // Now move the panel to a new location and check that all of the input
  // windows are moved correctly around it.
  panel.Move(wm_->panel_bar_->x() + wm_->panel_bar_->width() - 35, 0);

  MockXConnection::WindowInfo* top_info =
      xconn_->GetWindowInfoOrDie(panel.top_input_xid_);
  EXPECT_EQ(panel_info->x - Panel::kResizeBorderWidth +
              Panel::kResizeCornerSize,
            top_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth, top_info->y);
  EXPECT_EQ(titlebar_info->width + 2 * Panel::kResizeBorderWidth -
              2 * Panel::kResizeCornerSize,
            top_info->width);
  EXPECT_EQ(Panel::kResizeBorderWidth, top_info->height);

  MockXConnection::WindowInfo* top_left_info =
      xconn_->GetWindowInfoOrDie(panel.top_left_input_xid_);
  EXPECT_EQ(titlebar_info->x - Panel::kResizeBorderWidth, top_left_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth, top_left_info->y);
  EXPECT_EQ(Panel::kResizeCornerSize, top_left_info->width);
  EXPECT_EQ(Panel::kResizeCornerSize, top_left_info->height);

  MockXConnection::WindowInfo* top_right_info =
      xconn_->GetWindowInfoOrDie(panel.top_right_input_xid_);
  EXPECT_EQ(titlebar_info->x + titlebar_info->width +
              Panel::kResizeBorderWidth - Panel::kResizeCornerSize,
            top_right_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth, top_right_info->y);
  EXPECT_EQ(Panel::kResizeCornerSize, top_right_info->width);
  EXPECT_EQ(Panel::kResizeCornerSize, top_right_info->height);

  MockXConnection::WindowInfo* left_info =
      xconn_->GetWindowInfoOrDie(panel.left_input_xid_);
  EXPECT_EQ(panel_info->x - Panel::kResizeBorderWidth, left_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth +
              Panel::kResizeCornerSize,
            left_info->y);
  EXPECT_EQ(Panel::kResizeBorderWidth, left_info->width);
  EXPECT_EQ(panel_info->height + titlebar_info->height +
              Panel::kResizeBorderWidth - Panel::kResizeCornerSize,
            left_info->height);

  MockXConnection::WindowInfo* right_info =
      xconn_->GetWindowInfoOrDie(panel.right_input_xid_);
  EXPECT_EQ(panel_info->x + panel_info->width, right_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth +
              Panel::kResizeCornerSize,
            right_info->y);
  EXPECT_EQ(Panel::kResizeBorderWidth, right_info->width);
  EXPECT_EQ(panel_info->height + titlebar_info->height +
              Panel::kResizeBorderWidth - Panel::kResizeCornerSize,
            right_info->height);
}

}  // namespace chromeos

int main(int argc, char **argv) {
  return chromeos::InitAndRunTests(&argc, argv, FLAGS_logtostderr);
}
