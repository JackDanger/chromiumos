// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include <glog/logging.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

namespace chromeos {

class WindowManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    xconn_.reset(new MockXConnection);
    clutter_.reset(new MockClutterInterface);
    wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
    CHECK(wm_->Init());
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
};

class TestEventConsumer : public EventConsumer {
 public:
  TestEventConsumer()
      : EventConsumer(),
        num_mapped_windows_(0),
        num_unmapped_windows_(0),
        num_button_presses_(0) {
  }

  int num_mapped_windows() const { return num_mapped_windows_; }
  int num_unmapped_windows() const { return num_unmapped_windows_; }
  int num_button_presses() const { return num_button_presses_; }

  void HandleWindowMap(Window* win) { num_mapped_windows_++; }
  void HandleWindowUnmap(Window* win) { num_unmapped_windows_++; }
  bool HandleButtonPress(
      XWindow xid, int x, int y, int button, Time timestamp) {
    num_button_presses_++;
    return true;
  }

 private:
  int num_mapped_windows_;
  int num_unmapped_windows_;
  int num_button_presses_;
};

TEST_F(WindowManagerTest, InputWindows) {
  // Check that CreateInputWindow() creates windows as requested.
  XWindow xid = wm_->CreateInputWindow(100, 200, 300, 400);
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfo(xid);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(100, info->x);
  EXPECT_EQ(200, info->y);
  EXPECT_EQ(300, info->width);
  EXPECT_EQ(400, info->height);
  EXPECT_EQ(true, info->mapped);
  EXPECT_EQ(true, info->override_redirect);

  // Move and resize the window.
  EXPECT_TRUE(wm_->ConfigureInputWindow(xid, 500, 600, 700, 800));
  EXPECT_EQ(500, info->x);
  EXPECT_EQ(600, info->y);
  EXPECT_EQ(700, info->width);
  EXPECT_EQ(800, info->height);
  EXPECT_EQ(true, info->mapped);
}

TEST_F(WindowManagerTest, EventConsumer) {
  TestEventConsumer ec;
  wm_->event_consumers_.insert(&ec);

  // This window needs to have override redirect set; otherwise the
  // LayoutManager will claim ownership of the button press in the mistaken
  // belief that it's the result of a button grab on an unfocused window.
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      true,    // override redirect
      false,   // input only
      0);      // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Send various events to the WindowManager object and check that they
  // get forwarded to our EventConsumer.
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  MockXConnection::InitButtonPressEvent(&event, xid, 5, 5, 1);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  MockXConnection::InitUnmapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  EXPECT_EQ(1, ec.num_mapped_windows());
  EXPECT_EQ(1, ec.num_button_presses());
  EXPECT_EQ(1, ec.num_unmapped_windows());

  // TODO: Also test that map and unmap events get offered to all
  // consumers, while we only offer other events to consumers until we find
  // a consumer that handles them.

  // It's a bit of a stretch to include this in this test, but check that the
  // window manager didn't do anything to the window (since it's an
  // override-redirect window).
  EXPECT_FALSE(info->changed);
}

TEST_F(WindowManagerTest, Reparent) {
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  ASSERT_FALSE(info->redirected);

  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // WindowManager should redirect the window for compositing when it's
  // first created.
  EXPECT_TRUE(info->redirected);

  XReparentEvent* reparent_event = &(event.xreparent);
  memset(reparent_event, 0, sizeof(*reparent_event));
  reparent_event->type = ReparentNotify;
  reparent_event->window = xid;
  reparent_event->parent = 324324;  // arbitrary number
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // After the window gets reparented away from the root, WindowManager
  // should've unredirected it and should no longer be tracking it.
  EXPECT_TRUE(wm_->GetWindow(xid) == NULL);
  EXPECT_FALSE(info->redirected);
}

// Test that we ignore FocusIn and FocusOut events that occur as the result
// of a keyboard grab or ungrab, but honor other ones.
TEST_F(WindowManagerTest, IgnoreGrabFocusEvents) {
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);

  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  Window* win = wm_->GetWindow(xid);
  ASSERT_TRUE(win != NULL);
  EXPECT_TRUE(win->focused());

  // We should ignore a focus-out event caused by a grab...
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyGrab);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());

  // ... but honor one that comes in independently from a grab.
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(win->focused());

  // Similarly, we should ignore a focus-in event caused by an ungrab...
  MockXConnection::InitFocusInEvent(&event, xid, NotifyUngrab);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_FALSE(win->focused());

  // ... but honor one that comes in independently.
  MockXConnection::InitFocusInEvent(&event, xid, NotifyNormal);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());

  // We should pay attention to events that come in while a grab is already
  // active, though.
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyWhileGrabbed);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(win->focused());
  MockXConnection::InitFocusInEvent(&event, xid, NotifyWhileGrabbed);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());
}

TEST_F(WindowManagerTest, KeyEventSnooping) {
  XEvent event;

  // Create a toplevel window...
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // ... and a second one.
  XWindow xid2 = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info2 = xconn_->GetWindowInfoOrDie(xid2);
  MockXConnection::InitCreateWindowEvent(&event, *info2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid2);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Now create a window that's a child of the first window.  The window
  // manager doesn't expect to receive events for it yet.
  XWindow child_xid = CreateSimpleWindow(xid);
  MockXConnection::WindowInfo* child_info =
      xconn_->GetWindowInfoOrDie(child_xid);

  // Turn on key event snooping.
  wm_->SetKeyEventSnooping(true);

  // This should've selected KeyPressMask, KeyReleaseMask, and
  // SubstructureNotifyMask on all windows (the root already had
  // SubstructureNotifyMask selected, and we should've preserved all
  // windows' existing masks as well).
  MockXConnection::WindowInfo* root_info =
      xconn_->GetWindowInfoOrDie(xconn_->GetRootWindow());
  EXPECT_TRUE(root_info->event_mask & KeyPressMask);
  EXPECT_TRUE(root_info->event_mask & KeyReleaseMask);
  EXPECT_TRUE(root_info->event_mask & SubstructureNotifyMask);
  EXPECT_TRUE(root_info->event_mask & SubstructureRedirectMask);

  EXPECT_TRUE(info->event_mask & KeyPressMask);
  EXPECT_TRUE(info->event_mask & KeyReleaseMask);
  EXPECT_TRUE(info->event_mask & PropertyChangeMask);
  EXPECT_TRUE(info->event_mask & SubstructureNotifyMask);

  EXPECT_TRUE(info2->event_mask & KeyPressMask);
  EXPECT_TRUE(info2->event_mask & KeyReleaseMask);
  EXPECT_TRUE(info2->event_mask & PropertyChangeMask);
  EXPECT_TRUE(info2->event_mask & SubstructureNotifyMask);

  EXPECT_TRUE(child_info->event_mask & KeyPressMask);
  EXPECT_TRUE(child_info->event_mask & KeyReleaseMask);
  EXPECT_FALSE(child_info->event_mask & PropertyChangeMask);
  EXPECT_TRUE(child_info->event_mask & SubstructureNotifyMask);

  // Create a child of the existing child.
  XWindow grandchild_xid = CreateSimpleWindow(child_xid);
  MockXConnection::WindowInfo* grandchild_info =
      xconn_->GetWindowInfoOrDie(grandchild_xid);
  MockXConnection::InitCreateWindowEvent(&event, *grandchild_info);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, grandchild_xid);
  EXPECT_FALSE(wm_->HandleEvent(&event));

  // Check that we select its events but don't treat it as a regular
  // toplevel client window.
  EXPECT_TRUE(grandchild_info->event_mask & KeyPressMask);
  EXPECT_TRUE(grandchild_info->event_mask & KeyReleaseMask);
  EXPECT_TRUE(grandchild_info->event_mask & SubstructureNotifyMask);
  EXPECT_TRUE(wm_->GetWindow(grandchild_xid) == NULL);
  EXPECT_FALSE(wm_->stacked_xids_->Contains(grandchild_xid));

  // Unmap and destroy the grandchild window to check that the WM ignores
  // the events.
  CHECK(xconn_->DestroyWindow(grandchild_xid));
  grandchild_info = NULL;
  MockXConnection::InitUnmapEvent(&event, grandchild_xid);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  MockXConnection::InitDestroyWindowEvent(&event, grandchild_xid);
  EXPECT_FALSE(wm_->HandleEvent(&event));

  // Disable key event snooping.
  wm_->SetKeyEventSnooping(false);

  // Check that event masks are reset correctly.
  EXPECT_FALSE(root_info->event_mask & KeyPressMask);
  EXPECT_FALSE(root_info->event_mask & KeyReleaseMask);
  // We should leave SubstructureNotifyMask alone on the root window.
  EXPECT_TRUE(root_info->event_mask & SubstructureNotifyMask);
  EXPECT_TRUE(root_info->event_mask & SubstructureRedirectMask);

  EXPECT_FALSE(info->event_mask & KeyPressMask);
  EXPECT_FALSE(info->event_mask & KeyReleaseMask);
  EXPECT_TRUE(info->event_mask & PropertyChangeMask);
  EXPECT_FALSE(info->event_mask & SubstructureNotifyMask);

  EXPECT_FALSE(info2->event_mask & KeyPressMask);
  EXPECT_FALSE(info2->event_mask & KeyReleaseMask);
  EXPECT_TRUE(info2->event_mask & PropertyChangeMask);
  EXPECT_FALSE(info2->event_mask & SubstructureNotifyMask);

  EXPECT_FALSE(child_info->event_mask & KeyPressMask);
  EXPECT_FALSE(child_info->event_mask & KeyReleaseMask);
  EXPECT_FALSE(child_info->event_mask & SubstructureNotifyMask);
}

TEST_F(WindowManagerTest, TransientWindows) {
  XEvent event;

  // Create and map a window.
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

TEST_F(WindowManagerTest, RestackOverrideRedirectWindows) {
  XEvent event;

  // Create two override-redirect windows and map them both.
  XWindow xid = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      true,    // override redirect
      false,   // input only
      0);      // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  xconn_->MapWindow(xid);
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  Window* win = wm_->GetWindow(xid);
  ASSERT_TRUE(win != NULL);

  XWindow xid2 = xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      10, 20,  // x, y
      30, 40,  // width, height
      true,    // override redirect
      false,   // input only
      0);      // event mask
  MockXConnection::WindowInfo* info2 = xconn_->GetWindowInfoOrDie(xid2);
  MockXConnection::InitCreateWindowEvent(&event, *info2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  xconn_->MapWindow(xid2);
  MockXConnection::InitMapEvent(&event, xid2);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  Window* win2 = wm_->GetWindow(xid2);
  ASSERT_TRUE(win2 != NULL);

  // Send a ConfigureNotify saying that the second window has been stacked
  // on top of the first and then make sure that the Clutter actors are
  // stacked in the same manner.
  MockXConnection::InitConfigureNotifyEvent(&event, *info2);
  event.xconfigure.above = xid;
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  EXPECT_LT(stage->stacked_children()->GetIndex(
                dynamic_cast<MockClutterInterface::Actor*>(win2->actor())),
            stage->stacked_children()->GetIndex(
                dynamic_cast<MockClutterInterface::Actor*>(win->actor())));

  // Now send a message saying that the first window is on top of the second.
  MockXConnection::InitConfigureNotifyEvent(&event, *info);
  event.xconfigure.above = xid2;
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_LT(stage->stacked_children()->GetIndex(
                dynamic_cast<MockClutterInterface::Actor*>(win->actor())),
            stage->stacked_children()->GetIndex(
                dynamic_cast<MockClutterInterface::Actor*>(win2->actor())));
}

}  // namespace chromeos

int main(int argc, char **argv) {
  google::InitGoogleLogging(argv[0]);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
