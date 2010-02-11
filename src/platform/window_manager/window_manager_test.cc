// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdarg>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/layout_manager.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/panel_bar.h"
#include "window_manager/panel_manager.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class WindowManagerTest : public BasicWindowManagerTest {};

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
  bool HandleButtonPress(XWindow xid,
                         int x, int y,
                         int x_root, int y_root,
                         int button,
                         Time timestamp) {
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
      xconn_->GetStringProperty(wm_->wm_xid_, title_atom, &window_title));
  EXPECT_EQ(WindowManager::GetWmName(), window_title);

  // Check that the window and compositing manager selections are owned by
  // the window manager's window.
  XAtom wm_atom = None, cm_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("WM_S0", &wm_atom));
  ASSERT_TRUE(xconn_->GetAtom("_NET_WM_CM_S0", &cm_atom));
  EXPECT_EQ(wm_->wm_xid_, xconn_->GetSelectionOwner(wm_atom));
  EXPECT_EQ(wm_->wm_xid_, xconn_->GetSelectionOwner(cm_atom));

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
  EXPECT_EQ(wm_->wm_xid_, root_info->client_messages[0].data.l[2]);

  EXPECT_EQ(ClientMessage, root_info->client_messages[1].type);
  EXPECT_EQ(manager_atom, root_info->client_messages[1].message_type);
  EXPECT_EQ(XConnection::kLongFormat, root_info->client_messages[1].format);
  EXPECT_EQ(cm_atom, root_info->client_messages[1].data.l[1]);
  EXPECT_EQ(wm_->wm_xid_, root_info->client_messages[0].data.l[2]);
}

// Test different race conditions where a client window is created and/or
// mapped while WindowManager::Init() is running.
TEST_F(WindowManagerTest, ExistingWindows) {
  // First, test the case where a window has already been mapped before the
  // WindowManager object is initialized, so no CreateNotify or MapNotify
  // event is sent.
  wm_.reset(NULL);
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface(xconn_.get()));
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  xconn_->MapWindow(xid);

  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());
  Window* win = wm_->GetWindowOrDie(xid);
  EXPECT_TRUE(win->mapped());
  EXPECT_TRUE(dynamic_cast<const MockClutterInterface::Actor*>(
                  win->actor())->visible());

  // Now handle the case where the window starts out unmapped and
  // WindowManager misses the CreateNotify event but receives the
  // MapRequest (and subsequent MapNotify).
  wm_.reset(NULL);
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface(xconn_.get()));
  xid = CreateSimpleWindow();
  info = xconn_->GetWindowInfoOrDie(xid);

  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());
  EXPECT_FALSE(info->mapped);
  win = wm_->GetWindowOrDie(xid);
  EXPECT_FALSE(win->mapped());
  EXPECT_FALSE(dynamic_cast<const MockClutterInterface::Actor*>(
                   win->actor())->visible());

  XEvent event;
  MockXConnection::InitMapRequestEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(info->mapped);

  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->mapped());
  EXPECT_TRUE(dynamic_cast<const MockClutterInterface::Actor*>(
                  win->actor())->visible());

  // Here, we mimic the case where the window is created after
  // WindowManager selects SubstructureRedirect but before it queries for
  // existing windows, so it sees the window immediately but also gets a
  // CreateNotify event about it.
  wm_.reset(NULL);
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface(xconn_.get()));
  xid = CreateSimpleWindow();
  info = xconn_->GetWindowInfoOrDie(xid);

  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());
  EXPECT_FALSE(info->mapped);
  win = wm_->GetWindowOrDie(xid);
  EXPECT_FALSE(win->mapped());
  EXPECT_FALSE(dynamic_cast<const MockClutterInterface::Actor*>(
                   win->actor())->visible());

  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_FALSE(wm_->HandleEvent(&event));  // false because it's already known

  MockXConnection::InitMapRequestEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(info->mapped);

  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->mapped());
  EXPECT_TRUE(dynamic_cast<const MockClutterInterface::Actor*>(
                  win->actor())->visible());

  // Finally, test the typical case where a window is created after
  // WindowManager has been initialized.
  wm_.reset(NULL);
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface(xconn_.get()));
  xid = None;
  info = NULL;

  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());

  xid = CreateSimpleWindow();
  info = xconn_->GetWindowInfoOrDie(xid);

  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(info->mapped);
  win = wm_->GetWindowOrDie(xid);
  EXPECT_FALSE(win->mapped());
  EXPECT_FALSE(dynamic_cast<const MockClutterInterface::Actor*>(
                   win->actor())->visible());

  MockXConnection::InitMapRequestEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(info->mapped);
  EXPECT_FALSE(win->mapped());

  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->mapped());
  EXPECT_TRUE(dynamic_cast<const MockClutterInterface::Actor*>(
                  win->actor())->visible());
}

// Test that we display override-redirect windows onscreen regardless of
// whether they're mapped or not by the time that we learn about them.
TEST_F(WindowManagerTest, OverrideRedirectMapping) {
  // Test the case where a client has already mapped an override-redirect
  // window by the time that we receive the CreateNotify event about it.
  // We should still pay attention to the MapNotify event that comes
  // afterwards and display the window.
  XWindow xid = xconn_->CreateWindow(
        xconn_->GetRootWindow(),
        10, 20,  // x, y
        30, 40,  // width, height
        true,    // override redirect
        false,   // input only
        0);      // event mask
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  xconn_->MapWindow(xid);
  ASSERT_TRUE(info->mapped);

  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Now test the other possibility, where the window isn't mapped on the X
  // server yet when we receive the CreateNotify event.
  Window* win = wm_->GetWindowOrDie(xid);
  EXPECT_TRUE(dynamic_cast<const MockClutterInterface::Actor*>(
                  win->actor())->visible());

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
  ASSERT_TRUE(info2->mapped);
  MockXConnection::InitMapEvent(&event, xid2);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  Window* win2 = wm_->GetWindowOrDie(xid2);
  EXPECT_TRUE(dynamic_cast<const MockClutterInterface::Actor*>(
                  win2->actor())->visible());
}

TEST_F(WindowManagerTest, InputWindows) {
  // Check that CreateInputWindow() creates windows as requested.
  int event_mask = ButtonPressMask | ButtonReleaseMask;
  XWindow xid = wm_->CreateInputWindow(100, 200, 300, 400, event_mask);
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfo(xid);
  ASSERT_TRUE(info != NULL);
  EXPECT_EQ(100, info->x);
  EXPECT_EQ(200, info->y);
  EXPECT_EQ(300, info->width);
  EXPECT_EQ(400, info->height);
  EXPECT_EQ(true, info->mapped);
  EXPECT_EQ(true, info->override_redirect);
  EXPECT_EQ(event_mask, info->event_mask);

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

  MockXConnection::InitButtonPressEvent(&event, *info, 5, 5, 1);
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
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  ASSERT_FALSE(info->redirected);

  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  // The window shouldn't be redirected yet, since it hasn't been mapped.
  EXPECT_FALSE(info->redirected);

  // After we send a map request, the window should be redirected.
  MockXConnection::InitMapRequestEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(info->mapped);
  EXPECT_TRUE(info->redirected);

  // Finally, let the window manager know that the window has been mapped.
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

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
  XWindow xid = CreateSimpleWindow();
  SendInitialEventsForWindow(xid);

  Window* win = wm_->GetWindowOrDie(xid);
  EXPECT_TRUE(win->focused());

  // We should ignore a focus-out event caused by a grab...
  XEvent event;
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyGrab, NotifyNonlinear);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());

  // ... but honor one that comes in independently from a grab.
  MockXConnection::InitFocusOutEvent(
      &event, xid, NotifyNormal, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(win->focused());

  // Similarly, we should ignore a focus-in event caused by an ungrab...
  MockXConnection::InitFocusInEvent(
      &event, xid, NotifyUngrab, NotifyNonlinear);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_FALSE(win->focused());

  // ... but honor one that comes in independently.
  MockXConnection::InitFocusInEvent(
      &event, xid, NotifyNormal, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());

  // We should pay attention to events that come in while a grab is already
  // active, though.
  MockXConnection::InitFocusOutEvent(
      &event, xid, NotifyWhileGrabbed, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_FALSE(win->focused());
  MockXConnection::InitFocusInEvent(
      &event, xid, NotifyWhileGrabbed, NotifyNonlinear);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());

  // Events with a detail of NotifyPointer should be ignored.
  MockXConnection::InitFocusOutEvent(&event, xid, NotifyNormal, NotifyPointer);
  EXPECT_FALSE(wm_->HandleEvent(&event));
  EXPECT_TRUE(win->focused());
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
  Window* win = wm_->GetWindowOrDie(xid);

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
  Window* win2 = wm_->GetWindowOrDie(xid2);

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
  XWindow xid = CreateSimpleWindow();
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

TEST_F(WindowManagerTest, RandR) {
  // Look up EWMH atoms relating to the screen size.
  XAtom geometry_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("_NET_DESKTOP_GEOMETRY", &geometry_atom));
  XAtom workarea_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("_NET_WORKAREA", &workarea_atom));

  // Make sure that the WM is selecting RRScreenChangeNotify events on the
  // root window.
  XWindow root_xid = xconn_->GetRootWindow();
  MockXConnection::WindowInfo* root_info = xconn_->GetWindowInfoOrDie(root_xid);
  EXPECT_TRUE(root_info->randr_events_selected);

  // EWMH size properties should also be set correctly.
  TestIntArrayProperty(root_xid, geometry_atom, 2,
                       root_info->width, root_info->height);
  TestIntArrayProperty(root_xid, workarea_atom, 4,
                       0, 0, root_info->width,
                       root_info->height - WindowManager::kPanelBarHeight);

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
  XRRScreenChangeNotifyEvent* randr_event =
      reinterpret_cast<XRRScreenChangeNotifyEvent*>(&event);
  randr_event->type = xconn_->randr_event_base() + RRScreenChangeNotify;
  randr_event->window = root_xid;
  randr_event->root = root_xid;
  randr_event->width = new_width;
  randr_event->height = new_height;
  EXPECT_TRUE(wm_->HandleEvent(&event));

  EXPECT_EQ(new_width, wm_->width());
  EXPECT_EQ(new_height, wm_->height());
  EXPECT_EQ(new_width, wm_->stage()->GetWidth());
  EXPECT_EQ(new_height, wm_->stage()->GetHeight());

  EXPECT_EQ(0, wm_->layout_manager_->x());
  EXPECT_EQ(0, wm_->layout_manager_->y());
  EXPECT_EQ(new_width, wm_->layout_manager_->width());
  EXPECT_EQ(new_height, wm_->layout_manager_->height());

  // EWMH properties on the root window should be updated as well.
  TestIntArrayProperty(root_xid, geometry_atom, 2, new_width, new_height);
  TestIntArrayProperty(root_xid, workarea_atom, 4,
                       0, 0, new_width,
                       new_height - WindowManager::kPanelBarHeight);

  // The background window should be resized too.
  MockXConnection::WindowInfo* background_info =
      xconn_->GetWindowInfoOrDie(wm_->background_xid());
  EXPECT_EQ(0, background_info->x);
  EXPECT_EQ(0, background_info->y);
  EXPECT_EQ(new_width, background_info->width);
  EXPECT_EQ(new_height, background_info->height);
}

// Test that the _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING properties
// on the root window get updated correctly.
TEST_F(WindowManagerTest, ClientListProperties) {
  XWindow root_xid = xconn_->GetRootWindow();
  XAtom list_atom = None, stacking_atom = None;
  ASSERT_TRUE(xconn_->GetAtom("_NET_CLIENT_LIST", &list_atom));
  ASSERT_TRUE(xconn_->GetAtom("_NET_CLIENT_LIST_STACKING", &stacking_atom));

  // Both properties should be unset when there aren't any client windows.
  TestIntArrayProperty(root_xid, list_atom, 0);
  TestIntArrayProperty(root_xid, stacking_atom, 0);

  // Create and map a regular window.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  SendInitialEventsForWindow(xid);

  // Both properties should contain just this window.
  TestIntArrayProperty(root_xid, list_atom, 1, xid);
  TestIntArrayProperty(root_xid, stacking_atom, 1, xid);

  // Create and map an override-redirect window.
  XWindow override_redirect_xid =
      xconn_->CreateWindow(
          root_xid,  // parent
          0, 0,      // x, y
          200, 200,  // width, height
          true,      // override_redirect
          false,     // input_only
          0);        // event_mask
  MockXConnection::WindowInfo* override_redirect_info =
      xconn_->GetWindowInfoOrDie(override_redirect_xid);
  SendInitialEventsForWindow(override_redirect_xid);

  // The override-redirect window shouldn't be included.
  TestIntArrayProperty(root_xid, list_atom, 1, xid);
  TestIntArrayProperty(root_xid, stacking_atom, 1, xid);

  // Create and map a second regular window.
  XWindow xid2 = CreateSimpleWindow();
  SendInitialEventsForWindow(xid2);

  // The second window should appear after the first in _NET_CLIENT_LIST,
  // since it was mapped after it, and after the first in
  // _NET_CLIENT_LIST_STACKING, since it's stacked above it (new windows
  // get stacked above their siblings).
  TestIntArrayProperty(root_xid, list_atom, 2, xid, xid2);
  TestIntArrayProperty(root_xid, stacking_atom, 2, xid, xid2);

  // Raise the override-redirect window above the others.
  ASSERT_TRUE(xconn_->RaiseWindow(override_redirect_xid));
  XEvent event;
  MockXConnection::InitConfigureNotifyEvent(&event, *override_redirect_info);
  event.xconfigure.above = xid2;
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The properties should be unchanged.
  TestIntArrayProperty(root_xid, list_atom, 2, xid, xid2);
  TestIntArrayProperty(root_xid, stacking_atom, 2, xid, xid2);

  // Raise the first window on top of the second window.
  ASSERT_TRUE(xconn_->StackWindow(xid, xid2, true));
  MockXConnection::InitConfigureNotifyEvent(&event, *info);
  event.xconfigure.above = xid2;
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The list property should be unchanged, but the second window should
  // appear first in the stacking property since it's now on the bottom.
  TestIntArrayProperty(root_xid, list_atom, 2, xid, xid2);
  TestIntArrayProperty(root_xid, stacking_atom, 2, xid2, xid);

  // Destroy the first window.
  ASSERT_TRUE(xconn_->DestroyWindow(xid));
  MockXConnection::InitUnmapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitDestroyWindowEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Both properties should just contain the second window now.
  TestIntArrayProperty(root_xid, list_atom, 1, xid2);
  TestIntArrayProperty(root_xid, stacking_atom, 1, xid2);

  // Tell the window manager that the second window was reparented away.
  XReparentEvent* reparent_event = &(event.xreparent);
  memset(reparent_event, 0, sizeof(*reparent_event));
  reparent_event->type = ReparentNotify;
  reparent_event->window = xid2;
  reparent_event->parent = 324324;  // arbitrary number
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The properties should be unset.
  TestIntArrayProperty(root_xid, list_atom, 0);
  TestIntArrayProperty(root_xid, stacking_atom, 0);
}

TEST_F(WindowManagerTest, WmIpcVersion) {
  // BasicWindowManagerTest::SetUp() sends a WM_NOTIFY_IPC_VERSION message
  // automatically, since most tests want something reasonable there.
  // Create a new WindowManager object to work around this.
  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  ASSERT_TRUE(wm_->Init());

  // We should assume version 0 if we haven't received a message from Chrome.
  EXPECT_EQ(0, wm_->wm_ipc_version());

  // Now send the WM a message telling it that Chrome is using version 3.
  WmIpc::Message msg(WmIpc::Message::WM_NOTIFY_IPC_VERSION);
  msg.set_param(0, 3);
  XEvent event;
  wm_->wm_ipc()->FillXEventFromMessage(&event, wm_->wm_xid(), msg);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_EQ(3, wm_->wm_ipc_version());
}

// Test that we defer redirection of client windows until we see them getting
// mapped (and also that we redirect windows that were already mapped at
// startup).
TEST_F(WindowManagerTest, DeferRedirection) {
  // First, create a window that's already mapped when the window manager is
  // started.
  wm_.reset(NULL);
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface(xconn_.get()));
  XWindow existing_xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* existing_info =
      xconn_->GetWindowInfoOrDie(existing_xid);
  xconn_->MapWindow(existing_xid);
  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());

  // Check that the window manager redirected it.
  EXPECT_TRUE(existing_info->redirected);
  Window* existing_win = wm_->GetWindowOrDie(existing_xid);
  EXPECT_TRUE(existing_win->redirected());
  MockClutterInterface::TexturePixmapActor* existing_mock_actor =
      dynamic_cast<MockClutterInterface::TexturePixmapActor*>(
          existing_win->actor());
  CHECK(existing_mock_actor);
  EXPECT_EQ(existing_xid, existing_mock_actor->xid());

  // Now, create a new window, but don't map it yet.
  XWindow xid = CreateSimpleWindow();
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // The window shouldn't be redirected initially.
  EXPECT_FALSE(info->redirected);
  Window* win = wm_->GetWindowOrDie(xid);
  EXPECT_FALSE(win->redirected());
  MockClutterInterface::TexturePixmapActor* mock_actor =
      dynamic_cast<MockClutterInterface::TexturePixmapActor*>(win->actor());
  CHECK(mock_actor);
  EXPECT_EQ(None, mock_actor->xid());

  // After we send a MapRequest event, the window should be mapped and
  // redirected.
  MockXConnection::InitMapRequestEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  EXPECT_TRUE(info->mapped);
  EXPECT_TRUE(info->redirected);
  EXPECT_TRUE(win->redirected());
  EXPECT_EQ(xid, mock_actor->xid());

  // Finally, let the window manager know that the window has been mapped.
  MockXConnection::InitMapEvent(&event, xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // There won't be a MapRequest event for override-redirect windows, but they
  // should still get redirected in response to the MapNotify.
  XWindow override_redirect_xid = xconn_->CreateWindow(
        xconn_->GetRootWindow(),
        10, 20,  // x, y
        30, 40,  // width, height
        true,    // override redirect
        false,   // input only
        0);      // event mask
  MockXConnection::WindowInfo* override_redirect_info =
      xconn_->GetWindowInfoOrDie(override_redirect_xid);
  xconn_->MapWindow(override_redirect_xid);
  ASSERT_TRUE(override_redirect_info->mapped);

  // Send CreateNotify and MapNotify events to the window manager.
  MockXConnection::InitCreateWindowEvent(&event, *override_redirect_info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  MockXConnection::InitMapEvent(&event, override_redirect_xid);
  EXPECT_TRUE(wm_->HandleEvent(&event));

  // Now check that it's redirected.
  EXPECT_TRUE(override_redirect_info->redirected);
  Window* override_redirect_win = wm_->GetWindowOrDie(override_redirect_xid);
  EXPECT_TRUE(override_redirect_win->redirected());
  MockClutterInterface::TexturePixmapActor* override_redirect_mock_actor =
      dynamic_cast<MockClutterInterface::TexturePixmapActor*>(
          override_redirect_win->actor());
  CHECK(override_redirect_mock_actor);
  EXPECT_EQ(override_redirect_xid, override_redirect_mock_actor->xid());
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
