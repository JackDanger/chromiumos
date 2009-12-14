// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

extern "C" {
#include <X11/Xlib.h>
}

#include "base/scoped_ptr.h"
#include "base/logging.h"

typedef ::Atom XAtom;
typedef ::Window XWindow;

namespace chromeos {

class MockXConnection;
class MockClutterInterface;
class WindowManager;

// Test that two bytes sequences are equal, pretty-printing the difference
// otherwise.  Invoke as:
//
//   EXPECT_PRED_FORMAT3(BytesAreEqual, expected, actual, length);
//
testing::AssertionResult BytesAreEqual(
    const char* expected_expr,
    const char* actual_expr,
    const char* size_expr,
    const unsigned char* expected,
    const unsigned char* actual,
    size_t size);

// Called from tests' main() functions to handle a bunch of boilerplate.
// Its return value should be returned from main().  We initialize the
// flag-parsing code, so if the caller wants to set 'log_to_stderr' based
// on a flag, a pointer to the flag's variable should be passed here (e.g.
// '&FLAGS_logtostderr').
int InitAndRunTests(int* argc, char** argv, bool* log_to_stderr);

// A basic test that sets up fake X and Clutter interfaces and creates a
// WindowManager object.  Also includes several methods that tests can use
// for convenience.
class BasicWindowManagerTest : public ::testing::Test {
 protected:
  virtual void SetUp();

  // Create a toplevel client window with the passed-in position and
  // dimensions.
  XWindow CreateToplevelWindow(int x, int y, int width, int height);

  // Creates a toplevel client window with an arbitrary size.
  XWindow CreateSimpleWindow();

  // Create a panel titlebar or panel content window.
  XWindow CreateTitlebarWindow(int width, int height);
  XWindow CreatePanelWindow(
      int width, int height, XWindow titlebar_xid, bool expanded);

  // Make the window manager handle a CreateNotify event and, if the window
  // isn't override-redirect, a MapRequest.  If it's mapped after this
  // (expected if we sent a MapRequest), send a MapNotify event.
  void SendInitialEventsForWindow(XWindow xid);

  // Make the window manager handle FocusNotify events saying that the
  // focus was passed from 'out_xid' to 'in_xid'.  Events are only sent for
  // windows that are neither None nor the root window.
  void SendFocusEvents(XWindow out_xid, XWindow in_xid);

  // Get the current value of the _NET_ACTIVE_WINDOW property on the root
  // window.
  XWindow GetActiveWindowProperty();

  // Fetch an int array property on a window and check that it contains the
  // expected values.  'num_values' is the number of expected values passed
  // as varargs.
  void TestIntArrayProperty(XWindow xid, XAtom atom, int num_values, ...);

  scoped_ptr<MockXConnection> xconn_;
  scoped_ptr<MockClutterInterface> clutter_;
  scoped_ptr<WindowManager> wm_;
};

}  // namespace chromeos
