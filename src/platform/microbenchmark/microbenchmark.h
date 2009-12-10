// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Simple microbenchmark framework

#ifndef __CHROMEOS_MICROBENCHMARK_MICROBENCHMARK_H
#define __CHROMEOS_MICROBENCHMARK_MICROBENCHMARK_H

#include <errno.h>
#include <time.h>

#include <iostream>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/scoped_ptr.h>
#include <gtest/gtest.h>

// CHROMEOS_MICROBENCHMARK_WITH_SETUP is the primary macro for
// using a microbenchmark from this framework.
//
// For quick use, create a new .cc file in your project.
// Include this header and create two static functions,
// one for setup and one for executing the test once.
// After both are defined, append:
//   CHROMEOS_MICROBENCHMARK_WITH_SETUP(MySetup, MyTest, 100000)
// The last argument is the default number of runs.  This may
// be overridden at run-time and, in the future, may be automatically
// tweaked to avoid measurement errors.
//
// The _NAME function should be of the prototype:
//   void MyTest(bool scaffold_only);
// The _SETUP_NAME function should be of the prototype:
//   void SetupMyTest(uint64 number_of_runs);
#define CHROMEOS_MICROBENCHMARK_WITH_SETUP(_SETUP_NAME, _NAME, _RUNS) \
class _NAME ## Class : public Microbenchmark { \
 public: \
  _NAME ## Class() {} \
  ~_NAME ## Class() {} \
  const char *name() const { return #_NAME; } \
  void Setup(uint64 runs) { _SETUP_NAME(runs); } \
  void SingleTest(bool scaffold_only) { _NAME(scaffold_only); } \
}; \
TEST(_NAME, Microbenchmark) { \
  _NAME ## Class chromeos_benchmark; \
  CommandLine *cl = CommandLine::ForCurrentProcess(); \
  errno = 0; \
  std::string runs_str = \
    cl->GetSwitchValueASCII(chromeos::Microbenchmark::kRunsSwitch); \
  unsigned long long runs = _RUNS; \
  if (!runs_str.empty()) { \
    errno = 0; \
    runs = strtoull(runs_str.c_str(), NULL, 0); \
    if (errno) \
      runs = _RUNS; \
   } \
  chromeos_benchmark.Run(runs); \
  chromeos_benchmark.Print(); \
}

// This is a shortcut macro.  If you don't need to setup any global state for
// use in your test, you can use this instead of _WITH_SETUP.
#define CHROMEOS_MICROBENCHMARK(_NAME, _RUNS) \
  CHROMEOS_MICROBENCHMARK_WITH_SETUP(chromeos::microbenchmark_helper::NoSetup, \
                                     _NAME, \
                                     _RUNS)
namespace chromeos {

namespace microbenchmark_helper {
void NoSetup(uint64 runs);
}  // microbenchmark_helper

// A simple microbenchmarking abstract class.
// This class is not thread-safe and should only be invoked
// from one thread at a time.
class Microbenchmark {
 public:
   Microbenchmark() : scaffold_total_ns_(0),
                      scaffold_per_run_ns_(0),
                      total_ns_(0),
                      per_run_ns_(0),
                      runs_(0) {}
   virtual ~Microbenchmark() {}
   // Switch to override the number of runs to perform.
   static const char *kRunsSwitch;
   // Performs the actual microbenchmarking.
   void Run(uint64 number_of_runs);
   // Outputs a standard format of the testing data to stdout.
   void Print() const;
   // Accessors
   const uint64 total_nanoseconds() const { return total_ns_; }
   const uint64 per_run_nanoseconds() const { return per_run_ns_; }
   const uint64 scaffold_total_nanoseconds() const
     { return scaffold_total_ns_; }
   const uint64 scaffold_per_run_nanoseconds() const
     { return scaffold_per_run_ns_; }
   const uint64 runs() const { return runs_; }

   //// Test code to be implemented by the class consumer.
   virtual const char *name() const = 0;
   // Called automatically before the benchmark.
   virtual void Setup(uint64 runs) = 0;
   // Should execute the test to benchmark once.
   virtual void SingleTest(bool scaffold_only) = 0;

 private:
  uint64 scaffold_total_ns_;
  uint64 scaffold_per_run_ns_;
  uint64 total_ns_;
  uint64 per_run_ns_;
  uint64 runs_;
  DISALLOW_COPY_AND_ASSIGN(Microbenchmark);
};

}  // chromeos
#endif  // __CHROMEOS_MICROBENCHMARK_MICROBENCHMARK_H
