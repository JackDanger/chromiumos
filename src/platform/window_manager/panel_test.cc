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

class PanelTest : public BasicWindowManagerTest {
 protected:
  virtual void SetUp() {
    BasicWindowManagerTest::SetUp();
    panel_bar_ = wm_->panel_bar_.get();
  }

  PanelBar* panel_bar_;  // instance belonging to 'wm_'
};

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
  Panel panel(panel_bar_, &panel_win, &titlebar_win, 0);
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
  panel.Move(panel_bar_->x() + panel_bar_->width() - 35, 0);

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

TEST_F(PanelTest, Resize) {
  int orig_width = 200;
  int orig_titlebar_height = 20;
  XWindow titlebar_xid = CreateTitlebarWindow(orig_width, orig_titlebar_height);
  Window titlebar_win(wm_.get(), titlebar_xid);
  MockXConnection::WindowInfo* titlebar_info =
      xconn_->GetWindowInfoOrDie(titlebar_xid);

  int orig_panel_height = 400;
  XWindow panel_xid =
      CreatePanelWindow(orig_width, orig_panel_height, titlebar_xid, true);
  Window panel_win(wm_.get(), panel_xid);
  MockXConnection::WindowInfo* panel_info =
      xconn_->GetWindowInfoOrDie(panel_xid);

  // Create a panel and expand it.
  Panel panel(panel_bar_, &panel_win, &titlebar_win, 0);
  panel.SetState(true);

  // The panel should grab the pointer when it gets a button press on one
  // of its resize windows.
  panel.HandleInputWindowButtonPress(
      panel.top_left_input_xid_,
      0, 0,  // relative x, y
      1,     // button
      1);    // timestamp
  EXPECT_EQ(panel.top_left_input_xid_, xconn_->pointer_grab_xid());

  // Release the button immediately and check that the grab has been
  // removed.
  panel.HandleInputWindowButtonRelease(
      panel.top_left_input_xid_,
      0, 0,  // relative x, y
      1);    // button
  EXPECT_EQ(None, xconn_->pointer_grab_xid());

  // Check that the panel's dimensions are unchanged.
  EXPECT_EQ(orig_width, titlebar_info->width);
  EXPECT_EQ(orig_titlebar_height, titlebar_info->height);
  EXPECT_EQ(orig_width, panel_info->width);
  EXPECT_EQ(orig_panel_height, panel_info->height);

  int initial_x = titlebar_info->x;
  EXPECT_EQ(initial_x, panel_info->x);
  int initial_titlebar_y = titlebar_info->y;
  EXPECT_EQ(initial_titlebar_y + titlebar_info->height, panel_info->y);

  // Now start a second resize using the upper-left handle.  Drag a few
  // pixels up and to the left and then let go of the button.
  panel.HandleInputWindowButtonPress(
      panel.top_left_input_xid_, 0, 0, 1, 1);
  EXPECT_EQ(panel.top_left_input_xid_, xconn_->pointer_grab_xid());
  panel.HandleInputWindowPointerMotion(panel.top_left_input_xid_, -2, -4);
  panel.HandleInputWindowButtonRelease(panel.top_left_input_xid_, -5, -6, 1);
  EXPECT_EQ(None, xconn_->pointer_grab_xid());

  // The titlebar should be offset by the drag and made a bit wider.
  EXPECT_EQ(initial_x - 5, titlebar_info->x);
  EXPECT_EQ(initial_titlebar_y - 6, titlebar_info->y);
  EXPECT_EQ(orig_width + 5, titlebar_info->width);
  EXPECT_EQ(orig_titlebar_height, titlebar_info->height);

  // The panel should move along with its titlebar, and it should get wider
  // and taller by the amount of the drag.
  EXPECT_EQ(initial_x - 5, panel_info->x);
  EXPECT_EQ(titlebar_info->y + titlebar_info->height, panel_info->y);
  EXPECT_EQ(orig_width + 5, panel_info->width);
  EXPECT_EQ(orig_panel_height + 6, panel_info->height);
}

// Test that the _CHROME_STATE property is updated correctly to reflect the
// panel's expanded/collapsed state.
TEST_F(PanelTest, ChromeState) {
  const XAtom state_atom = wm_->GetXAtom(ATOM_CHROME_STATE);
  const XAtom collapsed_atom = wm_->GetXAtom(ATOM_CHROME_STATE_COLLAPSED_PANEL);

  // Create a panel.
  XWindow titlebar_xid = CreateTitlebarWindow(200, 20);
  Window titlebar_win(wm_.get(), titlebar_xid);
  XWindow panel_xid = CreatePanelWindow(200, 400, titlebar_xid, false);
  Window panel_win(wm_.get(), panel_xid);
  Panel panel(panel_bar_, &panel_win, &titlebar_win, 0);

  // The panel's content window should have have a collapsed state in
  // _CHROME_STATE initially.
  std::vector<int> values;
  ASSERT_TRUE(xconn_->GetIntArrayProperty(panel_xid, state_atom, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(collapsed_atom, values[0]);

  // After we tell the panel to expand itself, it should remove the
  // collapsed atom (and additionally, the entire property).
  panel.SetState(true);
  EXPECT_FALSE(xconn_->GetIntArrayProperty(panel_xid, state_atom, &values));
}

}  // namespace chromeos

int main(int argc, char **argv) {
  return chromeos::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
