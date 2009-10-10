// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_WINDOW_MANAGER_WINDOW_H__
#define __PLATFORM_WINDOW_MANAGER_WINDOW_H__

extern "C" {
#include <X11/Xlib.h>
}
#include <glog/logging.h>
#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/wm_ipc.h"

typedef ::Window XWindow;

namespace chromeos {

class Shadow;
template<class T> class Stacker;  // from util.h
class WindowManager;

// A client window.
//
// Because we use Xcomposite, there are (at least) two locations for a
// given window that we need to keep track of:
//
// - Where the client window is actually located on the X server.  This is
//   relevant for input -- we shape the compositing overlay window so that
//   events fall through it to the client windows underneath.
// - Where the window gets drawn on the compositing overlay window.  It'll
//   typically just be drawn in the same location as the actual X window,
//   but we could also e.g. draw a scaled-down version of it in a different
//   location.
//
// These two locations are not necessarily the same.  When animating a
// window move, it may be desireable to just move the X window once to the
// final location and then animate the move on the overlay.  As a result,
// there are different sets of methods to manipulate the client window and
// the composited window.
class Window {
 public:
  Window(WindowManager* wm, XWindow xid);

  // The destructor also cleans up any transient-for references to or from
  // this window.
  ~Window();

  XWindow xid() const { return xid_; }
  ClutterInterface::Actor* actor() { return actor_.get(); }
  const Shadow* shadow() const { return shadow_.get(); }
  Window* transient_for_window() const { return transient_for_window_; }
  bool override_redirect() const { return override_redirect_; }
  WmIpc::WindowType type() const { return type_; }
  WmIpc::WindowType* mutable_type() { return &type_; }
  const vector<int>& type_params() const { return type_params_; }
  vector<int>* mutable_type_params() { return &type_params_; }
  bool mapped() const { return mapped_; }
  void set_mapped(bool mapped) { mapped_ = mapped; }
  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }
  bool shaped() const { return shaped_; }

  int client_x() const { return client_x_; }
  int client_y() const { return client_y_; }
  int client_width() const { return client_width_; }
  int client_height() const { return client_height_; }

  bool composited_shown() const { return composited_shown_; }
  int composited_x() const { return composited_x_; }
  int composited_y() const { return composited_y_; }
  double composited_scale_x() const { return composited_scale_x_; }
  double composited_scale_y() const { return composited_scale_y_; }
  double composited_opacity() const { return composited_opacity_; }

  const string& title() const { return title_; }
  void set_title(const string& title) {
    VLOG(1) << "Setting " << xid_ << "'s title to \"" << title << "\"";
    title_ = title;
  }

  bool wm_state_fullscreen() const { return wm_state_fullscreen_; }
  bool wm_state_modal() const { return wm_state_modal_; }

  // Add or remove the passed-in window from 'transient_windows_'.
  void AddTransientWindow(Window* win);
  void RemoveTransientWindow(Window* win);

  // Get and apply hints that have been set for the client window.
  bool FetchAndApplySizeHints();
  bool FetchAndApplyTransientHint();

  // Update the window based on its Chrome OS window type property.
  // If 'update_shadow' is true, add or remove a drop shadow as needed.
  bool FetchAndApplyWindowType(bool update_shadow);

  // Update the window's opacity in response to the current value of its
  // _NET_WM_WINDOW_OPACITY property.
  void FetchAndApplyWindowOpacity();

  // Fetch the window's WM_PROTOCOLS property (ICCCM 4.1.2.7) if it exists
  // and update 'supports_wm_take_focus_'.
  void FetchAndApplyWmProtocols();

  // Fetch the window's _NET_WM_STATE property and update our internal copy
  // of it.  ClientMessage events should be used to update the states of mapped
  // windows, so this is primarily useful for getting the initial state of the
  // window before it's been mapped.
  void FetchAndApplyWmState();

  // Check if the window has been shaped using the Shape extension and
  // update its Clutter actor accordingly.  If 'update_shadow' is true, add
  // or remove a drop shadow as needed.
  void FetchAndApplyShape(bool update_shadow);

  // Handle a _NET_WM_STATE message about this window.  Updates our internal
  // copy of the state and the window's _NET_WM_STATE property.
  bool HandleWmStateMessage(const XClientMessageEvent& event);

  // If this window has a mapped, modal transient window, return it.  (If
  // there are multiple ones, the topmost is returned.)
  Window* GetTopModalTransient();

  // If one of this window's transient windows is focused, return it.
  Window* GetFocusedTransient();

  // Give keyboard focus to the client window, using a WM_TAKE_FOCUS
  // message if the client supports it or a SetInputFocus request
  // otherwise.  (Note that the client doesn't necessarily need to accept
  // the focus if WM_TAKE_FOCUS is used; see ICCCM 4.1.7.)
  bool TakeFocus(Time timestamp);

  // If the window supports WM_DELETE_WINDOW messages, ask it to delete
  // itself.  Just does nothing and returns false otherwise.
  bool SendDeleteRequest(Time timestamp);

  // Add or remove passive a passive grab on button presses within this
  // window.  When any button is pressed, an active pointer grab will be
  // installed.
  bool AddPassiveButtonGrab();
  bool RemovePassiveButtonGrab();

  // Get the largest possible size for this window smaller than or equal to
  // the passed-in desired dimensions (while respecting any sizing hints it
  // supplied via the WM_NORMAL_HINTS property).
  void GetMaxSize(int desired_width, int desired_height,
                  int* width_out, int* height_out) const;

  // Tell the X server to map or unmap this window.
  bool MapClient();
  bool UnmapClient();

  // Update our internal copy of the client window's position.
  void SaveClientPosition(int x, int y) {
    client_x_ = x;
    client_y_ = y;
  }

  // Update our internal copy of the client window's dimensions.  We also
  // update the Clutter actor's dimensions -- it doesn't make sense for
  // it to be any size other than that of the client window that gets
  // copied to it (note that the comp. window's *scale* may be different).
  void SaveClientAndCompositedSize(int width, int height);

  // Ask the X server to move or resize the client window.  Also calls the
  // corresponding SetClient*() method on success.  Returns false on
  // failure.
  bool MoveClient(int x, int y);

  bool MoveClientOffscreen();
  bool MoveClientToComposited();

  // Center the client window over the passed-in window.
  bool CenterClientOverWindow(Window* owner);

  enum Gravity {
    GRAVITY_NORTHWEST = 0,
    GRAVITY_NORTHEAST,
    GRAVITY_SOUTHWEST,
    GRAVITY_SOUTHEAST,
  };
  bool ResizeClient(int width, int height, Gravity gravity);

  // Raise the client window to the top of the stacking order.
  bool RaiseClient();

  // Stack the client window directly above or below another window.
  bool StackClientAbove(XWindow sibling_xid);
  bool StackClientBelow(XWindow sibling_xid);

  // Make various changes to the composited window (and its shadow).
  void MoveComposited(int x, int y, int anim_ms);
  void MoveCompositedX(int x, int anim_ms);
  void MoveCompositedY(int y, int anim_ms);
  void ShowComposited();
  void HideComposited();
  void SetCompositedOpacity(double opacity, int anim_ms);
  void ScaleComposited(double scale_x, double scale_y, int anim_ms);

  // Move and scale one of our transient window's actors to have the
  // correct relative position and same scale as us.
  void MoveAndScaleCompositedTransientWindow(
      Window* transient_win, int anim_ms);

  // Change the opacity of the window's shadow, overriding any previous
  // setting from SetCompositedOpacity().  This just temporarily changes
  // the opacity; the next call to SetCompositedOpacity() will restore the
  // shadow's opacity to the composited window's.
  void SetShadowOpacity(double opacity, int anim_ms);

  // Stack the window directly above 'actor' and its shadow directly below
  // 'shadow_actor' if supplied or below the window otherwise.  If 'actor'
  // is NULL, the window's stacking isn't changed (but its shadow's still
  // is).
  void StackCompositedAbove(ClutterInterface::Actor* actor,
                            ClutterInterface::Actor* shadow_actor);

  // Stack the window directly below 'actor' and its shadow directly below
  // 'shadow_actor' if supplied or below the window otherwise.  If 'actor'
  // is NULL, the window's stacking isn't changed (but its shadow's still
  // is).
  void StackCompositedBelow(ClutterInterface::Actor* actor,
                            ClutterInterface::Actor* shadow_actor);

 private:
  FRIEND_TEST(WindowTest, TransientFor);

  // Set the window for which this window is transient (or set it to not be
  // transient, if NULL is passed in).  Calls RemoveTransientWindow() on
  // the previous owner and AddTransientWindow() on the new one.  Also
  // repositions the transient window over its owner and scales it.
  void SetTransientForWindow(Window* win);

  // Record a transient window's position relative to us in
  // 'transient_window_positions_'.
  void SaveTransientWindowPosition(Window* transient_win);

  // Iterate over 'transient_windows_', invoking each window's
  // MoveAndScaleCompositedAsTransient() method.
  void MoveAndScaleCompositedTransientWindows(int anim_ms);

  // Stack all transient windows' client or composited windows on top of us
  // in the proper order.
  void RestackClientTransientWindows();
  void RestackCompositedTransientWindows();

  // Hide or show the window's shadow if necessary.
  void UpdateShadowIfNecessary();

  // Helper method for HandleWmStateMessage().  Given an action from a
  // _NET_WM_STATE message (i.e. the XClientMessageEvent's data.l[0] field),
  // updates 'value' accordingly.
  void SetWmStateInternal(int action, bool* value);

  XWindow xid_;
  WindowManager* wm_;
  scoped_ptr<ClutterInterface::TexturePixmapActor> actor_;
  scoped_ptr<Shadow> shadow_;

  // The window for which this window is transient, or NULL if it isn't
  // transient.
  Window* transient_for_window_;

  // Windows which are transient for this window.  We track the order in
  // which they should be stacked.
  scoped_ptr<Stacker<Window*> > transient_windows_;

  // Positions for transient windows, stored as offsets from the top left
  // corner of this window.
  map<XWindow, pair<int, int> > transient_window_positions_;

  // Was override-redirect set when the window was originally created?
  bool override_redirect_;

  // Is the client window currently mapped?
  bool mapped_;

  // Does the client window have the input focus?
  // Note that this is set to true in response to calls to TakeFocus() but
  // only set to false after receiving FocusOut events from the X server,
  // so there will be points in time at which multiple windows claim to be
  // focused.
  bool focused_;

  // Is the window shaped (using the Shape extension)?
  bool shaped_;

  // Client-supplied window type.
  WmIpc::WindowType type_;

  // Parameters associated with 'type_'.  See WmIpc::WindowType for
  // details.
  vector<int> type_params_;

  // Position and size of the client window.
  int client_x_;
  int client_y_;
  int client_width_;
  int client_height_;

  // Client-requested opacity (via _NET_WM_WINDOW_OPACITY).
  double client_opacity_;

  bool composited_shown_;
  int composited_x_;
  int composited_y_;
  double composited_scale_x_;
  double composited_scale_y_;
  double composited_opacity_;

  // Current shadow opacity.  Usually just 'client_opacity_' *
  // 'composited_opacity_', but can be overrided temporarily via
  // SetShadowOpacity().
  double shadow_opacity_;

  string title_;

  // Information from the WM_NORMAL_HINTS property (-1 if not set).
  int min_width_hint_;
  int min_height_hint_;
  int max_width_hint_;
  int max_height_hint_;
  int base_width_hint_;
  int base_height_hint_;
  int width_inc_hint_;
  int height_inc_hint_;

  // Does the window have a WM_PROTOCOLS property claiming that it supports
  // WM_TAKE_FOCUS or WM_DELETE_WINDOW messages?
  bool supports_wm_take_focus_;
  bool supports_wm_delete_window_;

  // EWMH window state, as set by _NET_WM_STATE client messages and exposed
  // in the window's _NET_WM_STATE property.
  bool wm_state_fullscreen_;
  bool wm_state_modal_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace chromeos

#endif
