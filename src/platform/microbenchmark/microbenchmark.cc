// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "microbenchmark/microbenchmark.h"

namespace chromeos {
// Commandline switch used to override the default number of runs for all
// tests.
const char *Microbenchmark::kRunsSwitch = "microbenchmark-runs";

void Microbenchmark::Run(uint64 number_of_runs) {
  runs_ = number_of_runs;
  Setup(number_of_runs);

  uint64 current_run = runs_;
  struct timespec start_time;
  struct timespec stop_time;
  // First we time the scaffolding.
  clock_gettime(CLOCK_REALTIME, &start_time);
  while (current_run--) {
    SingleTest(true);
  }
  clock_gettime(CLOCK_REALTIME, &stop_time);
  scaffold_total_ns_ +=
    ((stop_time.tv_sec - start_time.tv_sec) * 1000000000ULL) +
    (stop_time.tv_nsec - start_time.tv_nsec);
  scaffold_per_run_ns_ = scaffold_total_ns_ / runs_;
  // Now the real deal.
  current_run = runs_;
  clock_gettime(CLOCK_REALTIME, &start_time);
  while (current_run--) {
    SingleTest(false);
  }
  clock_gettime(CLOCK_REALTIME, &stop_time);
  total_ns_ += ((stop_time.tv_sec - start_time.tv_sec) * 1000000000ULL) +
               (stop_time.tv_nsec - start_time.tv_nsec);
  per_run_ns_ = total_ns_ / runs_;
}

void Microbenchmark::Print() const {
  LOG(WARNING) << "All measurements in nanoseconds";
  LOG(WARNING) << "Numbers may overflow and may not be statistically "
               << "meaningful.";
  std::cout << "MB:name,runs,total_ns,per_run_ns\nMB:"
            << name() << "-scaffold,"
            << runs() << ","
            << scaffold_total_nanoseconds() << ","
            << scaffold_per_run_nanoseconds() << "\nMB:"
            << name() << ","
            << runs() << ","
            << total_nanoseconds() << ","
            << per_run_nanoseconds() << "\nMB:"
            << name() << "-adjusted,"
            << runs() << ","
            << total_nanoseconds() - scaffold_total_nanoseconds() << ","
            << per_run_nanoseconds() - scaffold_per_run_nanoseconds()
            << "\n";
}

// Hide away helper functions here.
namespace microbenchmark_helper {
// Empty setup function.
void NoSetup(uint64) { }

}  // namespace microbenchmark_helper
}  // namespace chromeos

