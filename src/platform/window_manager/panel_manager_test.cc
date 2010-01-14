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
    panel_manager_->dragged_panel_event_coalescer_->set_synchronous(true);
    panel_bar_ = panel_manager_->panel_bar_.get();

    // Tell the WM that we implement a recent-enough version of the IPC
    // messages that we'll be giving it the position of the right-hand edge
    // of panels in drag messages.
    WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_IPC_VERSION);
    msg.set_param(0, 1);
    XEvent event;
    wm_->wm_ipc()->FillXEventFromMessage(&event, wm_->wm_xid(), msg);
    EXPECT_TRUE(wm_->HandleEvent(&event));
  }

  virtual void SendPanelDraggedMessage(Panel* panel, int x, int y) {
    WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_PANEL_DRAGGED);
    msg.set_param(0, panel->content_xid());
    msg.set_param(1, x);
    msg.set_param(2, y);
    XEvent event;
    wm_->wm_ipc()->FillXEventFromMessage(&event, wm_->wm_xid(), msg);
    EXPECT_TRUE(wm_->HandleEvent(&event));
  }

  virtual void SendPanelDragCompleteMessage(Panel* panel) {
    WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_PANEL_DRAG_COMPLETE);
    msg.set_param(0, panel->content_xid());
    XEvent event;
    wm_->wm_ipc()->FillXEventFromMessage(&event, wm_->wm_xid(), msg);
    EXPECT_TRUE(wm_->HandleEvent(&event));
  }

  PanelManager* panel_manager_;  // instance belonging to 'wm_'
  PanelBar* panel_bar_;          // instance belonging to 'panel_manager_'
};

// Test dragging a panel around to detach it and reattach it to the panel bar.
TEST_F(PanelManagerTest, AttachAndDetach) {
  const int titlebar_height = 20;
  XWindow titlebar_xid = CreatePanelTitlebarWindow(150, titlebar_height);
  SendInitialEventsForWindow(titlebar_xid);

  const int content_width = 300;
  const int content_height = 400;
  XWindow content_xid = CreatePanelContentWindow(
      content_width, content_height, titlebar_xid, true);
  SendInitialEventsForWindow(content_xid);

  // Get the position of the top of the expanded panel when it's in the bar.
  const int panel_y_in_bar =
      panel_bar_->y() - content_height - titlebar_height;

  // Drag the panel to the left, keeping it in line with the panel bar.
  Panel* panel = panel_manager_->GetPanelByXid(content_xid);
  ASSERT_TRUE(panel != NULL);
  SendPanelDraggedMessage(panel, 200, panel_y_in_bar);
  EXPECT_EQ(200, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());

  // Drag it up a bit, but not enough to detach it.
  SendPanelDraggedMessage(panel, 200, panel_y_in_bar - 5);
  EXPECT_EQ(200, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());

  // Now drag it up near the top of the screen.  It should get detached and
  // move to the same position as the mouse pointer.
  SendPanelDraggedMessage(panel, 400, 50);
  EXPECT_EQ(400, panel->right());
  EXPECT_EQ(50, panel->titlebar_y());

  // Drag the panel to a different spot near the top of the screen.
  SendPanelDraggedMessage(panel, 500, 25);
  EXPECT_EQ(500, panel->right());
  EXPECT_EQ(25, panel->titlebar_y());

  // Drag the panel all the way down to reattach it.
  SendPanelDraggedMessage(panel, 30, panel_bar_->y());
  EXPECT_EQ(30, panel->right());
  EXPECT_EQ(panel_y_in_bar, panel->titlebar_y());

  // Detach the panel again.
  SendPanelDraggedMessage(panel, 100, 20);
  EXPECT_EQ(100, panel->right());
  EXPECT_EQ(20, panel->titlebar_y());

  // Now finish the drag and check that the panel ends up back in the bar.
  SendPanelDragCompleteMessage(panel);
  EXPECT_EQ(100, panel->right());
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
  SendPanelDraggedMessage(panel, 400, panel_bar_->y());
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
