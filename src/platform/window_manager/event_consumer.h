// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_EVENT_CONSUMER_H__
#define __PLATFORM_WINDOW_MANAGER_EVENT_CONSUMER_H__

extern "C" {
#include <X11/Xlib.h>
}

#include "window_manager/wm_ipc.h"  // for WmIpc::Message

typedef ::Window XWindow;

namespace chromeos {

class Window;

// This is an abstract base class for consuming events received by the
// WindowManager class.  EventConsumer's virtual methods receive
// notifications of events and can state that they've consumed an event by
// returning true.  Different consumers' handlers are invoked in an
// arbitrary order; once one consumes an event, it isn't passed to any
// other consumers.
class EventConsumer {
 public:
  EventConsumer() {}
  virtual ~EventConsumer() {}

  // Is the passed-in window an input window owned by this consumer?
  virtual bool IsInputWindow(XWindow xid) const { return false; }

  // Handle a window being mapped.  This method and HandleWindowUnmap()
  // return void so as to be invoked for all consumers -- these events
  // should be relatively infrequent, and having a consumer miss an unmap
  // event that it wanted because another consumer claimed to have handled
  // it could result in dangling pointers and eventual segfaults.
  virtual void HandleWindowMap(Window* win) { }

  // Handle a window being unmapped.
  virtual void HandleWindowUnmap(Window* win) { }

  // Handle a window's request to be resized.  To permit the resize as
  // requested and allow other consumers to examine the request, the
  // consumer should return false.  Otherwise, the consumer should modify
  // 'req_width' and 'req_height' to the actual values that should be used
  // and return true.  (Modifications to the passed-in pointers will be
  // ignored if false is returned.)
  virtual bool HandleWindowResizeRequest(
      Window* win, int* req_width, int* req_height) {
    return false;
  }

  // Handle a button press or release on a window.  The position is
  // relative to the upper-left corner of the window.
  virtual bool HandleButtonPress(
      XWindow xid, int x, int y, int button, Time timestamp) {
    return false;
  }
  virtual bool HandleButtonRelease(
      XWindow xid, int x, int y, int button, Time timestamp) {
    return false;
  }

  // Handle the pointer entering or leaving an input window.
  virtual bool HandlePointerEnter(XWindow xid, Time timestamp) { return false; }
  virtual bool HandlePointerLeave(XWindow xid, Time timestamp) { return false; }
  virtual bool HandlePointerMotion(XWindow xid, int x, int y, Time timestamp) {
    return false;
  }

  // Handle a Chrome-specific message sent by a client app.
  virtual bool HandleChromeMessage(const WmIpc::Message& msg) {
    return false;
  }

  // Handle a regular X ClientMessage event from a client app.
  virtual bool HandleClientMessage(const XClientMessageEvent& e) {
    return false;
  }

  // Handle a focus change on a window.
  virtual bool HandleFocusChange(XWindow xid, bool focus_in) { return false; }
};

}  // namespace chromeos

#endif
