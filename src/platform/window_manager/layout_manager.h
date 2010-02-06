// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_LAYOUT_MANAGER_H_
#define WINDOW_MANAGER_LAYOUT_MANAGER_H_

extern "C" {
#include <X11/Xlib.h>
}
#include <deque>
#include <glib.h>  // for guint
#include <map>
#include <string>
#include <tr1/memory>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/key_bindings.h"
#include "window_manager/window.h"
#include "window_manager/wm_ipc.h"  // for WmIpc::Message

typedef ::Window XWindow;

namespace chrome_os_pb {
class SystemMetrics;
}

namespace window_manager {

class MotionEventCoalescer;
class Window;
class WindowManager;
template<class T> class Stacker;  // from util.h

// Manages the placement of regular client windows.
//
// It currently supports two modes: "active", where a single toplevel
// window is displayed at full scale and given the input focus, and
// "overview", where scaled-down copies of all toplevel windows are
// displayed across the bottom of the screen.
//
class LayoutManager : public EventConsumer {
 public:
  // 'x', 'y', 'width', and 'height' specify the area available for
  // displaying client windows.  Because of the way that overview mode is
  // currently implemented, this should ideally be flush with the bottom of
  // the screen.
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
    void Populate(chrome_os_pb::SystemMetrics* metrics_pb);

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
  int overview_panning_offset() const { return overview_panning_offset_; }

  // Returns a pointer to the struct in which LayoutManager tracks
  // relevant user metrics.
  Metrics* GetMetrics() { return &metrics_; }

  // Note: Begin overridden EventConsumer methods.

  // Is the passed-in window an input window?
  bool IsInputWindow(XWindow xid) ;

  // Handle a window's map request.  In most cases, we just restack the
  // window, move it offscreen, and map it (info bubbles don't get moved,
  // though).
  bool HandleWindowMapRequest(Window* win);

  // Handle a new window.  This method takes care of moving the client
  // window offscreen so it doesn't get input events and of redrawing the
  // layout if necessary.
  void HandleWindowMap(Window* win);

  // Handle the removal of a window.
  void HandleWindowUnmap(Window* win);

  // Handle a client window's request to get moved or resized.
  bool HandleWindowConfigureRequest(
      Window* win, int req_x, int req_y, int req_width, int req_height);

  // Handle events received by windows.
  bool HandleButtonPress(XWindow xid,
                         int x, int y,
                         int x_root, int y_root,
                         int button,
                         Time timestamp);
  bool HandleButtonRelease(XWindow xid,
                           int x, int y,
                           int x_root, int y_root,
                           int button,
                           Time timestamp);
  bool HandlePointerEnter(XWindow xid,
                          int x, int y,
                          int x_root, int y_root,
                          Time timestamp);
  bool HandlePointerLeave(XWindow xid,
                          int x, int y,
                          int x_root, int y_root,
                          Time timestamp);
  bool HandlePointerMotion(XWindow xid,
                           int x, int y,
                           int x_root, int y_root,
                           Time timestamp);
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
  FRIEND_TEST(LayoutManagerTest, OverviewFocus);

  // A toplevel window that we're managing.
  // TODO: This class is getting large.  It should probably be moved to a
  // separate file.
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

    // Get the absolute X-position of the window's center.
    int GetAbsoluteOverviewCenterX() const {
      return layout_manager_->x() + overview_x_ + 0.5 * overview_width_;
    }

    // Get the absolute Y-position to place a window directly below the
    // layout manager's region.
    int GetAbsoluteOverviewOffscreenY() const {
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

    // Get the absolute position of the window for overview mode.
    int GetAbsoluteOverviewX() const;
    int GetAbsoluteOverviewY() const;

    // Configure the window for active mode.  This involves either moving
    // the client window on- or offscreen (depending on
    // 'window_is_active'), animating the composited window according to
    // 'state_', and possibly focusing the window or one of its transients.
    // 'to_left_of_active' describes whether the window is to the left of
    // the active window or not.  If 'update_focus' is true, the window
    // will take the focus if it's active.
    void ConfigureForActiveMode(bool window_is_active,
                                bool to_left_of_active,
                                bool update_focus);

    // Configure the window for overview mode.  This involves animating its
    // composited position and scale as specified by 'overview_*' and
    // 'state_', moving its client window offscreen (so it won't receive
    // mouse events), and moving its input window onscreen.  If
    // 'incremental' is true, this is assumed to be an incremental change
    // being done in response to e.g. a mouse drag, so we will skip doing
    // things like animating changes and restacking windows.
    void ConfigureForOverviewMode(bool window_is_magnified,
                                  bool dim_if_unmagnified,
                                  ToplevelWindow* toplevel_to_stack_under,
                                  bool incremental);

    // Set 'overview_x_' and 'overview_y_' to the passed-in values.
    void UpdateOverviewPosition(int x, int y) {
      overview_x_ = x;
      overview_y_ = y;
    }

    // Update 'overview_width_', 'overview_height_', and 'overview_scale_'
    // for our composited window such that it fits in the dimensions
    // 'max_width' and 'max_height'.
    void UpdateOverviewScaling(int max_width, int max_height);

    // Focus 'transient_to_focus_' if non-NULL or 'win_' otherwise.  Also
    // raises the transient window to the top of the stacking order.
    void TakeFocus(Time timestamp);

    // Set the window to be focused the next time that TakeFocus() is
    // called.  NULL can be passed to indicate that the toplevel window
    // should get the focus.  Note that this request may be ignored if a
    // modal transient window already has the focus.
    void SetPreferredTransientWindowToFocus(Window* transient_win);

    // Does the toplevel window or one of its transients have the input focus?
    bool IsWindowOrTransientFocused() const;

    // Add a transient window.  Called in response to the window being
    // mapped.
    void AddTransientWindow(Window* transient_win);

    // Remove a transient window.  Called in response to the window being
    // unmapped.
    void RemoveTransientWindow(Window* transient_win);

    // Handle a ConfigureRequest event about one of our transient windows.
    void HandleTransientWindowConfigureRequest(
        Window* transient_win,
        int req_x, int req_y, int req_width, int req_height);

    // Handle one of this toplevel's windows (either the toplevel window
    // itself or one of its transients) gaining or losing the input focus.
    void HandleFocusChange(Window* focus_win, bool focus_in);

    // Handle one of this toplevel's windows (either the toplevel window
    // itself or one of its transients) getting a button press.  We remove
    // the active pointer grab and try to assign the focus to the
    // clicked-on window.
    void HandleButtonPress(Window* button_win, Time timestamp);

   private:
    // A transient window belonging to a toplevel window.
    struct TransientWindow {
     public:
      TransientWindow(Window* win)
          : win(win),
            x_offset(0),
            y_offset(0),
            centered(false) {
      }
      ~TransientWindow() {
        win = NULL;
      }

      // Save the transient window's current offset from its owner.
      void SaveOffsetsRelativeToOwnerWindow(Window* owner_win) {
        x_offset = win->client_x() - owner_win->client_x();
        y_offset = win->client_y() - owner_win->client_y();
      }

      // Update offsets so the transient will be centered over the
      // passed-in owner window.
      void UpdateOffsetsToCenterOverOwnerWindow(Window* owner_win) {
        x_offset = 0.5 * (owner_win->client_width() - win->client_width());
        y_offset = 0.5 * (owner_win->client_height() - win->client_height());
      }

      // The transient window itself.
      Window* win;

      // Transient window's position's offset from its owner's origin.
      int x_offset;
      int y_offset;

      // Is the transient window centered over its owner?  We set this when
      // we first center a transient window but remove it if the client
      // ever moves the transient itself.
      bool centered;
    };

    WindowManager* wm() { return layout_manager_->wm_; }

    // Get the TransientWindow struct representing the passed-in window.
    TransientWindow* GetTransientWindow(const Window& win);

    // Update the passed-in transient window's client and composited
    // windows appropriately for the toplevel window's current
    // configuration.
    void MoveAndScaleTransientWindow(TransientWindow* transient, int anim_ms);

    // Call UpdateTransientWindowPositionAndScale() for all transient
    // windows.
    void MoveAndScaleAllTransientWindows(int anim_ms);

    // Stack a transient window's composited and client windows on top of
    // another window.
    static void ApplyStackingForTransientWindowAboveWindow(
        TransientWindow* transient, Window* other_win);

    // Restack all transient windows' composited and client windows on top
    // of 'win_' in the order dictated by 'stacked_transients_'.
    void ApplyStackingForAllTransientWindows();

    // Choose a new transient window to focus.  We choose the topmost modal
    // window if there is one; otherwise we just return the topmost
    // transient, or NULL if there aren't any transients.
    TransientWindow* FindTransientWindowToFocus() const;

    // Move a transient window to the top of this toplevel's stacking
    // order, if it's not already there.  Updates the transient's position
    // in 'stacked_transients_' and also restacks its composited and client
    // windows.
    void RestackTransientWindowOnTop(TransientWindow* transient);

    // Static texture actor that we clone for each actor to draw a gradient
    // over windows when they're inactive.
    static ClutterInterface::Actor* static_gradient_texture_;

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
    // window in overview mode.  The X and Y coordinates are relative to
    // the layout manager's origin.
    int overview_x_;
    int overview_y_;
    int overview_width_;
    int overview_height_;
    double overview_scale_;

    // Cloned copy of 'static_gradient_texture_'.
    scoped_ptr<ClutterInterface::Actor> gradient_actor_;

    // Transient windows belonging to this toplevel window, keyed by XID.
    std::map<XWindow, std::tr1::shared_ptr<TransientWindow> > transients_;

    // Transient windows in top-to-bottom stacking order.
    scoped_ptr<Stacker<TransientWindow*> > stacked_transients_;

    // Transient window that should be focused when TakeFocus() is called,
    // or NULL if the toplevel window should be focused.
    TransientWindow* transient_to_focus_;

    DISALLOW_COPY_AND_ASSIGN(ToplevelWindow);
  };

  // Is the passed-in window type one that we should handle?
  static bool IsHandledWindowType(WmIpc::WindowType type);

  // Get the toplevel window represented by the passed-in input window, or
  // NULL if the input window doesn't belong to us.
  ToplevelWindow* GetToplevelWindowByInputXid(XWindow xid);

  // Get the 0-based index of the passed-in toplevel within 'toplevels_'.
  // Returns -1 if it isn't present.
  int GetIndexForToplevelWindow(const ToplevelWindow& toplevel) const;

  // Get the ToplevelWindow object representing the passed-in window.
  // Returns NULL if it isn't a toplevel window.
  ToplevelWindow* GetToplevelWindowByWindow(const Window& win);

  // Get the ToplevelWindow object representing the window with the
  // passed-in XID.  Returns NULL if the window doesn't exist or isn't a
  // toplevel window.
  ToplevelWindow* GetToplevelWindowByXid(XWindow xid);

  // Get the ToplevelWindow object that owns the passed-in
  // possibly-transient window.  Returns NULL if the window is unowned.
  ToplevelWindow* GetToplevelWindowOwningTransientWindow(const Window& win);

  // Get the XID of the input window created for a toplevel window.  This
  // is just used by testing code.
  XWindow GetInputXidForWindow(const Window& win);

  // Do some initial setup for windows that we're going to manage.
  // This includes stacking them and moving them offscreen.
  void DoInitialSetupForWindow(Window* win);

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

  // Calculate toplevel windows' positions and move them there.
  void LayoutToplevelWindowsForActiveMode(bool update_focus);
  // 'magnified_x' is passed to CalculatePositionsForOverviewMode().
  void LayoutToplevelWindowsForOverviewMode(int magnified_x);

  // Calculate the position and scaling of all windows for overview mode
  // and record it in 'toplevels_'.  If 'magnified_x' (given relative to
  // the layout manager's origin) is non-negative,
  // 'overview_panning_offset_' is set such that the magnified window is as
  // close to centered as possible while still being positioned underneath
  // 'magnified_x'.  This is useful for ensuring that the magnified window
  // remains underneath the pointer.
  void CalculatePositionsForOverviewMode(int magnified_x);

  // Configure all toplevel windows for overview mode based on their
  // previously-calculated positions.  'incremental' is passed to
  // ToplevelWindow::ConfigureForOverviewMode().
  void ConfigureWindowsForOverviewMode(bool incremental);

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

  // Pan across the windows horizontally in overview mode.
  // 'offset' is applied relative to the current panning offset.
  void PanOverviewMode(int offset);

  // Update the panning in overview mode based on mouse motion stored in
  // 'overview_background_event_coalescer_'.  Invoked by a timer.
  void UpdateOverviewPanningForMotion();

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
  typedef std::deque<std::tr1::shared_ptr<ToplevelWindow> > ToplevelWindows;
  ToplevelWindows toplevels_;

  // Map from input windows to the toplevel windows they represent.
  std::map<XWindow, ToplevelWindow*> input_to_toplevel_;

  // Map from transient windows' XIDs to the toplevel windows that own
  // them.  This is based on the transient windows' WM_TRANSIENT_FOR hints
  // at the time that they were mapped; we ignore any subsequent changes to
  // this hint.
  std::map<XWindow, ToplevelWindow*> transient_to_toplevel_;

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

  // Amount that toplevel windows' positions should be offset to the left
  // for overview mode.  Used to implement panning.
  int overview_panning_offset_;

  // We save the requested positions for the floating tab here so we can
  // apply them periodically in MoveFloatingTab().
  scoped_ptr<MotionEventCoalescer> floating_tab_event_coalescer_;

  // Mouse pointer motion gets stored here during a drag on the background
  // window in overview mode so that it can be applied periodically in
  // UpdateOverviewPanningForMotion().
  scoped_ptr<MotionEventCoalescer> overview_background_event_coalescer_;

  // X component of the pointer's previous position during a drag on the
  // background window.
  int overview_drag_last_x_;

  Metrics metrics_;

  // Have we seen a MapRequest event yet?  We perform some initial setup
  // (e.g. stacking) in response to MapRequests, so we track this so we can
  // perform the same setup at the MapNotify point for windows that were
  // already mapped or were in the process of being mapped when we were
  // started.
  // TODO: This is yet another hack that could probably removed in favor of
  // something more elegant if/when we're sharing an X connection with
  // Clutter and can safely grab the server at startup.
  bool saw_map_request_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_LAYOUT_MANAGER_H_
