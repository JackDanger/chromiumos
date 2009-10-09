// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// based on pam_google_testrunner.cc

#include <gtest/gtest.h>
#include <glib-object.h>
#include <glog/logging.h>

int main(int argc, char **argv) {
  ::g_type_init();

#if 0
  // Initializing with a name does not seem to be changing the log file name.
  // And options for logging to stderr are being ignored. If we don't initialize
  // logging goes to stderr by default.
  google::InitGoogleLogging("power_unittest");
#endif

  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
