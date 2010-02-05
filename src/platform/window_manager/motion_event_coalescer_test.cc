// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "chromeos/callback.h"
#include "window_manager/motion_event_coalescer.h"
#include "window_manager/test_lib.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

using chromeos::NewPermanentCallback;

class MotionEventCoalescerTest : public ::testing::Test {
};

// Test against regression of some hard-to-hit-outside-of-testing bugs in
// this class where we would sometimes not send notifications after
// restarting the coalescer if the first values it received matched the
// last ones it'd seen before it was restarted.
TEST_F(MotionEventCoalescerTest, InitialValues) {
  TestCallbackCounter counter;
  MotionEventCoalescer coalescer(
      NewPermanentCallback(&counter, &TestCallbackCounter::Increment), 100);
  coalescer.set_synchronous(true);

  coalescer.Start();
  EXPECT_EQ(0, counter.num_calls());

  // We used to initialize the positions to (0, 0) instead of (-1, -1), so
  // we'd incorrectly ignore initial (0, 0) values.
  coalescer.StorePosition(0, 0);
  EXPECT_EQ(1, counter.num_calls());
  EXPECT_EQ(0, coalescer.x());
  EXPECT_EQ(0, coalescer.y());

  coalescer.StorePosition(200, 300);
  EXPECT_EQ(2, counter.num_calls());
  EXPECT_EQ(200, coalescer.x());
  EXPECT_EQ(300, coalescer.y());

  coalescer.Stop();
  EXPECT_EQ(2, counter.num_calls());

  coalescer.Start();
  EXPECT_EQ(2, counter.num_calls());

  // We should still notify if the first values that we receive after
  // restarting matched the last ones that we saw before.
  coalescer.StorePosition(200, 300);
  EXPECT_EQ(3, counter.num_calls());
  EXPECT_EQ(200, coalescer.x());
  EXPECT_EQ(300, coalescer.y());
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
