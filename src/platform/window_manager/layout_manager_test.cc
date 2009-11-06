// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/layout_manager.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace chromeos {

class LayoutManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    xconn_.reset(new MockXConnection);
    clutter_.reset(new MockClutterInterface);
    wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
    CHECK(wm_->Init());
    lm_ = wm_->layout_manager_.get();
  }

  // Simple method that creates a toplevel client window and returns its ID.
  virtual XWindow CreateSimpleWindow(XWindow parent) {
    return xconn_->CreateWindow(
        parent,
        10, 20,  // x, y
        30, 40,  // width, height
        false,   // override redirect
        false,   // input only
        0);      // event mask
  }

  scoped_ptr<MockXConnection> xconn_;
  scoped_ptr<MockClutterInterface> clutter_;
  scoped_ptr<WindowManager> wm_;
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
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  ASSERT_EQ(None, xconn_->focused_xid());

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
  EXPECT_EQ(None, wm_->active_window_xid());
  EXPECT_TRUE(info->all_buttons_grabbed);

  // We shouldn't actually update _NET_ACTIVE_WINDOW and remove the passive
  // button grab until we get the FocusIn event.
  MockXConnection::InitFocusInEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, wm_->active_window_xid());
  EXPECT_FALSE(info->all_buttons_grabbed);

  // Now create a second window.
  XWindow xid2 = CreateSimpleWindow(xconn_->GetRootWindow());
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
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, wm_->active_window_xid());
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
  MockXConnection::InitFocusOutEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, wm_->active_window_xid());
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_TRUE(info2->all_buttons_grabbed);

  // Unmap the first window and check that the second window gets focused.
  MockXConnection::InitUnmapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, xconn_->focused_xid());
  ASSERT_TRUE(lm_->active_toplevel_ != NULL);
  EXPECT_EQ(xid2, lm_->active_toplevel_->win()->xid());

  MockXConnection::InitFocusInEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, wm_->active_window_xid());
  EXPECT_FALSE(info2->all_buttons_grabbed);
}

TEST_F(LayoutManagerTest, ConfigureTransient) {
  XEvent event;

  // Create and map a toplevel window.
  XWindow owner_xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* owner_info =
      xconn_->GetWindowInfoOrDie(owner_xid);
  MockXConnection::InitCreateWindowEvent(&event, *owner_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, owner_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitConfigureNotifyEvent(&event, *owner_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

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
  MockXConnection::InitCreateWindowEvent(&event, *transient_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, transient_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

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
}

TEST_F(LayoutManagerTest, FocusTransient) {
  // Create a window.
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  // Send CreateNotify, MapNotify, and FocusNotify events.
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, xconn_->focused_xid());
  MockXConnection::InitFocusInEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_EQ(xid, wm_->active_window_xid());
  EXPECT_TRUE(wm_->GetWindow(xid)->focused());

  // Now create a transient window.
  XWindow transient_xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* transient_info =
      xconn_->GetWindowInfoOrDie(transient_xid);
  transient_info->transient_for = xid;

  // Send CreateNotify and MapNotify events for the transient window.
  MockXConnection::InitCreateWindowEvent(&event, *transient_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, transient_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // We should ask the X server to focus the transient window as soon as it
  // gets mapped.
  EXPECT_EQ(transient_xid, xconn_->focused_xid());

  // Send FocusOut and FocusIn events and check that we add a passive
  // button grab on the owner window and remove the grab on the transient.
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(info->all_buttons_grabbed);
  EXPECT_FALSE(transient_info->all_buttons_grabbed);
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());

  // _NET_ACTIVE_WINDOW should still be set to the owner instead of the
  // transient window, though.
  EXPECT_EQ(xid, wm_->active_window_xid());

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
  MockXConnection::InitFocusOutEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(info->all_buttons_grabbed);
  EXPECT_TRUE(transient_info->all_buttons_grabbed);
  EXPECT_EQ(xid, wm_->active_window_xid());
  EXPECT_TRUE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());

  // Give the focus back to the transient window.
  xconn_->set_pointer_grab_xid(transient_xid);
  MockXConnection::InitButtonPressEvent(&event, transient_xid, 0, 0, 1);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(transient_xid, xconn_->focused_xid());
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
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());

  // Now create another top-level window, which we'll switch to
  // automatically.
  XWindow xid2 = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info2 = xconn_->GetWindowInfoOrDie(xid2);
  MockXConnection::InitCreateWindowEvent(&event, *info2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, wm_->active_window_xid());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(xid2)->focused());

  // When we cycle to the first toplevel window, its modal transient
  // window, rather than the toplevel itself, should get the focus.
  lm_->CycleActiveToplevelWindow(false);
  EXPECT_EQ(transient_xid, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, wm_->active_window_xid());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(xid2)->focused());

  // Switch back to the second toplevel window.
  lm_->CycleActiveToplevelWindow(false);
  EXPECT_EQ(xid2, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid2, wm_->active_window_xid());
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
  MockXConnection::InitFocusOutEvent(&event, xid2, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(xid, wm_->active_window_xid());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_TRUE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(xid2)->focused());

  // Switch to overview mode.  We should give the focus back to the root
  // window (we don't want the transient to receive keypresses at this
  // point).
  lm_->SetMode(LayoutManager::MODE_OVERVIEW);
  EXPECT_EQ(xconn_->GetRootWindow(), xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(&event, transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(None, wm_->active_window_xid());
  EXPECT_FALSE(wm_->GetWindow(xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(transient_xid)->focused());
  EXPECT_FALSE(wm_->GetWindow(xid2)->focused());
}

TEST_F(LayoutManagerTest, MultipleTransients) {
  // Create a window.
  XWindow owner_xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* owner_info =
      xconn_->GetWindowInfoOrDie(owner_xid);

  // Send CreateNotify, MapNotify, and FocusNotify events.
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *owner_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, owner_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, owner_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Create a transient window, send CreateNotify and MapNotify events for
  // it, and check that it has the focus.
  XWindow first_transient_xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* first_transient_info =
      xconn_->GetWindowInfoOrDie(first_transient_xid);
  first_transient_info->transient_for = owner_xid;
  MockXConnection::InitCreateWindowEvent(&event, *first_transient_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, first_transient_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(first_transient_xid, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(&event, owner_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, first_transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));

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
  XWindow second_transient_xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* second_transient_info =
      xconn_->GetWindowInfoOrDie(second_transient_xid);
  second_transient_info->transient_for = owner_xid;
  MockXConnection::InitCreateWindowEvent(&event, *second_transient_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, second_transient_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(second_transient_xid, xconn_->focused_xid());
  MockXConnection::InitFocusOutEvent(&event, first_transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, second_transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));

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
  MockXConnection::InitFocusOutEvent(
      &event, second_transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitFocusInEvent(&event, first_transient_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
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
  MockXConnection::InitFocusOutEvent(&event, first_transient_xid, NotifyNormal);
  EXPECT_FALSE(wm_->HandleEvent(&event));  // false because window was unmapped
  MockXConnection::InitFocusInEvent(&event, second_transient_xid, NotifyNormal);
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
      &event, second_transient_xid, NotifyNormal);
  EXPECT_FALSE(wm_->HandleEvent(&event));  // false because window was unmapped
  MockXConnection::InitFocusInEvent(&event, owner_xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
}

TEST_F(LayoutManagerTest, SetWmStateMaximized) {
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  std::vector<int> atoms;
  ASSERT_TRUE(xconn_->GetIntArrayProperty(
                  xid, wm_->GetXAtom(ATOM_NET_WM_STATE), &atoms));
  ASSERT_EQ(2, atoms.size());
  EXPECT_EQ(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ), atoms[0]);
  EXPECT_EQ(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT), atoms[1]);
}

TEST_F(LayoutManagerTest, Resize) {
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

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

}  // namespace chromeos

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       FLAGS_logtostderr ?
                         logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                         logging::LOG_NONE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
