// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/test_lib.h"

#include <vector>

#include <gflags/gflags.h>

#include "base/command_line.h"
#include "base/string_util.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/window_manager.h"
#include "window_manager/wm_ipc.h"

namespace window_manager {

testing::AssertionResult BytesAreEqual(
    const char* expected_expr,
    const char* actual_expr,
    const char* size_expr,
    const unsigned char* expected,
    const unsigned char* actual,
    size_t size) {
  for (size_t i = 0; i < size; ++i) {
    if (expected[i] != actual[i]) {
      testing::Message msg;
      std::string expected_str, actual_str, hl_str;
      bool first = true;
      for (size_t j = 0; j < size; ++j) {
        expected_str +=
            StringPrintf(" %02x", static_cast<unsigned char>(expected[j]));
        actual_str +=
            StringPrintf(" %02x", static_cast<unsigned char>(actual[j]));
        hl_str += (expected[j] == actual[j]) ? "   " : " ^^";
        if ((j % 16) == 15 || j == size - 1) {
          msg << (first ? "Expected:" : "\n         ") << expected_str << "\n"
              << (first ? "  Actual:" : "         ") << actual_str << "\n"
              << "         " << hl_str;
          expected_str = actual_str = hl_str = "";
          first = false;
        }
      }
      return testing::AssertionFailure(msg);
    }
  }
  return testing::AssertionSuccess();
}

int InitAndRunTests(int* argc, char** argv, bool* log_to_stderr) {
  google::ParseCommandLineFlags(argc, &argv, true);
  CommandLine::Init(*argc, argv);
  logging::InitLogging(NULL,
                       (log_to_stderr && *log_to_stderr) ?
                         logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                         logging::LOG_NONE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  ::testing::InitGoogleTest(argc, argv);
  return RUN_ALL_TESTS();
}


void BasicWindowManagerTest::SetUp() {
  xconn_.reset(new MockXConnection);
  clutter_.reset(new MockClutterInterface(xconn_.get()));
  wm_.reset(new WindowManager(xconn_.get(), clutter_.get()));
  CHECK(wm_->Init());
}

XWindow BasicWindowManagerTest::CreateToplevelWindow(
    int x, int y, int width, int height) {
  return xconn_->CreateWindow(
      xconn_->GetRootWindow(),
      x, y,
      width, height,
      false,  // override redirect
      false,  // input only
      0);     // event mask
}

XWindow BasicWindowManagerTest::CreateSimpleWindow() {
  return CreateToplevelWindow(0, 0, 640, 480);
}

XWindow BasicWindowManagerTest::CreatePanelTitlebarWindow(
    int width, int height) {
  XWindow xid = CreateToplevelWindow(0, 0, width, height);
  wm_->wm_ipc()->SetWindowType(
      xid, WmIpc::WINDOW_TYPE_CHROME_PANEL_TITLEBAR, NULL);
  return xid;
}

XWindow BasicWindowManagerTest::CreatePanelContentWindow(
    int width, int height, XWindow titlebar_xid, bool expanded) {
  XWindow xid = CreateToplevelWindow(0, 0, width, height);
  std::vector<int> params;
  params.push_back(titlebar_xid);
  params.push_back(expanded ? 1 : 0);
  wm_->wm_ipc()->SetWindowType(
      xid, WmIpc::WINDOW_TYPE_CHROME_PANEL_CONTENT, &params);
  return xid;
}

void BasicWindowManagerTest::SendInitialEventsForWindow(XWindow xid) {
  MockXConnection::WindowInfo* info = xconn_->GetWindowInfoOrDie(xid);
  XEvent event;
  MockXConnection::InitCreateWindowEvent(&event, *info);
  EXPECT_TRUE(wm_->HandleEvent(&event));
  if (!info->override_redirect) {
    MockXConnection::InitMapRequestEvent(&event, *info);
    EXPECT_TRUE(wm_->HandleEvent(&event));
    EXPECT_TRUE(info->mapped);
  }
  if (info->mapped) {
    MockXConnection::InitMapEvent(&event, xid);
    EXPECT_TRUE(wm_->HandleEvent(&event));
  }
}

void BasicWindowManagerTest::SendFocusEvents(XWindow out_xid, XWindow in_xid) {
  XWindow root_xid = xconn_->GetRootWindow();

  XEvent event;
  if (out_xid != None && out_xid != root_xid) {
    MockXConnection::InitFocusOutEvent(
        &event, out_xid, NotifyNormal,
        (in_xid == root_xid) ? NotifyAncestor : NotifyNonlinear);
    EXPECT_TRUE(wm_->HandleEvent(&event));
  }
  if (in_xid != None && in_xid != root_xid) {
    MockXConnection::InitFocusInEvent(
        &event, in_xid, NotifyNormal,
        (out_xid == root_xid) ? NotifyAncestor : NotifyNonlinear);
    EXPECT_TRUE(wm_->HandleEvent(&event));
  }
}

XWindow BasicWindowManagerTest::GetActiveWindowProperty() {
  int active_window;
  if (!xconn_->GetIntProperty(xconn_->GetRootWindow(),
                              wm_->GetXAtom(ATOM_NET_ACTIVE_WINDOW),
                              &active_window)) {
    return None;
  }
  return active_window;
}

void BasicWindowManagerTest::TestIntArrayProperty(
    XWindow xid, XAtom atom, int num_values, ...) {
  std::vector<int> expected;

  va_list args;
  va_start(args, num_values);
  CHECK_GE(num_values, 0);
  for (; num_values; num_values--) {
    int arg = va_arg(args, int);
    expected.push_back(arg);
  }
  va_end(args);

  std::vector<int> actual;
  int exists = xconn_->GetIntArrayProperty(xid, atom, &actual);
  if (expected.empty()) {
    EXPECT_FALSE(exists);
  } else {
    EXPECT_TRUE(exists);
    ASSERT_EQ(expected.size(), actual.size());
    for (size_t i = 0; i < actual.size(); ++i)
      EXPECT_EQ(expected[i], actual[i]);
  }
}

}  // namespace window_manager
