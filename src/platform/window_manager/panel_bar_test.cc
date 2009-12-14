// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/panel_bar.h"
#include "window_manager/shadow.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/wm_ipc.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace chromeos {

class PanelBarTest : public BasicWindowManagerTest {};

TEST_F(PanelBarTest, Basic) {
  // First, create a toplevel window.
  XWindow toplevel_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* toplevel_info =
      xconn_->GetWindowInfoOrDie(toplevel_xid);
  SendInitialEventsForWindow(toplevel_xid);

  // It should be initially focused.
  EXPECT_EQ(toplevel_xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), toplevel_xid);
  EXPECT_EQ(toplevel_xid, wm_->active_window_xid());

  // Now create a panel titlebar, and then the actual panel window.
  const int initial_titlebar_height = 16;
  XWindow titlebar_xid = CreateTitlebarWindow(100, initial_titlebar_height);
  MockXConnection::WindowInfo* titlebar_info =
      xconn_->GetWindowInfoOrDie(titlebar_xid);
  SendInitialEventsForWindow(titlebar_xid);

  const int initial_panel_width = 250;
  const int initial_panel_height = 400;
  XWindow panel_xid = CreatePanelWindow(
      initial_panel_width, initial_panel_height, titlebar_xid, true);
  MockXConnection::WindowInfo* panel_info =
      xconn_->GetWindowInfoOrDie(panel_xid);
  SendInitialEventsForWindow(panel_xid);

  // The toplevel window should retain the focus and a button grab should
  // be installed on the titlebar window.
  EXPECT_EQ(toplevel_xid, xconn_->focused_xid());
  EXPECT_TRUE(panel_info->all_buttons_grabbed);

  // The titlebar should keep its initial height but be stretched to the
  // panel's width.  The panel's initial width and height should be
  // preserved.
  EXPECT_EQ(initial_panel_width, titlebar_info->width);
  EXPECT_EQ(initial_titlebar_height, titlebar_info->height);
  EXPECT_EQ(initial_panel_width, panel_info->width);
  EXPECT_EQ(initial_panel_height, panel_info->height);

  // The titlebar and panel client windows should be stacked above the
  // toplevel window's client window.
  EXPECT_LT(xconn_->stacked_xids().GetIndex(titlebar_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid));

  Window* toplevel_win = wm_->GetWindow(toplevel_xid);
  ASSERT_TRUE(toplevel_win != NULL);
  Window* titlebar_win = wm_->GetWindow(titlebar_xid);
  ASSERT_TRUE(titlebar_win != NULL);
  Window* panel_win = wm_->GetWindow(panel_xid);
  ASSERT_TRUE(panel_win != NULL);

  // The titlebar and panel actors and their shadows should all be stacked
  // on top of the toplevel window's actor.
  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->actor()),
            stage->GetStackingIndex(toplevel_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->shadow()->group()),
            stage->GetStackingIndex(toplevel_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(panel_win->actor()),
            stage->GetStackingIndex(toplevel_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(panel_win->shadow()->group()),
            stage->GetStackingIndex(toplevel_win->actor()));

  // The titlebar and panel windows shouldn't cast shadows on each other.
  EXPECT_LT(stage->GetStackingIndex(panel_win->actor()),
            stage->GetStackingIndex(titlebar_win->shadow()->group()));
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->actor()),
            stage->GetStackingIndex(panel_win->shadow()->group()));

  // After a button press on the panel window, its active and passive grabs
  // should be removed and it should be focused.
  xconn_->set_pointer_grab_xid(panel_xid);
  XEvent event;
  MockXConnection::InitButtonPressEvent(
      &event, panel_xid, 0, 0, 1);  // x, y, button
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, xconn_->pointer_grab_xid());
  EXPECT_EQ(panel_xid, xconn_->focused_xid());
  EXPECT_FALSE(panel_info->all_buttons_grabbed);

  // Send FocusOut and FocusIn events and check that the active window hint
  // is updated to contain the panel window.
  SendFocusEvents(toplevel_xid, panel_xid);
  EXPECT_EQ(panel_xid, wm_->active_window_xid());

  // Create a second toplevel window.
  XWindow toplevel_xid2 = CreateSimpleWindow();
  MockXConnection::WindowInfo* toplevel_info2 =
      xconn_->GetWindowInfoOrDie(toplevel_xid2);
  SendInitialEventsForWindow(toplevel_xid2);
  Window* toplevel_win2 = wm_->GetWindow(toplevel_xid2);
  ASSERT_TRUE(toplevel_win2 != NULL);

  // The panel's and titlebar's client and composited windows should be
  // stacked above those of the new toplevel window.
  EXPECT_LT(xconn_->stacked_xids().GetIndex(titlebar_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid2));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(panel_xid),
            xconn_->stacked_xids().GetIndex(toplevel_xid2));
  EXPECT_LT(stage->GetStackingIndex(titlebar_win->actor()),
            stage->GetStackingIndex(toplevel_win2->actor()));
  EXPECT_LT(stage->GetStackingIndex(panel_win->actor()),
            stage->GetStackingIndex(toplevel_win2->actor()));
}

}  // namespace chromeos

int main(int argc, char **argv) {
  return chromeos::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
