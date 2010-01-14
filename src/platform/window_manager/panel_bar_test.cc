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
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/wm_ipc.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class PanelBarTest : public BasicWindowManagerTest {
 protected:
  virtual void SetUp() {
    BasicWindowManagerTest::SetUp();
    panel_bar_ = wm_->panel_manager_->panel_bar_.get();
  }

  PanelBar* panel_bar_;  // instance belonging to wm_->panel_manager_
};

TEST_F(PanelBarTest, Basic) {
  // First, create a toplevel window.
  XWindow toplevel_xid = CreateSimpleWindow();
  SendInitialEventsForWindow(toplevel_xid);

  // It should be initially focused.
  EXPECT_EQ(toplevel_xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), toplevel_xid);
  EXPECT_EQ(toplevel_xid, GetActiveWindowProperty());

  // Now create a panel titlebar, and then the content window.
  const int initial_titlebar_height = 16;
  XWindow titlebar_xid =
      CreatePanelTitlebarWindow(100, initial_titlebar_height);
  MockXConnection::WindowInfo* titlebar_info =
      xconn_->GetWindowInfoOrDie(titlebar_xid);
  SendInitialEventsForWindow(titlebar_xid);

  const int initial_content_width = 250;
  const int initial_content_height = 400;
  XWindow content_xid = CreatePanelContentWindow(
      initial_content_width, initial_content_height, titlebar_xid, true);
  MockXConnection::WindowInfo* content_info =
      xconn_->GetWindowInfoOrDie(content_xid);
  SendInitialEventsForWindow(content_xid);

  // The panel's content window should take the focus, and no button grab
  // should be installed yet.
  EXPECT_EQ(content_xid, xconn_->focused_xid());
  SendFocusEvents(toplevel_xid, content_xid);
  EXPECT_EQ(content_xid, GetActiveWindowProperty());

  // Click on the toplevel window to give it the focus again.  A button
  // grab should be installed on the panel's content window.
  xconn_->set_pointer_grab_xid(toplevel_xid);
  XEvent event;
  MockXConnection::InitButtonPressEvent(
      &event, toplevel_xid, 0, 0, 1);  // x, y, button
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, xconn_->pointer_grab_xid());
  EXPECT_EQ(toplevel_xid, xconn_->focused_xid());
  SendFocusEvents(content_xid, toplevel_xid);
  EXPECT_TRUE(content_info->button_is_grabbed(AnyButton));
  EXPECT_EQ(toplevel_xid, GetActiveWindowProperty());

  // The titlebar should keep its initial height but be stretched to the
  // panel's width.  The content window's initial width and height should be
  // preserved.
  EXPECT_EQ(initial_content_width, titlebar_info->width);
  EXPECT_EQ(initial_titlebar_height, titlebar_info->height);
  EXPECT_EQ(initial_content_width, content_info->width);
  EXPECT_EQ(initial_content_height, content_info->height);

  // The titlebar and content client windows should be stacked above the
  // toplevel window's client window.
  EXPECT_LT(xconn_->stacked_xids().GetIndex(titlebar_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid));

  Window* toplevel_win = wm_->GetWindow(toplevel_xid);
  ASSERT_TRUE(toplevel_win != NULL);
  Window* titlebar_win = wm_->GetWindow(titlebar_xid);
  ASSERT_TRUE(titlebar_win != NULL);
  Window* content_win = wm_->GetWindow(content_xid);
  ASSERT_TRUE(content_win != NULL);

  // The titlebar and content actors and their shadows should all be stacked
  // on top of the toplevel window's actor.
  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->actor()),
            stage->GetStackingIndex(toplevel_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->shadow()->group()),
            stage->GetStackingIndex(toplevel_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(content_win->actor()),
            stage->GetStackingIndex(toplevel_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(content_win->shadow()->group()),
            stage->GetStackingIndex(toplevel_win->actor()));

  // The titlebar and content windows shouldn't cast shadows on each other.
  EXPECT_LT(stage->GetStackingIndex(content_win->actor()),
            stage->GetStackingIndex(titlebar_win->shadow()->group()));
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->actor()),
            stage->GetStackingIndex(content_win->shadow()->group()));

  // After a button press on the content window, its active and passive grabs
  // should be removed and it should be focused.
  xconn_->set_pointer_grab_xid(content_xid);
  MockXConnection::InitButtonPressEvent(
      &event, content_xid, 0, 0, 1);  // x, y, button
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, xconn_->pointer_grab_xid());
  EXPECT_EQ(content_xid, xconn_->focused_xid());
  EXPECT_FALSE(content_info->button_is_grabbed(AnyButton));

  // Send FocusOut and FocusIn events and check that the active window hint
  // is updated to contain the content window.
  SendFocusEvents(toplevel_xid, content_xid);
  EXPECT_EQ(content_xid, GetActiveWindowProperty());

  // Create a second toplevel window.
  XWindow toplevel_xid2 = CreateSimpleWindow();
  SendInitialEventsForWindow(toplevel_xid2);
  Window* toplevel_win2 = wm_->GetWindow(toplevel_xid2);
  ASSERT_TRUE(toplevel_win2 != NULL);

  // Check that the new toplevel window takes the focus (note that this is
  // testing LayoutManager code).
  EXPECT_EQ(toplevel_xid2, xconn_->focused_xid());
  SendFocusEvents(content_xid, toplevel_xid2);
  EXPECT_EQ(toplevel_xid2, GetActiveWindowProperty());

  // The panel's and titlebar's client and composited windows should be
  // stacked above those of the new toplevel window.
  EXPECT_LT(xconn_->stacked_xids().GetIndex(titlebar_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid2));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(content_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid2));
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->actor()),
            stage->GetStackingIndex(toplevel_win2->actor()));
  EXPECT_LT(stage->GetStackingIndex(content_win->actor()),
            stage->GetStackingIndex(toplevel_win2->actor()));

  // Create a second, collapsed panel.
  XWindow collapsed_titlebar_xid = CreatePanelTitlebarWindow(200, 20);
  SendInitialEventsForWindow(collapsed_titlebar_xid);
  XWindow collapsed_content_xid =
      CreatePanelContentWindow(200, 400, collapsed_titlebar_xid, false);
  SendInitialEventsForWindow(collapsed_content_xid);

  // The collapsed panel shouldn't have taken the focus.
  EXPECT_EQ(toplevel_xid2, xconn_->focused_xid());
  EXPECT_EQ(toplevel_xid2, GetActiveWindowProperty());
}

// Test that we expand and focus panels in response to _NET_ACTIVE_WINDOW
// client messages.
TEST_F(PanelBarTest, ActiveWindowMessage) {
  // Create a collapsed panel.
  XWindow titlebar_xid = CreatePanelTitlebarWindow(200, 20);
  SendInitialEventsForWindow(titlebar_xid);
  XWindow content_xid = CreatePanelContentWindow(200, 400, titlebar_xid, false);
  SendInitialEventsForWindow(content_xid);

  // Make sure that it starts out collapsed.
  Window* content_win = wm_->GetWindow(content_xid);
  ASSERT_TRUE(content_win != NULL);
  Panel* panel = panel_bar_->GetPanelByWindow(*content_win);
  ASSERT_TRUE(panel != NULL);
  EXPECT_FALSE(panel_bar_->GetPanelInfoOrDie(panel)->is_expanded);
  EXPECT_NE(content_xid, xconn_->focused_xid());

  // After sending a _NET_ACTIVE_WINDOW message asking the window manager
  // to focus the panel, it should be expanded and get the focus, and the
  // _NET_ACTIVE_WINDOW property should contain its ID.
  XEvent event;
  MockXConnection::InitClientMessageEvent(
      &event,
      content_xid,  // window to focus
      wm_->GetXAtom(ATOM_NET_ACTIVE_WINDOW),
      1,          // source indication: client app
      CurrentTime,
      None,       // currently-active window
      None);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(panel_bar_->GetPanelInfoOrDie(panel)->is_expanded);
  EXPECT_EQ(content_xid, xconn_->focused_xid());
  EXPECT_EQ(content_xid, GetActiveWindowProperty());
}

// Regression test for bug 540, a crash caused by PanelBar's window-unmap
// code calling WindowManager::TakeFocus() before the panel had been
// completely destroyed, resulting in PanelBar::TakeFocus() trying to
// refocus the partially-destroyed panel.
TEST_F(PanelBarTest, FocusNewPanel) {
  // Create an expanded panel.
  XWindow titlebar_xid = CreatePanelTitlebarWindow(200, 20);
  SendInitialEventsForWindow(titlebar_xid);
  XWindow content_xid = CreatePanelContentWindow(200, 400, titlebar_xid, true);
  SendInitialEventsForWindow(content_xid);

  // It should be focused initially.
  EXPECT_EQ(content_xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), content_xid);
  EXPECT_EQ(content_xid, GetActiveWindowProperty());

  // The panel's address should be contained in 'desired_panel_to_focus_'.
  ASSERT_EQ(1, panel_bar_->expanded_panels_.size());
  EXPECT_EQ(panel_bar_->expanded_panels_[0],
            panel_bar_->desired_panel_to_focus_);

  // Now send an unmap event for the content window.  The panel object
  // should be destroyed, and 'desired_panel_to_focus_' shouldn't refer to
  // it anymore.
  XEvent event;
  MockXConnection::InitUnmapEvent(&event, content_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(panel_bar_->expanded_panels_.empty());
  EXPECT_EQ(NULL, panel_bar_->desired_panel_to_focus_);
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
