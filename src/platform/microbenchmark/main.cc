// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "microbenchmark/microbenchmark.h"

#include <stdlib.h>

int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  ::testing::InitGoogleTest(&argc, argv);
  CommandLine *cl = CommandLine::ForCurrentProcess();
  if (cl->GetSwitchValueASCII(chromeos::Microbenchmark::kRunsSwitch).empty()) {
    LOG(INFO) << "Defaulting to the number of runs specified per test";
    LOG(INFO) << "To override, use --"
              << chromeos::Microbenchmark::kRunsSwitch
              << "=NUM";
  }
  return RUN_ALL_TESTS();
}
