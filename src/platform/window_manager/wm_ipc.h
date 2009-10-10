// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_WM_IPC_H__
#define __PLATFORM_WINDOW_MANAGER_WM_IPC_H__

#include <gdk/gdk.h>  // for GdkEventClient
#include <glog/logging.h>
extern "C" {
#include <X11/Xlib.h>
}

#include "base/basictypes.h"

typedef ::Atom XAtom;
typedef ::Window XWindow;

namespace chromeos {

class AtomCache;    // from atom_cache.h
class XConnection;  // from x_connection.h

// This class simplifies window-manager-to-client-app communication.  It
// consists primarily of utility methods to set and read properties on
// client windows and to pass messages back and forth between the WM and
// apps.
class WmIpc {
 public:
  WmIpc(XConnection* xconn, AtomCache* cache);

  // Get a window suitable for sending messages to the window manager.
  XWindow wm_window() const { return wm_window_; }

  enum WindowType {
    WINDOW_TYPE_UNKNOWN = 0,

    // A top-level Chrome window.
    WINDOW_TYPE_CHROME_TOPLEVEL,

    // A window showing scaled-down views of all of the tabs within a
    // Chrome window.
    WINDOW_TYPE_CHROME_TAB_SUMMARY,

    // A tab that's been detached from a Chrome window and is currently
    // being dragged.
    //   param[0]: Cursor's initial X position at the start of the drag
    //   param[1]: Cursor's initial Y position
    //   param[2]: X component of cursor's offset from upper-left corner of
    //             tab at start of drag
    //   param[3]: Y component of cursor's offset
    WINDOW_TYPE_CHROME_FLOATING_TAB,

    // The contents of a popup window.
    //   param[0]: X ID of associated titlebar, which must be mapped before
    //             its panel
    //   param[1]: Initial state for panel (0 is collapsed, 1 is expanded)
    WINDOW_TYPE_CHROME_PANEL,

    // A small window representing a collapsed panel in the panel bar and
    // drawn above the panel when it's expanded.
    WINDOW_TYPE_CHROME_PANEL_TITLEBAR,

    // A small window that when clicked creates a new browser window.
    WINDOW_TYPE_CREATE_BROWSER_WINDOW,

    kNumWindowTypes,
  };
  // Get or set a property describing a window's type.  The window type
  // property must be set before mapping a window (for GTK+ apps, this
  // means it must happen between gtk_widget_realize() and
  // gtk_widget_show()).  Type-specific parameters may also be supplied
  // ('params' is mandatory for GetWindowType() but optional for
  // SetWindowType()).  false is returned if an error occurs.
  bool GetWindowType(XWindow xid, WindowType* type, vector<int>* params);
  bool SetWindowType(XWindow xid, WindowType type, const vector<int>* params);

  // Messages are sent via ClientMessageEvents that have 'message_type' set
  // to _CHROME_WM_MESSAGE, 'format' set to 32 (that is, 32-bit values),
  // and l[0] set to a value from the MessageType enum.  The remaining four
  // values in the 'l' array contain data specific to the type of message
  // being sent.
  struct Message {
   public:
    enum Type {
      UNKNOWN = 0,

      // Notify Chrome when a floating tab has entered or left a tab
      // summary window.  Sent to the summary window.
      //   param[0]: X ID of the floating tab window
      //   param[1]: state (0 means left, 1 means entered or currently in)
      //   param[2]: X coordinate relative to summary window
      //   param[3]: Y coordinate
      CHROME_NOTIFY_FLOATING_TAB_OVER_TAB_SUMMARY,

      // Notify Chrome when a floating tab has entered or left a top-level
      // window.  Sent to the window being entered/left.
      //   param[0]: X ID of the floating tab window
      //   param[1]: state (0 means left, 1 means entered)
      CHROME_NOTIFY_FLOATING_TAB_OVER_TOPLEVEL,

      // Instruct a top-level Chrome window to change the visibility of its
      // tab summary window.
      //   param[0]: desired visibility (0 means hide, 1 means show)
      //   param[1]: X position (relative to the left edge of the root
      //             window) of the center of the top-level window.  Only
      //             relevant for "show" messages
      CHROME_SET_TAB_SUMMARY_VISIBILITY,

      // Tell the WM to collapse or expand a panel.
      //   param[0]: X ID of the panel window
      //   param[1]: desired state (0 means collapsed, 1 means expanded)
      WM_SET_PANEL_STATE,

      // Notify Chrome that the panel state has changed.  Sent to the panel
      // window.
      //   param[0]: new state (0 means collapsed, 1 means expanded)
      CHROME_NOTIFY_PANEL_STATE,

      // Instruct the WM to move a floating tab.  The passed-in position is
      // that of the cursor; the tab's composited window is displaced based
      // on the cursor's offset from the upper-left corner of the tab at
      // the start of the drag.
      //   param[0]: X ID of the floating tab window
      //   param[1]: X coordinate to which the tab should be moved
      //   param[2]: Y coordinate
      WM_MOVE_FLOATING_TAB,

      // Instruct the WM to move a panel.
      //   param[0]: X ID of the panel window
      //   param[1]: X coordinate to which the panel should be moved
      //   param[2]: Y coordinate
      WM_MOVE_PANEL,

      // Notify the WM that the panel drag is complete (that is, the mouse
      // button has been released).
      //   param[0]: X ID of the panel window
      WM_NOTIFY_PANEL_DRAG_COMPLETE,

      // Instruct the WM to focus a window (either top-level or a panel).
      // This is used when a tab is clicked in a tab overview window, or
      // when the user clicks on a panel's titlebar to expand it.
      //   param[0]: X ID of the window to focus
      WM_FOCUS_WINDOW,

      // Notify Chrome that the layout mode (for example, overview or
      // focused) has changed.
      //   param[0]: new mode (0 means focused, 1 means overview)
      CHROME_NOTIFY_LAYOUT_MODE,

      // Instruct the WM to enter overview mode.
      //   param[0]: X ID of the window show the tab overview for.
      WM_SWITCH_TO_OVERVIEW_MODE,

      kNumTypes,
    };

    Message() {
      Init(UNKNOWN);
    }
    Message(Type type) {
      Init(type);
    }

    Type type() const { return type_; }
    void set_type(Type type) { type_ = type; }

    inline int max_params() const {
      return arraysize(params_);
    }
    long param(int index) const {
      CHECK_GE(index, 0);
      CHECK_LT(index, max_params());
      return params_[index];
    }
    void set_param(int index, long value) {
      CHECK_GE(index, 0);
      CHECK_LT(index, max_params());
      params_[index] = value;
    }

   private:
    // Common initialization code shared between constructors.
    void Init(Type type) {
      set_type(type);
      for (int i = 0; i < max_params(); ++i) {
        set_param(i, 0);
      }
    }

    // Type of message that was sent.
    Type type_;

    // Type-specific data.  This is bounded by the number of 32-bit values
    // that we can pack into a ClientMessageEvent -- it holds five, but we
    // use the first one to store the Chrome OS message type.
    long params_[4];
  };

  // Check whether an event received from the X server contains a
  // ClientMessageEvent for us.  If it does, the message is copied to 'msg'
  // and true is returned; otherwise, false is returned and the caller
  // should continue processing the event.
  bool GetMessage(const XClientMessageEvent& e, Message* msg);

  // Convenience method that copies a GdkEventClient into an XEvent and
  // passes it to GetMessage().  Call GetMessage() directly instead if
  // possible.
  bool GetMessageGdk(const GdkEventClient& e, Message* msg);

  // Send a message to a window.  false is returned if an error occurs.
  bool SendMessage(XWindow xid, const Message& msg);

  // Set a property on the chosen window that contains system metrics
  // information.  False returned on error.
  bool SetSystemMetricsProperty(XWindow xid, const string& metrics);

 private:
  XConnection* xconn_;     // not owned
  AtomCache* atom_cache_;  // not owned

  // Window used for sending messages to the window manager.
  XWindow wm_window_;

  DISALLOW_COPY_AND_ASSIGN(WmIpc);
};

}  // namespace chromeos

#endif
