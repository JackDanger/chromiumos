// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include <gtest/gtest.h>

#include <base/command_line.h>
#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/layout_manager.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/panel_bar.h"
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

TEST_F(WindowManagerTest, RegisterExistence) {
  // First, make sure that the window manager created a window and gave it
  // a title.
  XAtom title_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("_NET_WM_NAME", &title_atom));
  std::string window_title;
  EXPECT_TRUE(
      xconn_->GetStringProperty(wm_->wm_window_, title_atom, &window_title));
  EXPECT_EQ(WindowManager::kWmName, window_title);

  // Check that the window and compositing manager selections are owned by
  // the window manager's window.
  XAtom wm_atom = None, cm_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("WM_S0", &wm_atom));
  ASSERT_TRUE(xconn_->GetAtom("_NET_WM_CM_S0", &cm_atom));
  EXPECT_EQ(wm_->wm_window_, xconn_->GetSelectionOwner(wm_atom));
  EXPECT_EQ(wm_->wm_window_, xconn_->GetSelectionOwner(cm_atom));

  XAtom manager_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("MANAGER", &manager_atom));

  // Client messages should be sent to the root window announcing the
  // window manager's existence.
  MockXConnection::WindowInfo* root_info =
      xconn_->GetWindowInfoOrDie(xconn_->GetRootWindow());
  ASSERT_GE(root_info->client_messages.size(), 2);

  EXPECT_EQ(ClientMessage, root_info->client_messages[0].type);
  EXPECT_EQ(manager_atom, root_info->client_messages[0].message_type);
  EXPECT_EQ(XConnection::kLongFormat, root_info->client_messages[0].format);
  EXPECT_EQ(wm_atom, root_info->client_messages[0].data.l[1]);

  EXPECT_EQ(ClientMessage, root_info->client_messages[1].type);
  EXPECT_EQ(manager_atom, root_info->client_messages[1].message_type);
  EXPECT_EQ(XConnection::kLongFormat, root_info->client_messages[1].format);
  EXPECT_EQ(cm_atom, root_info->client_messages[1].data.l[1]);
}

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
  EXPECT_LT(stage->GetStackingIndex(win2->actor()),
            stage->GetStackingIndex(win->actor()));

  // Now send a message saying that the first window is on top of the second.
  MockXConnection::InitConfigureNotifyEvent(&event, *info);
  event.xconfigure.above = xid2;
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_LT(stage->GetStackingIndex(win->actor()),
            stage->GetStackingIndex(win2->actor()));
}

// Test that we honor ConfigureRequest events that change an unmapped
// window's size, and that we ignore fields that are unset in its
// 'value_mask' field.
TEST_F(WindowManagerTest, ConfigureRequestResize) {
  XWindow xid = CreateSimpleWindow(xconn_->GetRootWindow());
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  const int orig_width = info->width;
  const int orig_height = info->height;

  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Send a ConfigureRequest event with its width and height fields masked
  // out, and check that the new width and height values are ignored.
  const int new_width = orig_width * 2;
  const int new_height = orig_height * 2;
  MockXConnection::InitConfigureRequestEvent(
      &event, xid, info->x, info->y, new_width, new_height);
  event.xconfigurerequest.value_mask &= ~(CWWidth | CWHeight);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(orig_width, info->width);
  EXPECT_EQ(orig_height, info->height);

  // Now turn on the width bit and check that it gets applied.
  event.xconfigurerequest.value_mask |= CWWidth;
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(new_width, info->width);
  EXPECT_EQ(orig_height, info->height);

  // Turn on the height bit as well.
  event.xconfigurerequest.value_mask |= CWHeight;
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(new_width, info->width);
  EXPECT_EQ(new_height, info->height);
}

TEST_F(WindowManagerTest, XRandR) {
  // Make sure that the WM is selecting RRScreenChangeNotify events on the
  // root window.
  XWindow root_xid = xconn_->GetRootWindow();
  MockXConnection::WindowInfo* root_info = xconn_->GetWindowInfoOrDie(root_xid);
  EXPECT_TRUE(root_info->xrandr_events_selected);

  int new_width = root_info->width / 2;
  int new_height = root_info->height / 2;

  // Resize the root and compositing overlay windows to half their size.
  root_info->width = new_width;
  root_info->height = new_height;
  MockXConnection::WindowInfo* composite_info =
      xconn_->GetWindowInfoOrDie(xconn_->GetCompositingOverlayWindow(root_xid));
  composite_info->width = new_width;
  composite_info->height = new_height;

  // Send the WM an event saying that the screen has been resized.
  XEvent event;
  XRRScreenChangeNotifyEvent* xrandr_event =
      reinterpret_cast<XRRScreenChangeNotifyEvent*>(&event);
  xrandr_event->type = xconn_->xrandr_event_base() + RRScreenChangeNotify;
  xrandr_event->window = root_xid;
  xrandr_event->root = root_xid;
  xrandr_event->width = new_width;
  xrandr_event->height = new_height;
  EXPECT_TRUE(wm_->HandleEvent(&event));

  EXPECT_EQ(new_width, wm_->width());
  EXPECT_EQ(new_height, wm_->height());
  EXPECT_EQ(new_width, wm_->stage()->GetWidth());
  EXPECT_EQ(new_height, wm_->stage()->GetHeight());

  // Check that the window manager passed the new dimensions to some of the
  // objects that it owns.  The panel bar shouldn't be visible since there
  // are no panels (and as a result, the layout manager should be taking up
  // the entire screen).
  EXPECT_FALSE(wm_->panel_bar_->is_visible());

  EXPECT_EQ(0, wm_->layout_manager_->x());
  EXPECT_EQ(0, wm_->layout_manager_->y());
  EXPECT_EQ(new_width, wm_->layout_manager_->width());
  EXPECT_EQ(new_height, wm_->layout_manager_->height());

  EXPECT_EQ(0, wm_->panel_bar_->x());
  EXPECT_EQ(new_height - WindowManager::kPanelBarHeight, wm_->panel_bar_->y());
  EXPECT_EQ(new_width, wm_->panel_bar_->width());
  EXPECT_EQ(WindowManager::kPanelBarHeight, wm_->panel_bar_->height());
}

}  // namespace chromeos

int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
