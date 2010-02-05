// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/motion_event_coalescer.h"

#include "chromeos/obsolete_logging.h"

namespace window_manager {

using chromeos::Closure;

MotionEventCoalescer::MotionEventCoalescer(Closure* cb, int timeout_ms)
    : timer_id_(0),
      timeout_ms_(timeout_ms),
      have_queued_position_(false),
      x_(-1),
      y_(-1),
      cb_(cb),
      synchronous_(false) {
  CHECK(cb);
  CHECK_GT(timeout_ms, 0);
}

MotionEventCoalescer::~MotionEventCoalescer() {
  if (IsRunning())
    StopInternal(false);
}

void MotionEventCoalescer::Start() {
  if (timer_id_) {
    LOG(WARNING) << "Ignoring request to start coalescer while timer "
                 << "is already running";
    return;
  }
  if (!synchronous_)
    timer_id_ = g_timeout_add(timeout_ms_, &HandleTimerThunk, this);
  have_queued_position_ = false;
  x_ = -1;
  y_ = -1;
}

void MotionEventCoalescer::Stop() {
  if (!synchronous_)
    StopInternal(true);
}

void MotionEventCoalescer::StorePosition(int x, int y) {
  if (x == x_ && y == y_)
    return;
  x_ = x;
  y_ = y;
  have_queued_position_ = true;
  if (synchronous_)
    HandleTimer();
}

void MotionEventCoalescer::StopInternal(bool maybe_run_callback) {
  if (!timer_id_) {
    LOG(WARNING) << "Ignoring request to stop coalescer while timer "
                 << "isn't running";
    return;
  }
  g_source_remove(timer_id_);
  timer_id_ = 0;

  if (maybe_run_callback) {
    // Invoke the handler one last time to catch any events that came in
    // after the final run.
    HandleTimer();
  }
}

gboolean MotionEventCoalescer::HandleTimer() {
  if (have_queued_position_) {
    cb_->Run();
    have_queued_position_ = false;
  }
  return TRUE;
}

}  // namespace window_manager
