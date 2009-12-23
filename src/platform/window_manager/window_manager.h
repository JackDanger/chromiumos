// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_WINDOW_MANAGER_H_
#define WINDOW_MANAGER_WINDOW_MANAGER_H_

#include <map>
#include <set>
#include <vector>
#include <tr1/memory>

extern "C" {
#include <X11/extensions/shape.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>
}

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/scoped_ptr.h"
#include "window_manager/atom_cache.h"  // for Atom enum
#include "window_manager/clutter_interface.h"

typedef ::Atom XAtom;
typedef ::Window XWindow;

namespace window_manager {

class EventConsumer;
class HotkeyOverlay;
class KeyBindings;
class LayoutManager;
class MetricsReporter;
class PanelBar;
class StackingManager;
class Window;
class WmIpc;
class XConnection;
template<class T> class Stacker;

class WindowManager {
 public:
  WindowManager(XConnection* xconn, ClutterInterface* clutter);
  ~WindowManager();

  XConnection* xconn() { return xconn_; }
  ClutterInterface* clutter() { return clutter_; }
  StackingManager* stacking_manager() { return stacking_manager_.get(); }

  XWindow root() const { return root_; }

  ClutterInterface::StageActor* stage() { return stage_; }
  ClutterInterface::Actor* background() { return background_.get(); }

  int width() const { return width_; }
  int height() const { return height_; }

  XWindow active_window_xid() const { return active_window_xid_; }

  KeyBindings* key_bindings() { return key_bindings_.get(); }
  WmIpc* wm_ipc() { return wm_ipc_.get(); }

  // Get the title for the window that we create to take ownership of management
  // selections.  This is also used to name our log files.
  static const char* GetWmName() { return "chromeos-wm"; }

  bool Init();

  bool HandleEvent(XEvent* event);

  // Create a new X window for receiving input.
  XWindow CreateInputWindow(int x, int y, int width, int height);

  // Move and resize the passed-in window.
  // TODO: This isn't limited to input windows.
  bool ConfigureInputWindow(XWindow xid, int x, int y, int width, int height);

  // Get the X server's ID corresponding to the passed-in atom (the Atom
  // enum is defined in atom_cache.h).
  XAtom GetXAtom(Atom atom);

  // Get the name for an atom from the X server.
  const std::string& GetXAtomName(XAtom xatom);

  // Get the current time from the server.  This can be useful for e.g.
  // getting a timestamp to pass to XSetInputFocus() when triggered by an
  // event that doesn't contain a timestamp.
  Time GetCurrentTimeFromServer();

  // Look up a window in 'client_windows_', returning NULL if it isn't
  // present.
  Window* GetWindow(XWindow xid);

  // Locks the screen by calling xscreensaver-command.
  void LockScreen();

  // Enables and configures an external monitor.
  void ConfigureExternalMonitor();

  // Do something reasonable with the input focus.
  // This is intended to be called by EventConsumers when they give up the
  // focus and aren't sure what to do with it.
  void TakeFocus();

  // Adjust the size of the window layout when the panel bar is shown or
  // hidden.
  void HandlePanelBarVisibilityChange(bool visible);

  // Set the _NET_ACTIVE_WINDOW property, which contains the ID of the
  // currently-active window (in our case, this is the top-level window or
  // panel window that has the focus).
  bool SetActiveWindowProperty(XWindow xid);

 private:
  // Height for the panel bar.
  static const int kPanelBarHeight;

  friend class LayoutManagerTest;         // uses 'layout_manager_'
  friend class PanelTest;                 // uses 'panel_bar_'
  friend class PanelBarTest;              // uses 'panel_bar_'
  FRIEND_TEST(LayoutManagerTest, Basic);  // uses TrackWindow()
  FRIEND_TEST(WindowTest, TransientFor);  // uses TrackWindow()
  FRIEND_TEST(WindowManagerTest, RegisterExistence);
  FRIEND_TEST(WindowManagerTest, EventConsumer);
  FRIEND_TEST(WindowManagerTest, RandR);

  // Is this one of our internally-created windows?
  bool IsInternalWindow(XWindow xid) {
    return (xid == stage_xid_ || xid == overlay_xid_ || xid == wm_xid_);
  }

  // Get a manager selection as described in ICCCM section 2.8.  'atom' is
  // the selection to take, 'manager_win' is the window acquiring the
  // selection, and 'timestamp' is the current time.
  bool GetManagerSelection(
      XAtom atom, XWindow manager_win, Time timestamp);

  // Tell the previous window and compositing managers to exit and register
  // ourselves as the new managers.
  bool RegisterExistence();

  // Set various one-time/unchanging properties on the root window as
  // specified in the Extended Window Manager Hints.
  bool SetEwmhGeneralProperties();

  // Set EWMH properties on the root window relating to the current screen
  // size (as stored in 'width_' and 'height_').
  bool SetEwmhSizeProperties();

  // Create a new ClutterGroup directly above 'bottom_actor'.  This is used
  // to create all of the '_depth' actors that are used for stacking.
  ClutterInterface::Actor* CreateActorAbove(
      ClutterInterface::Actor* bottom_actor);

  // Query the X server for all top-level windows and start tracking (and
  // possibly managing) them.
  bool ManageExistingWindows();

  // Start tracking this window (more specifically, create a Window object
  // for it and register it in 'client_windows_').  Returns NULL for
  // windows that we specifically shouldn't track (e.g. the Clutter stage
  // or the compositing overlay window).
  Window* TrackWindow(XWindow xid);

  // Handle a window getting mapped.  This is primarily used by
  // HandleMapNotify(), but is abstracted out into a separate method so
  // that ManageExistingWindows() can also use it to handle windows that
  // were already mapped when the WM started.
  void HandleMappedWindow(Window* win);

  // Set the WM_STATE property on a window.  Per ICCCM 4.1.3.1, 'state' can
  // be 0 (WithdrawnState), 1 (NormalState), or 3 (IconicState).  Per
  // 4.1.4, IconicState means that the top-level window isn't viewable, so
  // we should use NormalState even when drawing a scaled-down version of
  // the window.
  bool SetWmStateProperty(XWindow xid, int state);

  // Update the _NET_CLIENT_LIST and _NET_CLIENT_LIST_STACKING properties
  // on the root window (as described in EWMH).
  bool UpdateClientListProperty();
  bool UpdateClientListStackingProperty();

  // Handlers for various X events.
  bool HandleButtonPress(const XButtonEvent& e);
  bool HandleButtonRelease(const XButtonEvent& e);
  bool HandleClientMessage(const XClientMessageEvent& e);
  bool HandleConfigureNotify(const XConfigureEvent& e);
  bool HandleConfigureRequest(const XConfigureRequestEvent& e);
  bool HandleCreateNotify(const XCreateWindowEvent& e);
  bool HandleDestroyNotify(const XDestroyWindowEvent& e);
  bool HandleEnterNotify(const XEnterWindowEvent& e);
  bool HandleFocusChange(const XFocusChangeEvent& e);
  bool HandleKeyPress(const XKeyEvent& e);
  bool HandleKeyRelease(const XKeyEvent& e);
  bool HandleLeaveNotify(const XLeaveWindowEvent& e);
  bool HandleMapNotify(const XMapEvent& e);
  bool HandleMapRequest(const XMapRequestEvent& e);
  bool HandleMappingNotify(const XMappingEvent& e);
  bool HandleMotionNotify(const XMotionEvent& e);
  bool HandlePropertyNotify(const XPropertyEvent& e);
  bool HandleReparentNotify(const XReparentEvent& e);
  bool HandleRRScreenChangeNotify(const XRRScreenChangeNotifyEvent& e);
  bool HandleShapeNotify(const XShapeEvent& e);
  bool HandleUnmapNotify(const XUnmapEvent& e);

  // Callback when the hotkey for launching an xterm is pressed.
  void LaunchTerminalCallback();

  // Callback to show or hide debugging information about client windows.
  void ToggleClientWindowDebugging();

  // Callback to show or hide the hotkey overlay images.
  void ToggleHotkeyOverlay();

  // Write a screenshot to disk.  If 'use_active_window' is true, the
  // screenshot will contain the currently-active client window's offscreen
  // pixmap.  Otherwise, the composited image from the root window will be
  // captured.
  void TakeScreenshot(bool use_active_window);

  // Helper method called repeatedly by a GLib timeout while the hotkey
  // overlay is being displayed to query the current keyboard state from
  // the X server and pass it to the overlay.
  static gboolean QueryKeyboardStateThunk(gpointer data) {
    reinterpret_cast<WindowManager*>(data)->QueryKeyboardState();
    return TRUE;  // keep the timeout alive
  }
  void QueryKeyboardState();

  XConnection* xconn_;         // not owned
  ClutterInterface* clutter_;  // not owned

  XWindow root_;

  // Root window dimensions.
  int width_;
  int height_;

  // Offscreen window that we just use for registering as the WM.
  XWindow wm_xid_;

  ClutterInterface::StageActor* stage_;  // not owned
  scoped_ptr<ClutterInterface::Actor> background_;

  // Window containing the Clutter stage.
  XWindow stage_xid_;

  // XComposite overlay window.
  XWindow overlay_xid_;

  scoped_ptr<StackingManager> stacking_manager_;

  // Windows that are being tracked.
  std::map<XWindow, std::tr1::shared_ptr<Window> > client_windows_;

  // This is a list of mapped, managed (i.e. not override-redirect) client
  // windows, in most-to-least-recently-mapped order.  Used to set EWMH's
  // _NET_CLIENT_LIST property.
  scoped_ptr<Stacker<XWindow> > mapped_xids_;

  // All immediate children of the root window (even ones that we don't
  // "track", in the sense of having Window objects for them in
  // 'client_windows_') in top-to-bottom stacking order.  EWMH's
  // _NET_CLIENT_LIST_STACKING property contains the managed (i.e. not
  // override-redirect) windows from this list.
  scoped_ptr<Stacker<XWindow> > stacked_xids_;

  // Things that consume events (e.g. LayoutManager, PanelBar, etc.).
  std::set<EventConsumer*> event_consumers_;

  // Actors that are currently being used to debug client windows.
  std::vector<std::tr1::shared_ptr<ClutterInterface::Actor> >
      client_window_debugging_actors_;

  // The last window that was passed to SetActiveWindowProperty().
  XWindow active_window_xid_;

  scoped_ptr<AtomCache> atom_cache_;
  scoped_ptr<WmIpc> wm_ipc_;
  scoped_ptr<KeyBindings> key_bindings_;
  scoped_ptr<LayoutManager> layout_manager_;
  scoped_ptr<MetricsReporter> metrics_reporter_;
  scoped_ptr<PanelBar> panel_bar_;

  // GLib source ID for the timer that calls QueryKeyboardStateThunk().
  guint query_keyboard_state_timer_;

  // Is the hotkey overlay currently being shown?
  bool showing_hotkey_overlay_;

  // Shows overlayed images containing hotkeys.
  scoped_ptr<HotkeyOverlay> hotkey_overlay_;

  DISALLOW_COPY_AND_ASSIGN(WindowManager);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_WINDOW_MANAGER_H_
