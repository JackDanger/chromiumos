// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_COMPOSITOR_EVENT_SOURCE_H_
#define WINDOW_MANAGER_COMPOSITOR_EVENT_SOURCE_H_

#include "window_manager/x_types.h"

namespace window_manager {

// This is a small interface for classes that send X events to compositors
// (implemented by WindowManager).  Compositors can use it to register or
// unregister interest in receiving events about particular windows.
class CompositorEventSource {
 public:
  virtual ~CompositorEventSource() {}

  // Send or stop sending the compositor subsequent compositing-related
  // events about the passed-in window.
  virtual void StartSendingEventsForWindowToCompositor(XWindow xid) = 0;
  virtual void StopSendingEventsForWindowToCompositor(XWindow xid) = 0;
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_COMPOSITOR_EVENT_SOURCE_H_
