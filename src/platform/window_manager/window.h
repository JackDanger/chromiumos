// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_WINDOW_H_
#define WINDOW_MANAGER_WINDOW_H_

extern "C" {
#include <X11/Xlib.h>
}
#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro
#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "chromeos/obsolete_logging.h"
#include "window_manager/atom_cache.h"  // for Atom enum
#include "window_manager/clutter_interface.h"
#include "window_manager/wm_ipc.h"
#include "window_manager/x_connection.h"

namespace window_manager {

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
// window move, it may be desirable to just move the X window once to the
// final location and then animate the move on the overlay.  As a result,
// there are different sets of methods to manipulate the client window and
// the composited window.
class Window {
 public:
  Window(WindowManager* wm, XWindow xid, bool override_redirect);
  ~Window();

  XWindow xid() const { return xid_; }
  const std::string& xid_str() const { return xid_str_; }
  ClutterInterface::Actor* actor() { return actor_.get(); }
  const Shadow* shadow() const { return shadow_.get(); }
  bool using_shadow() const { return using_shadow_; }
  XWindow transient_for_xid() const { return transient_for_xid_; }
  bool override_redirect() const { return override_redirect_; }
  WmIpc::WindowType type() const { return type_; }
  WmIpc::WindowType* mutable_type() { return &type_; }
  const std::vector<int>& type_params() const { return type_params_; }
  std::vector<int>* mutable_type_params() { return &type_params_; }
  bool mapped() const { return mapped_; }
  void set_mapped(bool mapped) { mapped_ = mapped; }
  bool focused() const { return focused_; }
  void set_focused(bool focused) { focused_ = focused; }
  bool shaped() const { return shaped_; }
  bool redirected() const { return redirected_; }

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

  const std::string& title() const { return title_; }
  void SetTitle(const std::string& title);

  bool wm_state_fullscreen() const { return wm_state_fullscreen_; }
  bool wm_state_modal() const { return wm_state_modal_; }

  // Redirect the client window for compositing.  This should be called once
  // after we're sure that we're going to display the window (i.e. after it's
  // been mapped).  Otherwise, there's a potential race for Flash windows -- see
  // http://code.google.com/p/chromium-os/issues/detail?id=1151 .
  void Redirect();

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

  // Fetch the window's _CHROME_STATE property and update our internal copy
  // of it.
  void FetchAndApplyChromeState();

  // Check if the window has been shaped using the Shape extension and
  // update its Clutter actor accordingly.  If 'update_shadow' is true, add
  // or remove a drop shadow as needed.
  void FetchAndApplyShape(bool update_shadow);

  // Query the X server to see if this window is currently mapped or not.
  // This should only be used for checking the state of an existing window
  // at startup; use mapped() after that.
  bool FetchMapState();

  // Handle a _NET_WM_STATE message about this window.  Updates our internal
  // copy of the state and the window's _NET_WM_STATE property.
  bool HandleWmStateMessage(const XClientMessageEvent& event);

  // Set or unset _NET_WM_STATE values for this window.  Note that this is
  // for WM-initiated state changes -- client-initiated changes come in
  // through HandleWmStateMessage().
  bool ChangeWmState(const std::vector<std::pair<XAtom, bool> >& states);

  // Set or unset particular _CHROME_STATE values for this window (each
  // atom's bool value states whether it should be added or removed).
  // Other existing values in the property remain unchanged.
  bool ChangeChromeState(const std::vector<std::pair<XAtom, bool> >& states);

  // Give keyboard focus to the client window, using a WM_TAKE_FOCUS
  // message if the client supports it or a SetInputFocus request
  // otherwise.  (Note that the client doesn't necessarily need to accept
  // the focus if WM_TAKE_FOCUS is used; see ICCCM 4.1.7.)
  bool TakeFocus(Time timestamp);

  // If the window supports WM_DELETE_WINDOW messages, ask it to delete
  // itself.  Just does nothing and returns false otherwise.
  bool SendDeleteRequest(Time timestamp);

  // Add or remove passive a passive grab on button presses within this
  // window.  When any button is pressed, a _synchronous_ active pointer
  // grab will be installed.  Note that this means that no pointer events
  // will be received until the pointer grab is manually removed using
  // XConnection::RemovePointerGrab() -- this can be used to ensure that
  // the client receives the initial click on its window when implementing
  // click-to-focus behavior.
  bool AddButtonGrab();
  bool RemoveButtonGrab();

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

  // Change the opacity of the window's shadow, overriding any previous
  // setting from SetCompositedOpacity().  This just temporarily changes
  // the opacity; the next call to SetCompositedOpacity() will restore the
  // shadow's opacity to the composited window's.
  void SetShadowOpacity(double opacity, int anim_ms);

  // Stack the window directly above 'actor' and its shadow directly above
  // or below 'shadow_actor' if supplied or below the window otherwise.  If
  // 'actor' is NULL, the window's stacking isn't changed (but its shadow's
  // still is).  If 'shadow_actor' is supplied, 'stack_above_shadow_actor'
  // determines whether the shadow will be stacked above or below it.
  void StackCompositedAbove(ClutterInterface::Actor* actor,
                            ClutterInterface::Actor* shadow_actor,
                            bool stack_above_shadow_actor);

  // Stack the window directly below 'actor' and its shadow directly above
  // or below 'shadow_actor' if supplied or below the window otherwise.  If
  // 'actor' is NULL, the window's stacking isn't changed (but its shadow's
  // still is).  If 'shadow_actor' is supplied, 'stack_above_shadow_actor'
  // determines whether the shadow will be stacked above or below it.
  void StackCompositedBelow(ClutterInterface::Actor* actor,
                            ClutterInterface::Actor* shadow_actor,
                            bool stack_above_shadow_actor);

  // Return this window's bottom-most actor (either the window's shadow's
  // group, or its actor itself if there's no shadow).  This is useful for
  // stacking another actor underneath this window.
  ClutterInterface::Actor* GetBottomActor();

 private:
  // Hide or show the window's shadow if necessary.
  void UpdateShadowIfNecessary();

  // Helper method for HandleWmStateMessage() and ChangeWmState().  Given
  // an action from a _NET_WM_STATE message (i.e. the XClientMessageEvent's
  // data.l[0] field), updates 'value' accordingly.
  void SetWmStateInternal(int action, bool* value);

  // Update the window's _NET_WM_STATE property based on the current values
  // of the 'wm_state_*' members.
  bool UpdateWmStateProperty();

  // Update the window's _CHROME_STATE property based on the current
  // contents of 'chrome_state_atoms_'.
  bool UpdateChromeStateProperty();

  XWindow xid_;
  std::string xid_str_;  // hex for debugging
  WindowManager* wm_;
  scoped_ptr<ClutterInterface::TexturePixmapActor> actor_;
  scoped_ptr<Shadow> shadow_;

  // The XID that this window says it's transient for.  Note that the
  // client can arbitrarily supply an ID here; the window doesn't
  // necessarily exist.  A good general practice may be to examine this
  // value when the window is mapped and ignore any changes after that.
  XWindow transient_for_xid_;

  // Was override-redirect set when the window was originally created?
  bool override_redirect_;

  // Is the client window currently mapped?  This is only updated when the
  // Window object is first created and when a MapNotify or UnmapNotify
  // event is received (dependent on the receiver calling set_mapped()
  // appropriately), so e.g. a call to MapClient() will not be immediately
  // reflected in this variable.
  bool mapped_;

  // Does the client window have the input focus?
  // Note that this is set to true in response to calls to TakeFocus() but
  // only set to false after receiving FocusOut events from the X server,
  // so there will be points in time at which multiple windows claim to be
  // focused.
  bool focused_;

  // Is the window shaped (using the Shape extension)?
  bool shaped_;

  // Has the window been redirected for compositing already?
  bool redirected_;

  // Client-supplied window type.
  WmIpc::WindowType type_;

  // Parameters associated with 'type_'.  See WmIpc::WindowType for
  // details.
  std::vector<int> type_params_;

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

  // Are we currently displaying a drop shadow beneath this window?
  bool using_shadow_;

  // Current shadow opacity.  Usually just 'client_opacity_' *
  // 'composited_opacity_', but can be overrided temporarily via
  // SetShadowOpacity().
  double shadow_opacity_;

  std::string title_;

  // Information from the WM_NORMAL_HINTS property.
  XConnection::SizeHints size_hints_;

  // Does the window have a WM_PROTOCOLS property claiming that it supports
  // WM_TAKE_FOCUS or WM_DELETE_WINDOW messages?
  bool supports_wm_take_focus_;
  bool supports_wm_delete_window_;

  // EWMH window state, as set by _NET_WM_STATE client messages and exposed
  // in the window's _NET_WM_STATE property.
  // TODO: Just store these in a set like we do for _CHROME_STATE below.
  bool wm_state_fullscreen_;
  bool wm_state_maximized_horz_;
  bool wm_state_maximized_vert_;
  bool wm_state_modal_;

  // Chrome window state, as exposed in the window's _CHROME_STATE
  // property.
  std::set<XAtom> chrome_state_xatoms_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_WINDOW_H_
