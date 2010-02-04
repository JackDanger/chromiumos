// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_POINTER_POSITION_WATCHER_H_
#define WINDOW_MANAGER_POINTER_POSITION_WATCHER_H_

#include <glib.h>  // for gboolean and gint

#include "base/scoped_ptr.h"
#include "chromeos/callback.h"

namespace window_manager {

class XConnection;

// This class periodically queries the mouse pointer's position and invokes
// a callback once the pointer has moved into or out of a target rectangle.
//
// This is primarily useful for:
// a) avoiding race conditions in cases where we want to open a new window
//    under the pointer and then do something when the pointer leaves the
//    window -- it's possible that the pointer will have already been moved
//    away by the time that window is created
// b) getting notified when the pointer enters or leaves a region without
//    creating a window that will steal events from windows underneath it
//
// With that being said, repeatedly waking up to poll the X server over
// long periods of time is a bad idea from a power consumption perspective,
// so this should only be used in cases where the user is likely to
// enter/leave the target region soon.
class PointerPositionWatcher {
 public:
  // The constructor takes ownership of 'cb'.
  PointerPositionWatcher(
      XConnection* xconn,
      chromeos::Closure* cb,
      bool watch_for_entering_target,  // as opposed to leaving it
      int target_x, int target_y, int target_width, int target_height);
  ~PointerPositionWatcher();

  // Useful for testing.
  int timer_id() const { return timer_id_; }

  // Invoke HandleTimer() manually.  Useful for testing.
  void TriggerTimeout();

 private:
  static gboolean HandleTimerThunk(gpointer self) {
    return reinterpret_cast<PointerPositionWatcher*>(self)->HandleTimer();
  }
  gboolean HandleTimer();

  XConnection* xconn_;  // not owned

  // Callback that gets invoked when the pointer enters/exits the target
  // rectangle.
  scoped_ptr<chromeos::Closure> cb_;

  // Should we watch for the pointer entering the target rectangle, as
  // opposed to leaving it?
  bool watch_for_entering_target_;

  // Target rectangle.
  int target_x_;
  int target_y_;
  int target_width_;
  int target_height_;

  // ID of the timer's GLib event source, or 0 if the timer isn't active.
  guint timer_id_;
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_MOTION_POINTER_POSITION_WATCHER_H_
