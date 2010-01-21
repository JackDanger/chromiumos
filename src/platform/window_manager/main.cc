// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>
#include <cstdlib>
#include <ctime>

#include <unistd.h>

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
#ifdef USE_BREAKPAD
#include "handler/exception_handler.h"
#endif
#include "window_manager/clutter_interface.h"
#ifdef NO_CLUTTER
#include "window_manager/no_clutter.h"
#include "window_manager/real_gl_interface.h"
#endif
#include "window_manager/real_x_connection.h"
#include "window_manager/window_manager.h"

DECLARE_bool(wm_use_compositing);  // from window_manager.cc

DEFINE_string(log_dir, ".",
              "Directory where logs should be written; created if it doesn't "
              "exist.");
DEFINE_string(display, "",
              "X Display to connect to (overrides DISPLAY env var).");
DEFINE_bool(logtostderr, false,
            "Write logs to stderr instead of to a file in log_dir.");
DEFINE_string(minidump_dir, ".",
              "Directory where crash minidumps should be written; created if "
              "it doesn't exist.");
#ifdef NO_CLUTTER
DEFINE_bool(use_clutter, true,
            "Specify --nouse_clutter turn off clutter and use GL directly.");
#endif
DEFINE_int32(pause_at_start, 0,
             "Specify this to pause for N seconds at startup.");

using window_manager::ClutterInterface;
using window_manager::MockClutterInterface;
#ifdef NO_CLUTTER
using window_manager::NoClutterInterface;
#endif
using window_manager::RealClutterInterface;
#ifdef NO_CLUTTER
using window_manager::RealGLInterface;
#endif
using window_manager::RealXConnection;
using window_manager::WindowManager;

// Get the current time in the local time zone as "YYYYMMDD-HHMMSS".
static std::string GetCurrentTimeAsString() {
  time_t now = time(NULL);
  CHECK(now >= 0);
  struct tm now_tm;
  CHECK(localtime_r(&now, &now_tm) == &now_tm);
  char now_str[16];
  CHECK(strftime(now_str, sizeof(now_str), "%Y%m%d-%H%M%S", &now_tm) == 15);
  return std::string(now_str);
}

// Handler called by Chrome logging code on failed asserts.
static void HandleLogAssert(const std::string& str) {
  abort();
}

int main(int argc, char** argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (!FLAGS_display.empty()) {
    setenv("DISPLAY", FLAGS_display.c_str(), 1);
  }

  gdk_init(&argc, &argv);
#ifdef NO_CLUTTER
  if (FLAGS_use_clutter) {
#else
  if (1) {
#endif
    clutter_init(&argc, &argv);
  }

  CommandLine::Init(argc, argv);
  if (FLAGS_pause_at_start > 0) {
    ::sleep(FLAGS_pause_at_start);
  }

#ifdef USE_BREAKPAD
  if (!file_util::CreateDirectory(FilePath(FLAGS_minidump_dir)))
    LOG(ERROR) << "Unable to create minidump directory " << FLAGS_minidump_dir;
  google_breakpad::ExceptionHandler exception_handler(
      FLAGS_minidump_dir, NULL, NULL, NULL, true);
#endif

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

  // Chrome's logging code uses int3 to send SIGTRAP in response to failed
  // asserts, but Breakpad only installs signal handlers for SEGV, ABRT,
  // FPE, ILL, and BUS.  Use our own function to send ABRT instead.
  logging::SetLogAssertHandler(HandleLogAssert);

  RealXConnection xconn(GDK_DISPLAY());

  // Create the overlay window as soon as possible, to reduce the chances that
  // Chrome will be able to map a window before we've taken over.
  if (FLAGS_wm_use_compositing) {
    XWindow root = xconn.GetRootWindow();
    xconn.GetCompositingOverlayWindow(root);
  }

#ifdef NO_CLUTTER
  scoped_ptr<RealGLInterface> gl_interface;
#endif
  scoped_ptr<ClutterInterface> clutter;
  if (FLAGS_wm_use_compositing) {
#ifdef NO_CLUTTER
    if (FLAGS_use_clutter) {
#endif
      clutter.reset(new RealClutterInterface());
#ifdef NO_CLUTTER
    } else {
      gl_interface.reset(new RealGLInterface(&xconn));
      clutter.reset(new NoClutterInterface(&xconn, gl_interface.get()));
    }
#endif
  } else {
    clutter.reset(new MockClutterInterface(&xconn));
  }
  WindowManager wm(&xconn, clutter.get());
  wm.Init();

#ifdef NO_CLUTTER
  if (FLAGS_use_clutter) {
#endif
    clutter_main();
#ifdef NO_CLUTTER
  } else {
    GMainLoop* loop = g_main_loop_ref(g_main_loop_new(NULL, FALSE));
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
  }
#endif
  return 0;
}
