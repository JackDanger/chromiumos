// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_EVENT_CONSUMER_H_
#define WINDOW_MANAGER_EVENT_CONSUMER_H_

extern "C" {
#include <X11/Xlib.h>
}

#include "window_manager/wm_ipc.h"  // for WmIpc::Message

typedef ::Window XWindow;

namespace window_manager {

class Window;

// This is an interface for things that want to receive X events from the
// WindowManager class.
//
// Except where noted otherwise, consumers express interest in a particular
// window's events by calling
// WindowManager::RegisterEventConsumerForWindowEvents().  When the window
// manager receives an event concerning the window, all interested
// consumers' handlers will be invoked in an arbitrary order.  Note that
// consumers may also need to select some event types on windows using
// XConnection::SelectInputOnWindow() in order for the X server to report
// those events to the window manager.
//
// The common case is:
// - A window gets created and WindowManager begins tracking it.
// - The window tries to map itself.  WindowManager starts invoking
//   consumers' HandleWindowMapRequest() methods until one of them maps the
//   window and returns true.
// - WindowManager receives notification from the X server that the window
//   has been mapped and invokes all consumers' HandleWindowMap() methods.
//   The consumer that will be handling the window (typically the one that
//   handled the map request) registers interest in the window's events by
//   calling RegisterEventConsumerForWindowEvents() with the window's ID.
// - Stuff happens and the interested consumer is notified about the window's
//   events.
// - The window unmaps itself.  WindowManager invokes all consumers'
//   HandleWindowUnmap() methods.  The consumer that's handling the window
//   deletes any internal state about it and unregisters interest in the
//   window's events.
// - The window is deleted.  WindowManager stops tracking it.
class EventConsumer {
 public:
  EventConsumer() {}
  virtual ~EventConsumer() {}

  // Is the passed-in window an input window owned by this consumer?
  virtual bool IsInputWindow(XWindow xid) = 0;

  // Handle a window's request to be mapped.  This is invoked to give
  // consumers a chance to change a window's position, size, or stacking
  // before it gets mapped (note that the consumer is ultimately
  // responsible for mapping the window as well).
  //
  // WindowManager attempts to invoke this method for all consumers.  If a
  // consumer handles the event by mapping the window, it should return
  // true.  Once the event has been handled, it won't be passed to any
  // other consumers.
  virtual bool HandleWindowMapRequest(Window* win) = 0;

  // Handle a window being mapped.  Invoked for all consumers.
  virtual void HandleWindowMap(Window* win) = 0;

  // Handle a window being unmapped.  Invoked for all consumers.
  virtual void HandleWindowUnmap(Window* win) = 0;

  // Handle a mapped window's request to be configured (unmapped windows'
  // requests are applied automatically).  If the consumer wants to
  // configure the window (possibly with different parameters than the
  // requested ones), it should call Window::MoveClient() and
  // ResizeClient().
  virtual void HandleWindowConfigureRequest(Window* win,
                                            int req_x, int req_y,
                                            int req_width, int req_height) = 0;

  // Handle a button press or release on a window.  The first position is
  // relative to the upper-left corner of the window, while the second is
  // absolute.
  virtual void HandleButtonPress(XWindow xid,
                                 int x, int y,
                                 int x_root, int y_root,
                                 int button,
                                 Time timestamp) = 0;
  virtual void HandleButtonRelease(XWindow xid,
                                   int x, int y,
                                   int x_root, int y_root,
                                   int button,
                                   Time timestamp) = 0;

  // Handle the pointer entering, leaving, or moving within an input window.
  virtual void HandlePointerEnter(XWindow xid,
                                  int x, int y,
                                  int x_root, int y_root,
                                  Time timestamp) = 0;
  virtual void HandlePointerLeave(XWindow xid,
                                  int x, int y,
                                  int x_root, int y_root,
                                  Time timestamp) = 0;
  virtual void HandlePointerMotion(XWindow xid,
                                   int x, int y,
                                   int x_root, int y_root,
                                   Time timestamp) = 0;

  // Handle a Chrome-specific message sent by a client app.  Messages are
  // sent to consumers that have expressed interest in the messages' types
  // with WindowManager::RegisterEventConsumerForChromeMessages().
  virtual void HandleChromeMessage(const WmIpc::Message& msg) = 0;

  // Handle a regular X ClientMessage event from a client app.
  // These events are sent to consumers that have expressed interest in
  // events on the window referenced in the event's 'window' field.
  virtual void HandleClientMessage(const XClientMessageEvent& e) = 0;

  // Handle a focus change on a window.
  virtual void HandleFocusChange(XWindow xid, bool focus_in) = 0;

  // Handle a property change.  These changes are sent to consumers that
  // have expressed interest in the (xid, xatom) pair with
  // WindowManager::RegisterEventConsumerForPropertyChanges().
  virtual void HandleWindowPropertyChange(XWindow xid, XAtom xatom) = 0;
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_EVENT_CONSUMER_H_
