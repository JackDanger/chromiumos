// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_MOTION_EVENT_COALESCER_H__
#define __PLATFORM_WINDOW_MANAGER_MOTION_EVENT_COALESCER_H__

#include <glib.h>  // for gboolean and gint

#include <chromeos/callback.h>
#include "base/scoped_ptr.h"

namespace chromeos {

// Rate-limits how quickly motion events are processed by saving them as
// they're generated and then periodically invoking a callback (but only if
// new motion events have been received).
class MotionEventCoalescer {
 public:
  // The constructor takes ownership of 'cb'.
  MotionEventCoalescer(Closure* cb, int timeout_ms);
  ~MotionEventCoalescer();

  int x() const { return x_; }
  int y() const { return y_; }

  // Start or stop the timer.
  void Start();
  void Stop();

  // Is the timer currently running?
  bool IsRunning() {
    return timer_id_ != 0;
  }

  // Store a position.  This should be called in response to each motion
  // event.
  void StorePosition(int x, int y);

 private:
  // Invoked by Stop() and by the destructor to remove the timer.  If
  // 'maybe_run_callback' is true, the callback will be invoked one last
  // time if a new position has been received but not yet handled (the
  // destructor passes false here; running the callback may be dangerous if
  // parts of the owning class have already been destroyed).
  void StopInternal(bool maybe_run_callback);

  static gboolean HandleTimerThunk(gpointer self) {
    return reinterpret_cast<MotionEventCoalescer*>(self)->HandleTimer();
  }
  gboolean HandleTimer();

  // ID of the timer's GLib event source, or 0 if the timer isn't active.
  guint timer_id_;

  // Frequency for invoking the callback, in milliseconds.
  int timeout_ms_;

  // Have we received a position since the last time the callback was
  // invoked?
  bool have_queued_position_;

  // The most-recently-received position.
  int x_;
  int y_;

  // Callback that gets periodically invoked when there's a new position to
  // handle.
  // TODO: When we're using a callback library that supports parameters, we
  // should just pass the position directly to the callback.
  scoped_ptr<Closure> cb_;
};

}  // namespace chromeos

#endif
