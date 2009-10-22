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
// It currently supports two modes: "active", where a single toplevel
// window is displayed at full scale and given the input focus, and
// "overview", where scaled-down copies of all toplevel windows are
// displayed across the bottom of the screen.
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

  // Return a pointer to an arbitrary Chrome toplevel window, if one
  // exists.  Returns NULL if there is no such window.
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

  // A toplevel window that we're managing.
  class ToplevelWindow {
   public:
    ToplevelWindow(Window* win, LayoutManager* layout_manager);
    ~ToplevelWindow();

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

    Window* win() { return win_; }
    XWindow input_xid() { return input_xid_; }

    State state() const { return state_; }
    void set_state(State state) { state_ = state; }

    int overview_x() const { return overview_x_; }
    int overview_y() const { return overview_y_; }
    int overview_width() const { return overview_width_; }
    int overview_height() const { return overview_height_; }
    int overview_scale() const { return overview_scale_; }
    int overview_center_x() const {
      return overview_x_ + 0.5 * overview_width_;
    }
    int overview_offscreen_y() const {
      return layout_manager_->y() + layout_manager_->height();
    }

    // Does the passed-in point fall within the bounds of our window in
    // overview mode?
    bool OverviewWindowContainsPoint(int x, int y) const {
      return x >= overview_x_ &&
             x < overview_x_ + overview_width_ &&
             y >= overview_y_ &&
             y < overview_y_ + overview_height_;
    }

    // Arrange the window for active mode.  This involves either moving the
    // client window on- or offscreen (depending on 'window_is_active'),
    // animating the composited window according to 'state_', and possibly
    // focusing the window or one of its transients.
    void ArrangeForActiveMode(bool window_is_active);

    // Arrange the window for overview mode.  This involves animating its
    // composited position and scale as specified by 'overview_*' and
    // 'state_', moving its client window offscreen (so it won't receive
    // mouse events), and moving its input window onscreen.
    void ArrangeForOverviewMode(bool window_is_magnified,
                                bool dim_if_unmagnified);

    // Set 'overview_x_' and 'overview_y_' to the passed-in values.
    void UpdateOverviewPosition(int x, int y) {
      overview_x_ = x;
      overview_y_ = y;
    }

    // Update 'overview_width_', 'overview_height_', and 'overview_scale_'
    // for our composited window such that it fits in the dimensions
    // 'max_width' and 'max_height'.
    void UpdateOverviewScaling(int max_width, int max_height);

    // If we have a modal transient window, tell it to take the focus;
    // otherwise give it to the toplevel window.
    void FocusWindowOrModalTransient(Time timestamp);

   private:
    WindowManager* wm() { return layout_manager_->wm_; }

    // Window object for the toplevel client window.
    Window* win_;  // not owned

    LayoutManager* layout_manager_;  // not owned

    // The invisible input window that represents the client window in
    // overview mode.
    XWindow input_xid_;

    // The state the window is in.  Used to determine how it should be
    // animated by ArrangeFor*() methods.
    State state_;

    // Position, dimensions, and scale that should be used for drawing the
    // window in overview mode.  These are absolute, rather than relative
    // to the layout manager's origin.
    int overview_x_;
    int overview_y_;
    int overview_width_;
    int overview_height_;
    double overview_scale_;

   private:
    DISALLOW_COPY_AND_ASSIGN(ToplevelWindow);
  };

  // Get the toplevel window represented by the passed-in input window, or
  // NULL if the input window doesn't belong to us.
  ToplevelWindow* GetToplevelWindowByInputXid(XWindow xid) const;

  // Get the 0-based index of the passed-in toplevel within 'windows_'.
  // Returns -1 if it isn't present.
  int GetIndexForToplevelWindow(const ToplevelWindow& toplevel) const;

  // Get the ToplevelWindow object representing the passed-in window.
  // Returns NULL if it isn't a toplevel window.
  ToplevelWindow* GetToplevelWindowByWindow(const Window& win);

  // Modes used to display windows.
  enum Mode {
    // Display 'active_window_' at full size and let it receive input.
    // Hide all other windows.
    MODE_ACTIVE = 0,

    // Display thumbnails of all of the windows across the bottom of the
    // screen.
    MODE_OVERVIEW,
  };

  // Helper method that activates 'toplevel', using the passed-in states
  // for it and for the previously-active toplevel window.  Only has an
  // effect if we're already in active mode.
  void SetActiveToplevelWindow(ToplevelWindow* toplevel,
                               ToplevelWindow::State state_for_new_win,
                               ToplevelWindow::State state_for_old_win);

  // Switch to active mode.  If 'activate_magnified_win' is true and
  // there's a currently-magnified toplevel window, we focus it; otherwise
  // we refocus the previously-focused window).
  void SwitchToActiveMode(bool activate_magnified_win);

  // Activate the toplevel window at the passed-in 0-indexed position (or
  // the last window, for index -1).  Does nothing if no window exists at
  // that position or if we're not already in active mode.
  void ActivateToplevelWindowByIndex(int index);

  // Magnify the toplevel window at the passed-in 0-indexed position (or
  // the last window, for index -1).  Does nothing if no window exists at
  // that position or if not already in overview mode.
  void MagnifyToplevelWindowByIndex(int index);

  // Switch the current mode.
  void SetMode(Mode mode);

  // Arrange all windows for various modes.
  void ArrangeToplevelWindowsForActiveMode();
  void ArrangeToplevelWindowsForOverviewMode();

  // Calculate the position and scaling of all windows for overview mode
  // and record it in 'windows_'.
  void CalculateOverview();

  // Get the toplevel window whose image in overview mode covers the
  // passed-in position, or NULL if no such window exists.
  ToplevelWindow* GetOverviewToplevelWindowAtPoint(int x, int y) const;

  // Does the passed-in point lie inside of 'tab_summary_'?
  bool PointIsInTabSummary(int x, int y) const;

  // Does the passed-in point's position (well, currently just its Y
  // component) lie in the region between 'magnified_window_' and
  // 'tab_summary_'?
  bool PointIsBetweenMagnifiedToplevelWindowAndTabSummary(int x, int y) const;

  // Add or remove the relevant key bindings for the passed-in mode.
  void AddKeyBindingsForMode(Mode mode);
  void RemoveKeyBindingsForMode(Mode mode);

  // Cycle the active toplevel window.  Only makes sense in active mode.
  void CycleActiveToplevelWindow(bool forward);

  // Cycle the magnified toplevel window.  Only makes sense in overview mode.
  void CycleMagnifiedToplevelWindow(bool forward);

  // Set 'magnified_toplevel_' to the passed-in toplevel window (which can
  // be NULL to disable magnification).  Also takes care of telling the
  // previously-magnified window to hide its tabs (but not telling the new
  // window to show its tabs; we want to include the window's position in
  // those messages, so SendTabSummaryMessage() should be explicitly called
  // after ArrangeOverview() is called).
  void SetMagnifiedToplevelWindow(ToplevelWindow* toplevel);

  // Tell a toplevel window to show or hide its tab summary.  Does nothing
  // if 'toplevel' isn't a Chrome window.
  void SendTabSummaryMessage(ToplevelWindow* toplevel, bool show);

  // Send a message to a window describing the current state of 'mode_'.
  // Does nothing if 'win' isn't a toplevel Chrome window.
  void SendModeMessage(ToplevelWindow* toplevel);

  // Ask the active window to delete itself.
  void SendDeleteRequestToActiveWindow();

  WindowManager* wm_;  // not owned

  // The current mode.
  Mode mode_;

  // Area available to us for placing windows.
  int x_;
  int y_;
  int width_;
  int height_;

  // Maximum height of windows in overview mode.
  int overview_height_;

  // Information about toplevel windows, stored in the order in which
  // we'll display them in overview mode.
  typedef deque<ref_ptr<ToplevelWindow> > ToplevelWindows;
  ToplevelWindows toplevels_;

  // Map from input window to the toplevel window it represents.
  map<XWindow, ToplevelWindow*> input_to_toplevel_;

  // Currently-magnified toplevel window in overview mode, or NULL if no
  // window is magnified.
  ToplevelWindow* magnified_toplevel_;

  // Currently-active toplevel window in active mode.
  ToplevelWindow* active_toplevel_;

  // Floating window containing a tab that's being dragged around and the
  // toplevel Chrome window currently underneath it.
  Window* floating_tab_;
  ToplevelWindow* toplevel_under_floating_tab_;

  // Most recently-created tab summary window.
  Window* tab_summary_;

  // Window that when clicked creates a new browser.  Only shown in
  // overview mode, and may be NULL.
  Window* create_browser_window_;

  // We save the requested positions for the floating tab here so we can
  // apply them periodically in MoveFloatingTab().
  scoped_ptr<MotionEventCoalescer> floating_tab_event_coalescer_;

  Metrics metrics_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace chromeos

#endif
