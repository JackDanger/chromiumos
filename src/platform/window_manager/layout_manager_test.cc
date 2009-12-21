// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/layout_manager.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/wm_ipc.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class LayoutManagerTest : public BasicWindowManagerTest {
 protected:
  virtual void SetUp() {
    BasicWindowManagerTest::SetUp();
    lm_ = wm_->layout_manager_.get();
  }

  LayoutManager* lm_;  // points to wm_'s copy
};

TEST_F(LayoutManagerTest, Basic) {
  XWindow xid1 = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      100, 100,  // x, y
      640, 480,  // width, height
      false,     // override redirect
      false,     // input only
      0);        // event mask
  wm_->TrackWindow(xid1);

  Window* win1 = wm_->GetWindow(xid1);
  CHECK(win1);
  win1->MapClient();

  lm_->SetMode(LayoutManager::MODE_ACTIVE);
  lm_->HandleWindowMap(win1);
  int x = lm_->x() + 0.5 * (lm_->width() - win1->client_width());
  int y = lm_->y() + 0.5 * (lm_->height() - win1->client_height());
  EXPECT_EQ(x, win1->client_x());
  EXPECT_EQ(y, win1->client_y());
  EXPECT_EQ(x, win1->composited_x());
  EXPECT_EQ(y, win1->composited_y());
  EXPECT_DOUBLE_EQ(1.0, win1->composited_scale_x());
  EXPECT_DOUBLE_EQ(1.0, win1->composited_scale_y());
  EXPECT_DOUBLE_EQ(1.0, win1->composited_opacity());

  // Now create two more windows and map them.
  XWindow xid2 = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      100, 100,  // x, y
      640, 480,  // width, height
      false,     // override redirect
      false,     // input only
      0);        // event mask
  wm_->TrackWindow(xid2);
  Window* win2 = wm_->GetWindow(xid2);
  CHECK(win2);
  win2->MapClient();
  lm_->HandleWindowMap(win2);

  XWindow xid3 = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      100, 100,  // x, y
      640, 480,  // width, height
      false,     // override redirect
      false,     // input only
      0);        // event mask
  wm_->TrackWindow(xid3);
  Window* win3 = wm_->GetWindow(xid3);
  CHECK(win3);
  win3->MapClient();
  lm_->HandleWindowMap(win3);

  // The third window should be onscreen now, and the first and second
  // windows should be offscreen.
  EXPECT_EQ(wm_->width(), win1->client_x());
  EXPECT_EQ(wm_->height(), win1->client_y());
  EXPECT_EQ(wm_->width(), win2->client_x());
  EXPECT_EQ(wm_->height(), win2->client_y());
  EXPECT_EQ(x, win3->client_x());
  EXPECT_EQ(y, win3->client_y());
  EXPECT_EQ(x, win3->composited_x());
  EXPECT_EQ(y, win3->composited_y());
  // TODO: Test composited position.  Maybe just check that it's offscreen?

  // After cycling the windows, the second and third windows should be
  // offscreen and the first window should be centered.
  lm_->CycleActiveToplevelWindow(true);
  EXPECT_EQ(x, win1->client_x());
  EXPECT_EQ(y, win1->client_y());
  EXPECT_EQ(x, win1->composited_x());
  EXPECT_EQ(y, win1->composited_y());
  EXPECT_EQ(wm_->width(), win2->client_x());
  EXPECT_EQ(wm_->height(), win2->client_y());
  EXPECT_EQ(wm_->width(), win3->client_x());
  EXPECT_EQ(wm_->height(), win3->client_y());
}

TEST_F(LayoutManagerTest, Focus) {
  // Create a window.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  EXPECT_EQ(None, xconn_->focused_xid());

  // Send a CreateNotify event to the window manager.
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, xconn_->focused_xid());
  EXPECT_TRUE(lm_->active_toplevel_ == NULL);

  // The layout manager should activate and focus the window when it gets
  // mapped.
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, xconn_->focused_xid());
  ASSERT_TRUE(lm_->active_toplevel_ != NULL);
  EXPECT_EQ(xid, lm_->active_toplevel_->win()->xid());
  EXPECT_EQ(None, GetActiveWindowProperty());
  EXPECT_TRUE(info->all_buttons_grabbed);

  // We shouldn't actually update _NET_ACTIVE_WINDOW and remove the passive
  // button grab until we get the FocusIn event.
  SendFocusEvents(xconn_->GetRootWindow(), xid);
  EXPECT_EQ(xid, GetActiveWindowProperty());
  EXPECT_FALSE(info->all_buttons_grabbed);

  // Now create a second window.
  XWindow xid2 = CreateSimpleWindow();
  MockXConnection::WindowInfo* info2 = xconn_->GetWindowInfoOrDie(xid2);

  // When the second window is created, the first should still be active.
  MockXConnection::InitCreateWindowEvent(&event, *info2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, xconn_->focused_xid());
  ASSERT_TRUE(lm_->active_toplevel_ != NULL);
  EXPECT_EQ(xid, lm_->active_toplevel_->win()->xid());

  // When the second window is mapped, it should become the active window.
  MockXConnection::InitMapEvent(&event, xid2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, xconn_->focused_xid());
  ASSERT_TRUE(lm_->active_toplevel_ != NULL);
  EXPECT_EQ(xid2, lm_->active_toplevel_->win()->xid());
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_TRUE(info2->all_buttons_grabbed);

  // Now send the appropriate FocusOut and FocusIn events.
  SendFocusEvents(xid, xid2);
  EXPECT_EQ(xid2, GetActiveWindowProperty());
  EXPECT_TRUE(info->all_buttons_grabbed);
  EXPECT_FALSE(info2->all_buttons_grabbed);

  // Now send a _NET_ACTIVE_WINDOW message asking the window manager to
  // focus the first window.
  MockXConnection::InitClientMessageEvent(
      &event,
      xid,   // window to focus
      wm_->GetXAtom(ATOM_NET_ACTIVE_WINDOW),
      1,     // source indication: client app
      CurrentTime,
      xid2,  // currently-active window
      None);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, xconn_->focused_xid());
  ASSERT_TRUE(lm_->active_toplevel_ != NULL);
  EXPECT_EQ(xid, lm_->active_toplevel_->win()->xid());

  // Send the appropriate FocusOut and FocusIn events.
  SendFocusEvents(xid2, xid);
  EXPECT_EQ(xid, GetActiveWindowProperty());
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_TRUE(info2->all_buttons_grabbed);

  // Unmap the first window and check that the second window gets focused.
  MockXConnection::InitUnmapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, xconn_->focused_xid());
  ASSERT_TRUE(lm_->active_toplevel_ != NULL);
  EXPECT_EQ(xid2, lm_->active_toplevel_->win()->xid());

  SendFocusEvents(None, xid2);
  EXPECT_EQ(xid2, GetActiveWindowProperty());
  EXPECT_FALSE(info2->all_buttons_grabbed);
}

TEST_F(LayoutManagerTest, ConfigureTransient) {
  XEvent event;

  // Create and map a toplevel window.
  XWindow owner_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* owner_info =
      xconn_->GetWindowInfoOrDie(owner_xid);
  SendInitialEventsForWindow(owner_xid);
  MockXConnection::InitConfigureNotifyEvent(&event, *owner_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  EXPECT_EQ(0, owner_info->x);
  EXPECT_EQ(0, owner_info->y);
  EXPECT_EQ(lm_->width(), owner_info->width);
  EXPECT_EQ(lm_->height(), owner_info->height);

  // Now create and map a transient window.
  XWindow transient_xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      60, 70,    // x, y
      320, 240,  // width, height
      false,     // override redirect
      false,     // input only
      0);        // event mask
  MockXConnection::WindowInfo* transient_info =
      xconn_->GetWindowInfoOrDie(transient_xid);
  transient_info->transient_for = owner_xid;
  SendInitialEventsForWindow(transient_xid);

  // The transient window should initially be centered over its owner.
  EXPECT_EQ(owner_info->x + 0.5 * (owner_info->width - transient_info->width),
            transient_info->x);
  EXPECT_EQ(owner_info->y + 0.5 * (owner_info->height - transient_info->height),
            transient_info->y);
  MockXConnection::InitConfigureNotifyEvent(&event, *owner_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Send a ConfigureRequest event to move and resize the transient window
  // and make sure that it gets applied.
  MockXConnection::InitConfigureRequestEvent(
      &event, transient_xid, owner_info->x + 20, owner_info->y + 10, 200, 150);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(owner_info->x + 20, transient_info->x);
  EXPECT_EQ(owner_info->y + 10, transient_info->y);
  EXPECT_EQ(200, transient_info->width);
  EXPECT_EQ(150, transient_info->height);

  // Create and map an info bubble window.
  int bubble_x = owner_info->x + 40;
  int bubble_y = owner_info->y + 30;
  XWindow bubble_xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      bubble_x, bubble_y,
      320, 240,  // width, height
      false,     // override redirect
      false,     // input only
      0);        // event mask
  ASSERT_TRUE(wm_->wm_ipc()->SetWindowType(
                  bubble_xid, WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE, NULL));
  MockXConnection::WindowInfo* bubble_info =
      xconn_->GetWindowInfoOrDie(bubble_xid);
  bubble_info->transient_for = owner_xid;
  SendInitialEventsForWindow(bubble_xid);

  // The bubble's initial position should be preserved.
  EXPECT_EQ(bubble_x, bubble_info->x);
  EXPECT_EQ(bubble_y, bubble_info->y);
  MockXConnection::InitConfigureNotifyEvent(&event, *owner_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
}

TEST_F(LayoutManagerTest, FocusTransient) {
  // Create a window.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  // Send CreateNotify, MapNotify, and FocusNotify events.
  XEvent event;
  SendInitialEventsForWindow(xid);
  EXPECT_EQ(xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), xid);
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_EQ(xid, GetActiveWindowProperty());
  EXPECT_TRUE(wm_->GetWindow(xid)->focused());

  // Now create a transient window.
  XWindow transient_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* transient_info =
      xconn_->GetWindowInfoOrDie(transient_xid);
  transient_info->transient_for = xid;

  // Send CreateNotify and MapNotify events for the transient window.
  SendInitialEventsForWindow(transient_xid);

  // We should ask the X server to focus the transient window as soon as it
  // gets mapped.
  EXPECT_EQ(transient_xid, xconn_->focused_xid());

  // Send FocusOut and FocusIn events and check that we add a passive
  // button grab on the owner window and remove the grab on the transient.
  SendFocusEvents(xid, transient_xid);
  EXPECT_TRUE(info->all_buttons_grabbed);
  EXPECT_FALSE(transient_info->all_buttons_grabbed);
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());

  // _NET_ACTIVE_WINDOW should also be set to the transient window (EWMH is
  // vague about this, but it seems to match what other WMs do).
  EXPECT_EQ(transient_xid, GetActiveWindowProperty());

  // Now simulate a button press on the owner window.
  xconn_->set_pointer_grab_xid(xid);
  MockXConnection::InitButtonPressEvent(&event, xid, 0, 0, 1);  // x, y, button
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // LayoutManager should remove the active pointer grab and try to focus
  // the owner window.
  EXPECT_EQ(None, xconn_->pointer_grab_xid());
  EXPECT_EQ(xid, xconn_->focused_xid());

  // After the FocusOut and FocusIn events come through, the button grabs
  // should be updated again.
  SendFocusEvents(transient_xid, xid);
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_TRUE(transient_info->all_buttons_grabbed);
  EXPECT_EQ(xid, GetActiveWindowProperty());
  EXPECT_TRUE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());

  // Give the focus back to the transient window.
  xconn_->set_pointer_grab_xid(transient_xid);
  MockXConnection::InitButtonPressEvent(&event, transient_xid, 0, 0, 1);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(transient_xid, xconn_->focused_xid());
  SendFocusEvents(xid, transient_xid);
  EXPECT_EQ(transient_xid, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());

  // Set the transient window as modal.
  MockXConnection::InitClientMessageEvent(
      &event, transient_xid, wm_->GetXAtom(ATOM_NET_WM_STATE),
      1, wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL), None, None);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Since it's modal, the transient window should still keep the focus
  // after a button press in the owner window.
  xconn_->set_pointer_grab_xid(xid);
  MockXConnection::InitButtonPressEvent(&event, xid, 0, 0, 1);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(transient_xid, xconn_->focused_xid());
  EXPECT_EQ(transient_xid, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());

  // Now create another toplevel window, which we'll switch to
  // automatically.
  XWindow xid2 = CreateSimpleWindow();
  MockXConnection::WindowInfo* info2 = xconn_->GetWindowInfoOrDie(xid2);
  SendInitialEventsForWindow(xid2);
  EXPECT_EQ(xid2, xconn_->focused_xid());
  SendFocusEvents(transient_xid, xid2);
  EXPECT_EQ(xid2, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(xid2)->focused());

  // When we cycle to the first toplevel window, its modal transient
  // window, rather than the toplevel itself, should get the focus.
  lm_->CycleActiveToplevelWindow(false);
  EXPECT_EQ(transient_xid, xconn_->focused_xid());
  SendFocusEvents(xid2, transient_xid);
  EXPECT_EQ(transient_xid, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(xid2)->focused());

  // Switch back to the second toplevel window.
  lm_->CycleActiveToplevelWindow(false);
  EXPECT_EQ(xid2, xconn_->focused_xid());
  SendFocusEvents(transient_xid, xid2);
  EXPECT_EQ(xid2, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(xid2)->focused());

  // Make the transient window non-modal.
  MockXConnection::InitClientMessageEvent(
      &event, transient_xid, wm_->GetXAtom(ATOM_NET_WM_STATE),
      0, wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL), None, None);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Now send a _NET_ACTIVE_WINDOW message asking to focus the transient.
  // We should switch back to the first toplevel, and the transient should
  // get the focus.
  MockXConnection::InitClientMessageEvent(
      &event, transient_xid, wm_->GetXAtom(ATOM_NET_ACTIVE_WINDOW),
      1, 21321, 0, None);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(transient_xid, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(
      &event, xid2, NotifyNormal, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(
      &event, transient_xid, NotifyNormal, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(transient_xid, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(xid2)->focused());

  // Switch to overview mode.  We should give the focus back to the root
  // window (we don't want the transient to receive keypresses at this
  // point).
  lm_->SetMode(LayoutManager::MODE_OVERVIEW);
  EXPECT_EQ(xconn_->GetRootWindow(), xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(
      &event, transient_xid, NotifyWhileGrabbed, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(
      &event, transient_xid, NotifyWhileGrabbed, NotifyPointer);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, GetActiveWindowProperty());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(xid2)->focused());
}

TEST_F(LayoutManagerTest, MultipleTransients) {
  // Create a window.
  XWindow owner_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* owner_info =
      xconn_->GetWindowInfoOrDie(owner_xid);

  // Send CreateNotify, MapNotify, and FocusNotify events.
  XEvent event;
  SendInitialEventsForWindow(owner_xid);
  EXPECT_EQ(owner_xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), owner_xid);

  // Create a transient window, send CreateNotify and MapNotify events for
  // it, and check that it has the focus.
  XWindow first_transient_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* first_transient_info =
      xconn_->GetWindowInfoOrDie(first_transient_xid);
  first_transient_info->transient_for = owner_xid;
  SendInitialEventsForWindow(first_transient_xid);
  EXPECT_EQ(first_transient_xid, xconn_->focused_xid());
  SendFocusEvents(owner_xid, first_transient_xid);

  // The transient window should be stacked on top of its actor (in terms
  // of both its composited and client windows).
  Window* owner_win = wm_->GetWindow(owner_xid);
  ASSERT_TRUE(owner_win != NULL);
  Window* first_transient_win = wm_->GetWindow(first_transient_xid);
  ASSERT_TRUE(first_transient_win != NULL);
  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  EXPECT_LT(stage->GetStackingIndex(first_transient_win->actor()),
            stage->GetStackingIndex(owner_win->actor()));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(first_transient_xid),
            xconn_->stacked_xids().GetIndex(owner_xid));

  // Now create a second transient window, which should get the focus when
  // it's mapped.
  XWindow second_transient_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* second_transient_info =
      xconn_->GetWindowInfoOrDie(second_transient_xid);
  second_transient_info->transient_for = owner_xid;
  SendInitialEventsForWindow(second_transient_xid);
  EXPECT_EQ(second_transient_xid, xconn_->focused_xid());
  SendFocusEvents(first_transient_xid, second_transient_xid);

  // The second transient should be on top of the first, which should be on
  // top of the owner.
  Window* second_transient_win = wm_->GetWindow(second_transient_xid);
  ASSERT_TRUE(second_transient_win != NULL);
  EXPECT_LT(stage->GetStackingIndex(second_transient_win->actor()),
            stage->GetStackingIndex(first_transient_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(first_transient_win->actor()),
            stage->GetStackingIndex(owner_win->actor()));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(second_transient_xid),
            xconn_->stacked_xids().GetIndex(first_transient_xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(first_transient_xid),
            xconn_->stacked_xids().GetIndex(owner_xid));

  // Click on the first transient.  It should get the focused and be moved to
  // the top of the stack.
  xconn_->set_pointer_grab_xid(first_transient_xid);
  MockXConnection::InitButtonPressEvent(&event, first_transient_xid, 0, 0, 1);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(first_transient_xid, xconn_->focused_xid());
  SendFocusEvents(second_transient_xid, first_transient_xid);
  EXPECT_LT(stage->GetStackingIndex(first_transient_win->actor()),
            stage->GetStackingIndex(second_transient_win->actor()));
  EXPECT_LT(stage->GetStackingIndex(second_transient_win->actor()),
            stage->GetStackingIndex(owner_win->actor()));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(first_transient_xid),
            xconn_->stacked_xids().GetIndex(second_transient_xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(second_transient_xid),
            xconn_->stacked_xids().GetIndex(owner_xid));

  // Unmap the first transient.  The second transient should be focused.
  MockXConnection::InitUnmapEvent(&event, first_transient_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(second_transient_xid, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(
      &event, first_transient_xid, NotifyNormal, NotifyNonlinear);
  EXPECT_FALSE(wm_->HandleEvent(&event));  // false because window was unmapped
  MockXConnection::InitFocusInEvent(
      &event, second_transient_xid, NotifyNormal, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_LT(stage->GetStackingIndex(second_transient_win->actor()),
            stage->GetStackingIndex(owner_win->actor()));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(second_transient_xid),
            xconn_->stacked_xids().GetIndex(owner_xid));

  // After we unmap the second transient, the owner should get the focus.
  MockXConnection::InitUnmapEvent(&event, second_transient_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(owner_xid, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(
      &event, second_transient_xid, NotifyNormal, NotifyNonlinear);
  EXPECT_FALSE(wm_->HandleEvent(&event));  // false because window was unmapped
  MockXConnection::InitFocusInEvent(
      &event, owner_xid, NotifyNormal, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
}

TEST_F(LayoutManagerTest, SetWmStateMaximized) {
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  SendInitialEventsForWindow(xid);

  std::vector<int> atoms;
  ASSERT_TRUE(xconn_->GetIntArrayProperty(
                  xid, wm_->GetXAtom(ATOM_NET_WM_STATE), &atoms));
  ASSERT_EQ(2, atoms.size());
  EXPECT_EQ(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ), atoms[0]);
  EXPECT_EQ(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT), atoms[1]);
}

TEST_F(LayoutManagerTest, Resize) {
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  SendInitialEventsForWindow(xid);

  Window* win = wm_->GetWindow(xid);
  ASSERT_TRUE(win != NULL);

  // The client window and its composited counterpart should be resized to
  // take up all the space available to the layout manager.
  EXPECT_EQ(lm_->x(), info->x);
  EXPECT_EQ(lm_->y(), info->y);
  EXPECT_EQ(lm_->width(), info->width);
  EXPECT_EQ(lm_->height(), info->height);
  EXPECT_EQ(lm_->x(), win->composited_x());
  EXPECT_EQ(lm_->y(), win->composited_y());
  EXPECT_DOUBLE_EQ(1.0, win->composited_scale_x());
  EXPECT_DOUBLE_EQ(1.0, win->composited_scale_y());

  // Now tell the layout manager to resize itself.  The client window
  // should also be resized.
  int new_width = lm_->width() / 2;
  int new_height = lm_->height() / 2;
  lm_->Resize(new_width, new_height);
  EXPECT_EQ(new_width, lm_->width());
  EXPECT_EQ(new_height, lm_->height());
  EXPECT_EQ(lm_->width(), info->width);
  EXPECT_EQ(lm_->height(), info->height);
}

// Test that we let clients resize toplevel windows after they've been
// mapped.  This isn't what we actually want to do (why would a client even
// care?  Their window is maximized), but is required to avoid triggering
// issue 449, where Chrome's option window seems to stop redrawing itself
// if it doesn't get the size that it asks for.
TEST_F(LayoutManagerTest, ConfigureToplevel) {
  // Create and map a toplevel window.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  SendInitialEventsForWindow(xid);

  // The window should initially be maximized to fit the area available to
  // the layout manager.
  EXPECT_EQ(lm_->x(), info->x);
  EXPECT_EQ(lm_->y(), info->y);
  EXPECT_EQ(lm_->width(), info->width);
  EXPECT_EQ(lm_->height(), info->height);

  // Now ask for a new position and larger size.
  int new_x = 20;
  int new_y = 40;
  int new_width = lm_->x() + 10;
  int new_height = lm_->y() + 5;
  XEvent event;
  MockXConnection::InitConfigureRequestEvent(
      &event, xid, 0, 0, new_width, new_height);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The position change should be ignored, but the window should be
  // resized.
  EXPECT_EQ(lm_->x(), info->x);
  EXPECT_EQ(lm_->y(), info->y);
  EXPECT_EQ(new_width, info->width);
  EXPECT_EQ(new_height, info->height);
}

TEST_F(LayoutManagerTest, OverviewFocus) {
  // Create and map a toplevel window.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  SendInitialEventsForWindow(xid);

  // The window should get the focus, the active window property should be
  // updated, and there shouldn't be a button grab on the window.
  EXPECT_EQ(xid, xconn_->focused_xid());
  SendFocusEvents(xconn_->GetRootWindow(), xid);
  EXPECT_EQ(xid, GetActiveWindowProperty());
  EXPECT_FALSE(info->all_buttons_grabbed);

  // Now create and map a second window.
  XWindow xid2 = CreateSimpleWindow();
  MockXConnection::WindowInfo* info2 = xconn_->GetWindowInfoOrDie(xid2);
  SendInitialEventsForWindow(xid2);

  // The second window should be focused and set as the active window,
  // and we should install a button grab on the first window.
  EXPECT_EQ(xid2, xconn_->focused_xid());
  SendFocusEvents(xid, xid2);
  EXPECT_EQ(xid2, GetActiveWindowProperty());
  EXPECT_TRUE(info->all_buttons_grabbed);
  EXPECT_FALSE(info2->all_buttons_grabbed);

  // Now switch to overview mode.  Neither window should have the focus,
  // both should have button grabs, and the active window property should
  // be unset.
  lm_->SetMode(LayoutManager::MODE_OVERVIEW);
  EXPECT_EQ(xconn_->GetRootWindow(), xconn_->focused_xid());
  XEvent event;
  MockXConnection::InitFocusOutEvent(
      &event, xid2, NotifyWhileGrabbed, NotifyVirtual);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  // This FocusIn event with detail NotifyPointer is odd, but appears to be
  // what happens in actuality.
  MockXConnection::InitFocusInEvent(
      &event, xid2, NotifyWhileGrabbed, NotifyPointer);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, GetActiveWindowProperty());
  EXPECT_TRUE(info->all_buttons_grabbed);
  EXPECT_TRUE(info2->all_buttons_grabbed);

  // Click on the first window's input window.
  XWindow input_xid = lm_->GetInputXidForWindow(*(wm_->GetWindow(xid)));
  EXPECT_TRUE(input_xid != None);
  MockXConnection::InitButtonPressEvent(
      &event, input_xid, 0, 0, 1);  // x, y, button
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The first window should be focused and set as the active window, and
  // only the second window should still have a button grab.
  EXPECT_EQ(xid, xconn_->focused_xid());
  SendFocusEvents(xid2, xid);
  EXPECT_EQ(xid, GetActiveWindowProperty());
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_TRUE(info2->all_buttons_grabbed);
}

// Test that already-existing windows get stacked correctly.
TEST_F(LayoutManagerTest, InitialWindowStacking) {
  // Reset everything so we can start from scratch.
  wm_.reset(NULL);
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface);
  lm_ = NULL;

  // Create and map a toplevel window.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  xconn_->MapWindow(xid);

  // Now create a new WindowManager object that will see the toplevel
  // window as already existing.
  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());

  // Get the stacking reference points for toplevel windows and for the
  // layer beneath them.
  XWindow toplevel_stacking_xid = FindWithDefault(
      wm_->stacking_manager()->layer_to_xid_,
      StackingManager::LAYER_TOPLEVEL_WINDOW,
      static_cast<XWindow>(None));
  ASSERT_TRUE(toplevel_stacking_xid != None);
  ClutterInterface::Actor* toplevel_stacking_actor = FindWithDefault(
      wm_->stacking_manager()->layer_to_actor_,
      StackingManager::LAYER_TOPLEVEL_WINDOW,
      std::tr1::shared_ptr<ClutterInterface::Actor>()).get();
  ASSERT_TRUE(toplevel_stacking_actor != None);

  XWindow lower_stacking_xid = FindWithDefault(
      wm_->stacking_manager()->layer_to_xid_,
      static_cast<StackingManager::Layer>(
          StackingManager::LAYER_TOPLEVEL_WINDOW + 1),
      static_cast<XWindow>(None));
  ASSERT_TRUE(lower_stacking_xid != None);
  ClutterInterface::Actor* lower_stacking_actor = FindWithDefault(
      wm_->stacking_manager()->layer_to_actor_,
      static_cast<StackingManager::Layer>(
          StackingManager::LAYER_TOPLEVEL_WINDOW + 1),
      std::tr1::shared_ptr<ClutterInterface::Actor>()).get();
  ASSERT_TRUE(lower_stacking_actor != None);

  // Check that the toplevel window is stacked between the two reference
  // points.
  EXPECT_LT(xconn_->stacked_xids().GetIndex(toplevel_stacking_xid),
            xconn_->stacked_xids().GetIndex(xid));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(xid),
            xconn_->stacked_xids().GetIndex(lower_stacking_xid));

  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  Window* win = wm_->GetWindow(xid);
  ASSERT_TRUE(win != NULL);
  EXPECT_LT(stage->GetStackingIndex(toplevel_stacking_actor),
            stage->GetStackingIndex(win->actor()));
  EXPECT_LT(stage->GetStackingIndex(win->actor()),
            stage->GetStackingIndex(lower_stacking_actor));
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
