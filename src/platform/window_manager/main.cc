// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdlib>
#include <ctime>

extern "C" {
#include <clutter/clutter.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
}

#include <gflags/gflags.h>

#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/window_manager.h"
#include "window_manager/real_x_connection.h"

DECLARE_bool(wm_use_compositing);  // from window_manager.cc

DEFINE_string(log_dir, ".",
              "Directory where logs should be written; created if it doesn't "
              "exist.");
DEFINE_bool(logtostderr, false,
            "Should logs be written to stderr instead of to a file in "
            "--log_dir?");

using chromeos::ClutterInterface;
using chromeos::MockClutterInterface;
using chromeos::RealClutterInterface;
using chromeos::RealXConnection;
using chromeos::WindowManager;

// Get the current time in the local time zone as "YYYYMMDD-HHMMSS".
std::string GetCurrentTimeAsString() {
  time_t now = time(NULL);
  CHECK(now >= 0);
  struct tm now_tm;
  CHECK(localtime_r(&now, &now_tm) == &now_tm);
  char now_str[16];
  CHECK(strftime(now_str, sizeof(now_str), "%Y%m%d-%H%M%S", &now_tm) == 15);
  return std::string(now_str);
}

int main(int argc, char** argv) {
  gdk_init(&argc, &argv);
  clutter_init(&argc, &argv);
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);

  if (!FLAGS_logtostderr) {
    if (!file_util::CreateDirectory(FilePath(FLAGS_log_dir)))
      LOG(ERROR) << "Unable to create logging directory " << FLAGS_log_dir;
  }
  std::string log_filename = StringPrintf(
      "%s/%s.%s", FLAGS_log_dir.c_str(), WindowManager::GetWmName(),
      GetCurrentTimeAsString().c_str());
  logging::InitLogging(log_filename.c_str(),
                       FLAGS_logtostderr ?
                         logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                         logging::LOG_ONLY_TO_FILE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);

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
