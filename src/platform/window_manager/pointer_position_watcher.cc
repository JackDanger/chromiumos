// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/pointer_position_watcher.h"

#include "window_manager/x_connection.h"

namespace window_manager {

// How frequently should we query the pointer position, in milliseconds?
static const int kTimeoutMs = 200;

PointerPositionWatcher::PointerPositionWatcher(
    XConnection* xconn,
    chromeos::Closure* cb,
    bool watch_for_entering_target,
    int target_x, int target_y, int target_width, int target_height)
    : xconn_(xconn),
      cb_(cb),
      watch_for_entering_target_(watch_for_entering_target),
      target_x_(target_x),
      target_y_(target_y),
      target_width_(target_width),
      target_height_(target_height),
      timer_id_(g_timeout_add(kTimeoutMs, &HandleTimerThunk, this)) {
}

PointerPositionWatcher::~PointerPositionWatcher() {
  if (timer_id_) {
    g_source_remove(timer_id_);
    timer_id_ = 0;
  }
}

void PointerPositionWatcher::TriggerTimeout() {
  // We need to store the timer ID since HandleTimer() can clear it.
  int timer_id = timer_id_;
  if (HandleTimer() == FALSE)
    g_source_remove(timer_id);
}

gboolean PointerPositionWatcher::HandleTimer() {
  int pointer_x = 0, pointer_y = 0;
  if (!xconn_->QueryPointerPosition(&pointer_x, &pointer_y))
    return TRUE;

  // Bail out early if we're not in the desired state yet.
  bool in_target = pointer_x >= target_x_ &&
                   pointer_x < target_x_ + target_width_ &&
                   pointer_y >= target_y_ &&
                   pointer_y < target_y_ + target_height_;
  if ((watch_for_entering_target_ && !in_target) ||
      (!watch_for_entering_target_ && in_target))
    return TRUE;

  // Otherwise, run the callback and kill the timer.  We clear 'timer_id_'
  // before running the callback, since it's possible that the callback may
  // delete us.
  timer_id_ = 0;
  cb_->Run();
  return FALSE;
}

}  // namespace window_manager
