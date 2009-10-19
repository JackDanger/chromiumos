// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdlib>

extern "C" {
#include <clutter/clutter.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
}
#include <gflags/gflags.h>

#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/window_manager.h"
#include "window_manager/real_x_connection.h"

DECLARE_bool(wm_use_compositing);  // from window_manager.cc

using chromeos::ClutterInterface;
using chromeos::MockClutterInterface;
using chromeos::RealClutterInterface;
using chromeos::RealXConnection;
using chromeos::WindowManager;

int main(int argc, char** argv) {
  gdk_init(&argc, &argv);
  clutter_init(&argc, &argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);

  RealXConnection xconn(GDK_DISPLAY());

  // Create the overlay window as soon as possible, to reduce the chances that
  // Chrome will be able to map a window before we've taken over.
  if (FLAGS_wm_use_compositing) {
    XWindow root = xconn.GetRootWindow();
    xconn.GetCompositingOverlayWindow(root);
  }

  scoped_ptr<ClutterInterface> clutter;
  if (FLAGS_wm_use_compositing) {
    clutter.reset(new RealClutterInterface);
  } else {
    clutter.reset(new MockClutterInterface);
  }
  WindowManager wm(&xconn, clutter.get());
  wm.Init();

  clutter_main();
  return 0;
}
