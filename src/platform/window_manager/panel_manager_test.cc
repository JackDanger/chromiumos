// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/panel.h"
#include "window_manager/panel_bar.h"
#include "window_manager/panel_manager.h"
#include "window_manager/test_lib.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/wm_ipc.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class PanelManagerTest : public BasicWindowManagerTest {
 protected:
  virtual void SetUp() {
    BasicWindowManagerTest::SetUp();
    panel_manager_ = wm_->panel_manager_.get();
    panel_bar_ = panel_manager_->panel_bar_.get();
  }

  PanelManager* panel_manager_;  // instance belonging to 'wm_'
  PanelBar* panel_bar_;          // instance belonging to 'panel_manager_'
};

// Test dragging a panel around to detach it and reattach it to the panel
// bar and panel docks.
TEST_F(PanelManagerTest, AttachAndDetach) {
  XConnection::WindowGeometry root_geometry;
  ASSERT_TRUE(
      xconn_->GetWindowGeometry(xconn_->GetRootWindow(), &root_geometry));

  const int titlebar_height = 20;
  const int content_width = 200;
  const int content_height = 400;
  Panel* panel =
      CreatePanel(content_width, titlebar_height, content_height, true);

  // Get the position of the top of the expanded panel when it's in the bar.
  const int panel_y_in_bar = wm_->height() - content_height - titlebar_height;

  // Drag the panel to the left, keeping it in line with the panel bar.
  SendPanelDraggedMessage(panel, 600, panel_y_in_bar);
  EXPECT_EQ(600, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());

  // Drag it up a bit, but not enough to detach it.
  SendPanelDraggedMessage(panel, 600, panel_y_in_bar - 5);
  EXPECT_EQ(600, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());

  // Now drag it up near the top of the screen.  It should get detached and
  // move to the same position as the mouse pointer.
  SendPanelDraggedMessage(panel, 500, 50);
  EXPECT_EQ(500, panel->right());
  EXPECT_EQ(50, panel->titlebar_y());

  // Drag the panel to a different spot near the top of the screen.
  SendPanelDraggedMessage(panel, 700, 25);
  EXPECT_EQ(700, panel->right());
  EXPECT_EQ(25, panel->titlebar_y());

  // Drag the panel all the way down to reattach it.
  SendPanelDraggedMessage(panel, 700, wm_->height() - 1);
  EXPECT_EQ(700, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());

  // Detach the panel again.
  SendPanelDraggedMessage(panel, 700, 20);
  EXPECT_EQ(700, panel->right());
  EXPECT_EQ(20, panel->titlebar_y());

  // Move the panel to the right side of the screen so it gets attached to
  // one of the panel docks.
  SendPanelDraggedMessage(panel, root_geometry.width - 10, 200);
  EXPECT_EQ(root_geometry.width, panel->right());
  EXPECT_EQ(200, panel->titlebar_y());

  // Move it left so it's attached to the other dock.
  SendPanelDraggedMessage(panel, 10, 300);
  EXPECT_EQ(panel->content_width(), panel->right());
  EXPECT_EQ(300, panel->titlebar_y());

  // Detach it again.
  SendPanelDraggedMessage(panel, 700, 300);
  EXPECT_EQ(700, panel->right());
  EXPECT_EQ(300, panel->titlebar_y());

  // Now finish the drag and check that the panel ends up back in the bar.
  SendPanelDragCompleteMessage(panel);
  EXPECT_EQ(wm_->width() - PanelBar::kPixelsBetweenPanels, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());
}

// Check that panels retain the focus when they get dragged out of the
// panel bar and reattached to it, and also that we assign the focus to a
// new panel when one with the focus gets destroyed.
TEST_F(PanelManagerTest, DragFocusedPanel) {
  // Create a panel and check that it has the focus.
  XWindow old_titlebar_xid = CreatePanelTitlebarWindow(150, 20);
  SendInitialEventsForWindow(old_titlebar_xid);
  XWindow old_content_xid =
      CreatePanelContentWindow(200, 300, old_titlebar_xid, true);
  SendInitialEventsForWindow(old_content_xid);
  ASSERT_EQ(old_content_xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), old_content_xid);

  // Create a second panel, which should take the focus.
  XWindow titlebar_xid = CreatePanelTitlebarWindow(150, 20);
  SendInitialEventsForWindow(titlebar_xid);
  XWindow content_xid = CreatePanelContentWindow(200, 300, titlebar_xid, true);
  SendInitialEventsForWindow(content_xid);
  ASSERT_EQ(content_xid, xconn_->focused_xid());
  SendFocusEvents(old_content_xid, content_xid);
  EXPECT_EQ(content_xid, GetActiveWindowProperty());

  // Drag the second panel out of the panel bar and check that it still has
  // the focus.
  Panel* panel = panel_manager_->GetPanelByXid(content_xid);
  ASSERT_TRUE(panel != NULL);
  SendPanelDraggedMessage(panel, 400, 50);
  ASSERT_TRUE(panel_manager_->GetContainerForPanel(*panel) == NULL);
  EXPECT_EQ(content_xid, xconn_->focused_xid());
  EXPECT_EQ(content_xid, GetActiveWindowProperty());

  // Now reattach it and check that it still has the focus.
  SendPanelDraggedMessage(panel, 400, wm_->height() - 1);
  ASSERT_EQ(panel_manager_->GetContainerForPanel(*panel), panel_bar_);
  EXPECT_EQ(content_xid, xconn_->focused_xid());
  EXPECT_EQ(content_xid, GetActiveWindowProperty());

  // Destroy the second panel.
  XEvent event;
  ASSERT_TRUE(xconn_->DestroyWindow(content_xid));
  MockXConnection::InitUnmapEvent(&event, content_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitDestroyWindowEvent(&event, content_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  ASSERT_TRUE(xconn_->DestroyWindow(titlebar_xid));
  MockXConnection::InitUnmapEvent(&event, titlebar_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitDestroyWindowEvent(&event, titlebar_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The first panel should be focused now.
  ASSERT_EQ(old_content_xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), old_content_xid);
  EXPECT_EQ(old_content_xid, GetActiveWindowProperty());
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
