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
#include "window_manager/panel_manager.h"
#include "window_manager/shadow.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class PanelTest : public BasicWindowManagerTest {
 protected:
  virtual void SetUp() {
    BasicWindowManagerTest::SetUp();
    panel_bar_ = wm_->panel_manager_->panel_bar_.get();
  }

  PanelBar* panel_bar_;  // instance belonging to wm_->panel_manager_
};

TEST_F(PanelTest, InputWindows) {
  XWindow titlebar_xid = CreatePanelTitlebarWindow(200, 20);
  Window titlebar_win(wm_.get(), titlebar_xid, false);
  MockXConnection::WindowInfo* titlebar_info =
      xconn_->GetWindowInfoOrDie(titlebar_xid);

  XWindow content_xid = CreatePanelContentWindow(200, 400, titlebar_xid, true);
  Window content_win(wm_.get(), content_xid, false);
  MockXConnection::WindowInfo* content_info =
      xconn_->GetWindowInfoOrDie(content_xid);

  // Create a panel.
  Panel panel(wm_.get(), &content_win, &titlebar_win);
  panel.SetResizable(true);
  panel.Move(0, 0, true, 0);

  // Restack the panel and check that its titlebar is stacked above the
  // content window, and that the content window is above all of the input
  // windows used for resizing.
  panel.StackAtTopOfLayer(StackingManager::LAYER_STATIONARY_PANEL);
  EXPECT_LT(xconn_->stacked_xids().GetIndex(titlebar_xid),
            xconn_->stacked_xids().GetIndex(content_xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(panel.top_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(panel.top_left_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(panel.top_right_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(panel.left_input_xid_));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(panel.right_input_xid_));

  // Now move the panel to a new location and check that all of the input
  // windows are moved correctly around it.
  panel.MoveX(wm_->width() - 35, true, 0);

  MockXConnection::WindowInfo* top_info =
      xconn_->GetWindowInfoOrDie(panel.top_input_xid_);
  EXPECT_EQ(content_info->x - Panel::kResizeBorderWidth +
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
  EXPECT_EQ(content_info->x - Panel::kResizeBorderWidth, left_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth +
              Panel::kResizeCornerSize,
            left_info->y);
  EXPECT_EQ(Panel::kResizeBorderWidth, left_info->width);
  EXPECT_EQ(content_info->height + titlebar_info->height +
              Panel::kResizeBorderWidth - Panel::kResizeCornerSize,
            left_info->height);

  MockXConnection::WindowInfo* right_info =
      xconn_->GetWindowInfoOrDie(panel.right_input_xid_);
  EXPECT_EQ(content_info->x + content_info->width, right_info->x);
  EXPECT_EQ(titlebar_info->y - Panel::kResizeBorderWidth +
              Panel::kResizeCornerSize,
            right_info->y);
  EXPECT_EQ(Panel::kResizeBorderWidth, right_info->width);
  EXPECT_EQ(content_info->height + titlebar_info->height +
              Panel::kResizeBorderWidth - Panel::kResizeCornerSize,
            right_info->height);
}

TEST_F(PanelTest, Resize) {
  int orig_width = 200;
  int orig_titlebar_height = 20;
  XWindow titlebar_xid =
      CreatePanelTitlebarWindow(orig_width, orig_titlebar_height);
  Window titlebar_win(wm_.get(), titlebar_xid, false);
  MockXConnection::WindowInfo* titlebar_info =
      xconn_->GetWindowInfoOrDie(titlebar_xid);

  int orig_content_height = 400;
  XWindow content_xid = CreatePanelContentWindow(
      orig_width, orig_content_height, titlebar_xid, true);
  Window content_win(wm_.get(), content_xid, false);
  MockXConnection::WindowInfo* content_info =
      xconn_->GetWindowInfoOrDie(content_xid);

  // Create a panel.
  Panel panel(wm_.get(), &content_win, &titlebar_win);
  panel.SetResizable(true);
  panel.Move(0, 0, true, 0);

  // Check that one of the panel's resize handles has an asynchronous grab
  // installed on the first mouse button.
  MockXConnection::WindowInfo* handle_info =
      xconn_->GetWindowInfoOrDie(panel.top_left_input_xid_);
  EXPECT_TRUE(handle_info->button_is_grabbed(1));
  EXPECT_EQ(ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
            handle_info->button_grabs[1].event_mask);
  EXPECT_FALSE(handle_info->button_grabs[1].synchronous);

  // Pretend like the top left handle was clicked and a pointer grab was
  // automatically installed.
  xconn_->set_pointer_grab_xid(panel.top_left_input_xid_);
  panel.HandleInputWindowButtonPress(
      panel.top_left_input_xid_,
      0, 0,  // relative x, y
      1,     // button
      1);    // timestamp

  // Release the button immediately.
  xconn_->set_pointer_grab_xid(None);
  panel.HandleInputWindowButtonRelease(
      panel.top_left_input_xid_,
      0, 0,  // relative x, y
      1,     // button
      CurrentTime);

  // Check that the panel's dimensions are unchanged.
  EXPECT_EQ(orig_width, titlebar_info->width);
  EXPECT_EQ(orig_titlebar_height, titlebar_info->height);
  EXPECT_EQ(orig_width, content_info->width);
  EXPECT_EQ(orig_content_height, content_info->height);

  int initial_x = titlebar_info->x;
  EXPECT_EQ(initial_x, content_info->x);
  int initial_titlebar_y = titlebar_info->y;
  EXPECT_EQ(initial_titlebar_y + titlebar_info->height, content_info->y);

  // Now start a second resize using the upper-left handle.  Drag a few
  // pixels up and to the left and then let go of the button.
  xconn_->set_pointer_grab_xid(panel.top_left_input_xid_);
  panel.HandleInputWindowButtonPress(
      panel.top_left_input_xid_, 0, 0, 1, CurrentTime);
  EXPECT_EQ(panel.top_left_input_xid_, xconn_->pointer_grab_xid());
  panel.HandleInputWindowPointerMotion(panel.top_left_input_xid_, -2, -4);
  xconn_->set_pointer_grab_xid(None);
  panel.HandleInputWindowButtonRelease(
      panel.top_left_input_xid_, -5, -6, 1, CurrentTime);

  // The titlebar should be offset by the drag and made a bit wider.
  EXPECT_EQ(initial_x - 5, titlebar_info->x);
  EXPECT_EQ(initial_titlebar_y - 6, titlebar_info->y);
  EXPECT_EQ(orig_width + 5, titlebar_info->width);
  EXPECT_EQ(orig_titlebar_height, titlebar_info->height);

  // The panel should move along with its titlebar, and it should get wider
  // and taller by the amount of the drag.
  EXPECT_EQ(initial_x - 5, content_info->x);
  EXPECT_EQ(titlebar_info->y + titlebar_info->height, content_info->y);
  EXPECT_EQ(orig_width + 5, content_info->width);
  EXPECT_EQ(orig_content_height + 6, content_info->height);
}

// Test that the _CHROME_STATE property is updated correctly to reflect the
// panel's expanded/collapsed state.
TEST_F(PanelTest, ChromeState) {
  const XAtom state_atom = wm_->GetXAtom(ATOM_CHROME_STATE);
  const XAtom collapsed_atom = wm_->GetXAtom(ATOM_CHROME_STATE_COLLAPSED_PANEL);

  // Create a panel.
  XWindow titlebar_xid = CreatePanelTitlebarWindow(200, 20);
  Window titlebar_win(wm_.get(), titlebar_xid, false);
  XWindow content_xid = CreatePanelContentWindow(200, 400, titlebar_xid, false);
  Window content_win(wm_.get(), content_xid, false);
  Panel panel(wm_.get(), &content_win, &titlebar_win);
  panel.Move(0, 0, true, 0);

  // The panel's content window should have have a collapsed state in
  // _CHROME_STATE initially.
  panel.NotifyChromeAboutState(false);
  std::vector<int> values;
  ASSERT_TRUE(xconn_->GetIntArrayProperty(content_xid, state_atom, &values));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ(collapsed_atom, values[0]);

  // After we tell the panel to notify Chrome that it's been expanded, it
  // should remove the collapsed atom (and additionally, the entire
  // property).
  panel.NotifyChromeAboutState(true);
  EXPECT_FALSE(xconn_->GetIntArrayProperty(content_xid, state_atom, &values));
}

// Test that we're able to hide content windows' shadows (we do this when
// panels are collapsed so they won't show up across the bottom of the
// screen).
TEST_F(PanelTest, Shadows) {
  // Create a panel.
  XWindow titlebar_xid = CreatePanelTitlebarWindow(200, 20);
  Window titlebar_win(wm_.get(), titlebar_xid, false);
  XWindow content_xid = CreatePanelContentWindow(200, 400, titlebar_xid, false);
  Window content_win(wm_.get(), content_xid, false);
  Panel panel(wm_.get(), &content_win, &titlebar_win);
  panel.Move(0, 0, true, 0);

  // Both the titlebar and content windows' shadows should be visible
  // initially.
  EXPECT_TRUE(titlebar_win.shadow()->is_shown());
  EXPECT_TRUE(content_win.shadow()->is_shown());
  EXPECT_DOUBLE_EQ(1.0, titlebar_win.shadow()->opacity());
  EXPECT_DOUBLE_EQ(1.0, content_win.shadow()->opacity());

  // Now tell the panel to hide the content shadow.
  panel.SetContentShadowOpacity(0, 0);
  EXPECT_TRUE(titlebar_win.shadow()->is_shown());
  EXPECT_TRUE(content_win.shadow()->is_shown());
  EXPECT_DOUBLE_EQ(1.0, titlebar_win.shadow()->opacity());
  EXPECT_DOUBLE_EQ(0.0, content_win.shadow()->opacity());
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
