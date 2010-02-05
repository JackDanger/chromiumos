// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "chromeos/callback.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/pointer_position_watcher.h"
#include "window_manager/test_lib.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

using chromeos::NewPermanentCallback;

class PointerPositionWatcherTest : public ::testing::Test {
};

// Struct that contains a watcher and has a method to delete it.
// Used by the DeleteFromCallback test.
struct WatcherContainer {
  void set_watcher(PointerPositionWatcher* new_watcher) {
    watcher.reset(new_watcher);
  }
  scoped_ptr<PointerPositionWatcher> watcher;
};

TEST_F(PointerPositionWatcherTest, Basic) {
  MockXConnection xconn;
  xconn.SetPointerPosition(0, 0);

  // Watch for the pointer moving into a 20x30 rectangle at (50, 100).
  TestCallbackCounter counter;
  scoped_ptr<PointerPositionWatcher> watcher(
      new PointerPositionWatcher(
          &xconn,
          NewPermanentCallback(&counter, &TestCallbackCounter::Increment),
          true,      // watch_for_entering_target
          50, 100,   // x, y
          20, 30));  // width, height
  EXPECT_NE(0, watcher->timer_id());

  // Check that the callback doesn't get run and the timer stays active as
  // long as the pointer is outside of the rectangle.
  watcher->TriggerTimeout();
  EXPECT_EQ(0, counter.num_calls());
  EXPECT_NE(0, watcher->timer_id());

  xconn.SetPointerPosition(49, 105);
  watcher->TriggerTimeout();
  EXPECT_EQ(0, counter.num_calls());
  EXPECT_NE(0, watcher->timer_id());

  // As soon as the pointer moves into the rectangle, the callback should
  // be run and the timer should be destroyed.
  xconn.SetPointerPosition(50, 105);
  watcher->TriggerTimeout();
  EXPECT_EQ(1, counter.num_calls());
  EXPECT_EQ(0, watcher->timer_id());

  // Now create a new watcher that waits for the pointer to move *outside*
  // of the same region.
  watcher.reset(
      new PointerPositionWatcher(
          &xconn,
          NewPermanentCallback(&counter, &TestCallbackCounter::Increment),
          false,     // watch_for_entering_target=false
          50, 100,   // x, y
          20, 30));  // width, height
  EXPECT_NE(0, watcher->timer_id());
  counter.Reset();

  watcher->TriggerTimeout();
  EXPECT_EQ(0, counter.num_calls());
  EXPECT_NE(0, watcher->timer_id());

  xconn.SetPointerPosition(69, 129);
  watcher->TriggerTimeout();
  EXPECT_EQ(0, counter.num_calls());
  EXPECT_NE(0, watcher->timer_id());

  xconn.SetPointerPosition(69, 130);
  watcher->TriggerTimeout();
  EXPECT_EQ(1, counter.num_calls());
  EXPECT_EQ(0, watcher->timer_id());
}

// Test that we don't crash if a callback deletes the watcher that ran it.
TEST_F(PointerPositionWatcherTest, DeleteFromCallback) {
  MockXConnection xconn;
  xconn.SetPointerPosition(0, 0);

  // Register a callback that deletes its own watcher.
  WatcherContainer container;
  container.set_watcher(
      new PointerPositionWatcher(
          &xconn,
          NewPermanentCallback(
              &container,
              &WatcherContainer::set_watcher,
              static_cast<PointerPositionWatcher*>(NULL)),
          true,      // watch_for_entering_target
          0, 0,      // x, y
          10, 10));  // width, height

  container.watcher->TriggerTimeout();
  EXPECT_TRUE(container.watcher.get() == NULL);
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
