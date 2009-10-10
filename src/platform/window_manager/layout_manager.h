// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_LAYOUT_MANAGER_H__
#define __PLATFORM_WINDOW_MANAGER_LAYOUT_MANAGER_H__

extern "C" {
#include <X11/Xlib.h>
}
#include <deque>
#include <glib.h>  // for guint
#include <map>
#include <string>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/ref_ptr.h"
#include "base/scoped_ptr.h"
#include "window_manager/event_consumer.h"
#include "window_manager/key_bindings.h"
#include "window_manager/util.h"
#include "window_manager/wm_ipc.h"  // for WmIpc::Message

typedef ::Window XWindow;

namespace chrome_os_pb {
class SystemMetrics;
}

namespace chromeos {

class MotionEventCoalescer;
class Window;
class WindowManager;

// Manages the placement of regular client windows.
//
// It currently supports two modes: "active", where a single window is
// displayed at full scale and given the input focus, and "overview", where
// scaled-down copies of all windows are displayed across the bottom of the
// screen.
//
class LayoutManager : public EventConsumer {
 public:
  // 'x', 'y', 'width', and 'height' specify where windows should be
  // displayed on overview mode.  This should be at the bottom of the
  // screen.
  LayoutManager(WindowManager* wm, int x, int y, int width, int height);
  ~LayoutManager();

  // Struct for keeping track of user metrics relevant to the LayoutManager.
  struct Metrics {
    Metrics()
        : overview_by_keystroke_count(0),
          overview_exit_by_mouse_count(0),
          overview_exit_by_keystroke_count(0),
          window_cycle_by_keystroke_count(0) {
    }

    // Given a metrics protobuffer, populates the applicable fields.
    void Populate(chrome_os_pb::SystemMetrics *metrics_pb);

    void Reset() {
      overview_by_keystroke_count = 0;
      overview_exit_by_mouse_count = 0;
      overview_exit_by_keystroke_count = 0;
      window_cycle_by_keystroke_count = 0;
    }

    int overview_by_keystroke_count;
    int overview_exit_by_mouse_count;
    int overview_exit_by_keystroke_count;
    int window_cycle_by_keystroke_count;
  };

  int x() const { return x_; }
  int y() const { return y_; }
  int width() const { return width_; }
  int height() const { return height_; }

  // Returns a pointer to the struct in which LayoutManager tracks
  // relevant user metrics.
  Metrics* GetMetrics() { return &metrics_; }

  // Note: Begin overridden EventConsumer methods.

  // Is the passed-in window an input window?
  bool IsInputWindow(XWindow xid) const;

  // Handle a new window.  This method takes care of moving the client
  // window offscreen so it doesn't get input events and of redrawing the
  // layout if necessary.
  void HandleWindowMap(Window* win);

  // Handle the removal of a window.
  void HandleWindowUnmap(Window* win);

  // Handle events received by windows.
  bool HandleButtonPress(XWindow xid, int x, int y, int button, Time timestamp);
  bool HandlePointerEnter(XWindow xid, Time timestamp);
  bool HandlePointerLeave(XWindow xid, Time timestamp);
  bool HandleFocusChange(XWindow xid, bool focus_in);

  // Handle messages from client apps.
  bool HandleChromeMessage(const WmIpc::Message& msg);
  bool HandleClientMessage(const XClientMessageEvent& e);

  // Note: End overridden EventConsumer methods.

  // Return a pointer to a Chrome top-level window, if one exists.
  // Returns NULL if there is no such window.
  Window* GetChromeWindow();

  // Move a floating tab window to the queued values.  Invoked periodically
  // by 'floating_tab_event_coalescer_'.
  void MoveFloatingTab();

  // Take the input focus if possible.  Returns 'false' if it doesn't make
  // sense to take the focus (currently, we take the focus if we're in
  // active mode but refuse to in overview mode).
  bool TakeFocus();

  // Change the amount of space allocated to the layout manager.
  void Resize(int width, int height);

 private:
  FRIEND_TEST(LayoutManagerTest, Basic);  // uses SetMode()
  FRIEND_TEST(LayoutManagerTest, Focus);
  FRIEND_TEST(LayoutManagerTest, FocusTransient);

  // Contains information about managed top-level windows and where they
  // should be displayed.
  struct WindowInfo {
    WindowInfo(Window* win, XWindow input)
        : win(win),
          input_win(input),
          state(STATE_NEW),
          x(0),
          y(0),
          width(0),
          height(0),
          scale(1.0),
          fullscreen(false) {
    }

    // Window object for the toplevel client window.
    Window* win;  // not owned

    // The invisible input window that represents the client window in
    // overview mode.
    XWindow input_win;

    // The state the window is in.  Used to determine how it should be
    // animated by Arrange*() methods.
    enum State {
      // The window has just been added.
      STATE_NEW = 0,

      // We're in active mode and the window is onscreen.
      STATE_ACTIVE_MODE_ONSCREEN,

      // We're in active mode and the window is offscreen.
      STATE_ACTIVE_MODE_OFFSCREEN,

      // We're in active mode and the window should be animated sliding in
      // or out from a specific direction.
      STATE_ACTIVE_MODE_IN_FROM_RIGHT,
      STATE_ACTIVE_MODE_IN_FROM_LEFT,
      STATE_ACTIVE_MODE_OUT_TO_LEFT,
      STATE_ACTIVE_MODE_OUT_TO_RIGHT,
      STATE_ACTIVE_MODE_IN_FADE,
      STATE_ACTIVE_MODE_OUT_FADE,

      // We're in overview mode and the window should be displayed in the
      // normal manner on the bottom of the screen.
      STATE_OVERVIEW_MODE_NORMAL,

      // We're in overview mode and the window should be magnified.
      STATE_OVERVIEW_MODE_MAGNIFIED,
    };
    State state;

    // Position, dimensions, and scale that should be used for drawing the
    // window in overview mode.
    int x;
    int y;
    int width;
    int height;
    double scale;

    // Is the window currently in fullscreen mode?
    bool fullscreen;

   private:
    DISALLOW_COPY_AND_ASSIGN(WindowInfo);
  };

  // Get the window represented by the passed-in input window, or NULL if
  // the input window doesn't belong to us.
  Window* GetWindowByInput(XWindow xid) const;

  // Get the 0-based index of the passed-in window within 'windows_'.
  // Returns -1 if it isn't present.
  int GetWindowIndex(const Window& win) const;

  // Get the WindowInfo object representing the passed-in window.
  // Returns NULL if it isn't a top-level window.
  WindowInfo* GetWindowInfo(const Window& win);

  // Modes used to display windows.
  enum Mode {
    // Display 'active_window_' at full size and let it receive input.
    // Hide all other windows.
    MODE_ACTIVE = 0,

    // Display thumbnails of all of the windows across the bottom of the
    // screen.
    MODE_OVERVIEW,
  };

  // Helper method that activates 'win', using the passed-in states for it
  // and for the previously-active window.  Only has an effect if we're
  // already in active mode.
  void SetActiveWindow(Window* win,
                       WindowInfo::State state_for_new_win,
                       WindowInfo::State state_for_old_win);

  // Switch to active mode.  If 'activate_magnified_win' is true and
  // there's a currently-magnified window, we focus it; otherwise we
  // refocus the previously-focused window).
  void SwitchToActiveMode(bool activate_magnified_win);

  // Activate the window at the passed-in 0-indexed position (or the last
  // window, for index -1).  Does nothing if no window exists at that
  // position or if we're not already in active mode.
  void ActivateWindowByIndex(int index);

  // Magnify the window at the passed-in 0-indexed position (or the last
  // window, for index -1).  Does nothing if no window exists at that
  // position or if not already in overview mode.
  void MagnifyWindowByIndex(int index);

  // Switch the current mode.
  void SetMode(Mode mode);

  // Get the scaling for a particular composited window such that it fits
  // in the dimensions 'max_width' and 'max_height'.  Out params can be
  // NULL.
  static void GetWindowScaling(const Window* window,
                               int max_width,
                               int max_height,
                               int* width_out,
                               int* height_out,
                               double* scale_out);

  // Arrange windows for various modes.
  void ArrangeActive();
  void ArrangeOverview();

  // Calculate the position and scaling of all windows for overview mode
  // and record it in 'windows_'.
  void CalculateOverview();

  // Get the window whose image in overview mode covers the passed-in
  // position, or NULL if no such window exists.
  Window* GetOverviewWindowAtPoint(int x, int y) const;

  // Does the passed-in point lie inside of 'tab_summary_'?
  bool PointIsInTabSummary(int x, int y) const;

  // Does the passed-in point's position (well, currently just its Y
  // component) lie in the region between 'magnified_window_' and
  // 'tab_summary_'?
  bool PointIsBetweenMagnifiedWindowAndTabSummary(int x, int y) const;

  // Add or remove the relevant key bindings for the passed-in mode.
  void AddKeyBindingsForMode(Mode mode);
  void RemoveKeyBindingsForMode(Mode mode);

  // Cycle the active window.  Only makes sense in active mode.
  void CycleActiveWindow(bool forward);

  // Cycle the magnified window.  Only makes sense in overview mode.
  void CycleMagnifiedWindow(bool forward);

  // Set 'magnified_window_' to the passed-in window (which can be NULL to
  // disable magnification).  Also takes care of telling the
  // previously-magnified window to hide its tabs (but not telling the new
  // window to show its tabs; we want to include the window's position in
  // those messages, so SendTabSummaryMessage() should be explicitly called
  // after ArrangeOverview() is called).
  void SetMagnifiedWindow(Window* win);

  // Tell a window to show or hide its tab summary.  Does nothing if 'win'
  // isn't a top-level Chrome window.
  void SendTabSummaryMessage(Window* win, bool show);

  // Send a message to a window describing the current state of 'mode_'.
  // Does nothing if 'win' isn't a top-level Chrome window.
  void SendModeMessage(Window* win);

  // Ask the active window to delete itself.
  void SendDeleteRequestToActiveWindow();

  // If 'win' has a modal transient window, tell the transient to take the
  // focus; otherwise tell 'win' to take it.
  void FocusWindowOrModalTransient(Window* win, Time timestamp);

  WindowManager* wm_;  // not owned

  // The current mode.
  Mode mode_;

  // Position and dimensions where we draw windows.
  int x_;
  int y_;
  int width_;
  int height_;

  // Maximum height of windows in overview mode.
  int overview_height_;

  // Information about top-level windows, stored in the order in which
  // we'll display them in overview mode.
  typedef deque<ref_ptr<WindowInfo> > WindowInfos;
  WindowInfos windows_;

  // Map from input window to the top-level window it represents.
  map<XWindow, Window*> input_to_window_;

  // Currently-magnified window in overview mode, or NULL if no window is
  // magnified.
  Window* magnified_window_;

  // Currently-active window in active mode.
  Window* active_window_;

  // Floating window containing a tab that's being dragged around and the
  // toplevel Chrome window currently underneath it.
  Window* floating_tab_;
  Window* window_under_floating_tab_;

  // Most recently-created tab summary window.
  Window* tab_summary_;

  // Window that when clicked creates a new browser. May be null, and only
  // shown when in overview mode.
  Window* create_browser_window_;

  // We save the requested positions for the floating tab here so we can
  // apply them periodically in MoveFloatingTab().
  scoped_ptr<MotionEventCoalescer> floating_tab_event_coalescer_;

  Metrics metrics_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace chromeos

#endif
