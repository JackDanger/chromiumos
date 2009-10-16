// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define FUSE_USE_VERSION 26

#include <gflags/gflags.h>
#include <glog/logging.h>

#include <fuse.h>
#include <stdio.h>
#include <sys/prctl.h>

#include "fuse_bridge.h"


int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  // TODO: use google::InstallFailureFunction

  // TODO ensure this process is not ptrace-able when in production.
#ifdef NDEBUG
  prctl(PR_SET_DUMPABLE, 0, 0, 0, 0);
#endif

  umask(0);
  return fuse_main(argc, argv, chromeos_verity_operations, NULL);
}
