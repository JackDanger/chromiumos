// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/window.h"

#include <algorithm>

#include <gflags/gflags.h>

#include "base/logging.h"
#include "chromeos/obsolete_logging.h"

#include "window_manager/atom_cache.h"
#include "window_manager/shadow.h"
#include "window_manager/util.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_bool(window_drop_shadows, true, "Display drop shadows under windows");

DECLARE_bool(wm_use_compositing);  // from window_manager.cc

namespace chromeos {

Window::Window(WindowManager* wm, XWindow xid)
    : xid_(xid),
      wm_(wm),
      actor_(wm_->clutter()->CreateTexturePixmap()),
      shadow_(NULL),
      transient_for_xid_(None),
      override_redirect_(false),
      mapped_(false),
      shaped_(false),
      type_(WmIpc::WINDOW_TYPE_UNKNOWN),
      client_x_(-1),
      client_y_(-1),
      client_width_(1),
      client_height_(1),
      client_opacity_(1.0),
      composited_shown_(false),
      composited_x_(-1),
      composited_y_(-1),
      composited_scale_x_(1.0),
      composited_scale_y_(1.0),
      composited_opacity_(1.0),
      shadow_opacity_(1.0),
      min_width_hint_(-1),
      min_height_hint_(-1),
      max_width_hint_(-1),
      max_height_hint_(-1),
      base_width_hint_(-1),
      base_height_hint_(-1),
      width_inc_hint_(-1),
      height_inc_hint_(-1),
      supports_wm_take_focus_(false),
      supports_wm_delete_window_(false),
      wm_state_fullscreen_(false),
      wm_state_maximized_horz_(false),
      wm_state_maximized_vert_(false),
      wm_state_modal_(false) {
  // Listen for focus, property, and shape changes on this window.
  wm_->xconn()->SelectInputOnWindow(
      xid_, FocusChangeMask|PropertyChangeMask, true);
  wm_->xconn()->SelectShapeEventsOnWindow(xid_);

  // Get the window's initial state.
  XWindowAttributes attr;
  if (wm_->xconn()->GetWindowAttributes(xid_, &attr)) {
    // We update 'mapped_' when we get the MapNotify event instead of doing
    // it here; things get tricky otherwise since there's a race as to
    // whether override-redirect windows are mapped or not at this point.
    override_redirect_ = attr.override_redirect;
    client_x_ = composited_x_ = attr.x;
    client_y_ = composited_y_ = attr.y;
    client_width_ = attr.width;
    client_height_ = attr.height;

    // If the window has a border, remove it -- they make things more confusing
    // (we need to include the border when telling Clutter the window's size,
    // but it's not included when telling X to resize the window, etc.).
    // TODO: Reconsider this if there's actually a case where borders are useful
    // to have.
    if (attr.border_width > 0)
      wm_->xconn()->SetWindowBorderWidth(xid_, 0);
  }

  if (FLAGS_wm_use_compositing) {
    // Send the window to an offscreen pixmap.
    // Something within clutter_x11_texture_pixmap_set_window() appears to
    // be triggering a BadAccess error when we don't sync with the server
    // after calling XCompositeRedirectWindow() (but we get the sync
    // implicitly from the error-trapping in
    // XConnection::RedirectWindowForCompositing()).
    VLOG(1) << "Redirecting "
            << (override_redirect_ ? "override-redirect " : "")
            << "window " << xid_ << " "
            << "at (" << client_x_ << ", " << client_y_ << ") "
            << "with dimensions " << client_width_ << "x" << client_height_
            << " for compositing";
    wm_->xconn()->RedirectWindowForCompositing(xid_);
  }

  if (!actor_->IsUsingTexturePixmapExtension()) {
    static bool logged = false;
    LOG_IF(WARNING, !logged) <<
        "Not using texture-from-pixmap extension -- expect slowness";
    logged = true;
  }
  actor_->SetTexturePixmapWindow(xid_);
  actor_->Move(composited_x_, composited_y_, 0);
  actor_->SetSize(client_width_, client_height_);
  actor_->SetVisibility(false);
  wm_->stage()->AddActor(actor_.get());

  // Properties could've been set on this window after it was created but
  // before we selected on PropertyChangeMask, so we need to query them
  // here.  Don't create a shadow yet; we still need to check if it's
  // shaped.
  FetchAndApplyWindowType(false);

  // Check if the window is shaped (and create a shadow for if needed).
  FetchAndApplyShape(true);

  // Check if the client window has set _NET_WM_WINDOW_OPACITY.
  FetchAndApplyWindowOpacity();

  // Apply the size hints, which may resize the actor.
  FetchAndApplySizeHints();

  // Load other properties that might've gotten set before we started
  // listening for property changes on the window.
  FetchAndApplyWmProtocols();
  FetchAndApplyWmState();
  FetchAndApplyTransientHint();
}

Window::~Window() {
}

bool Window::FetchAndApplySizeHints() {
  // Once windows have been mapped, they should just request any desired
  // changes themselves.
  if (mapped_)
    return true;

  XSizeHints size_hints;
  memset(&size_hints, 0, sizeof(size_hints));
  long supplied_hints = 0;
  if (!wm_->xconn()->GetSizeHintsForWindow(
           xid_, &size_hints, &supplied_hints)) {
    return false;
  }

  if (size_hints.flags & PMinSize) {
    min_width_hint_ = size_hints.min_width;
    min_height_hint_ = size_hints.min_height;
  } else {
    min_width_hint_ = -1;
    min_height_hint_ = -1;
  }
  if (size_hints.flags & PMaxSize) {
    max_width_hint_ = size_hints.max_width;
    max_height_hint_ = size_hints.max_height;
  } else {
    max_width_hint_ = -1;
    max_height_hint_ = -1;
  }
  if (size_hints.flags & PBaseSize) {
    base_width_hint_ = size_hints.base_width;
    base_height_hint_ = size_hints.base_height;
  } else {
    base_width_hint_ = -1;
    base_height_hint_ = -1;
  }
  if (size_hints.flags & PResizeInc) {
    width_inc_hint_ = size_hints.width_inc;
    height_inc_hint_ = size_hints.height_inc;
  } else {
    width_inc_hint_ = -1;
    height_inc_hint_ = -1;
  }
  VLOG(1) << "Size hints for " << xid_ << ":"
          << " min=" << min_width_hint_ << "x" << min_height_hint_
          << " max=" << max_width_hint_ << "x" << max_height_hint_
          << " base=" << base_width_hint_ << "x" << base_height_hint_
          << " inc=" << width_inc_hint_ << "x" << height_inc_hint_;

  // Ignore position, aspect ratio, etc. hints for now.
  if (!override_redirect_ &&
      (size_hints.flags & USSize || size_hints.flags & PSize)) {
    VLOG(1) << "Got " << (size_hints.flags & USSize ? "user" : "program")
            << "-specified size hints for " << xid_ << ": "
            << size_hints.width << "x" << size_hints.width;
    ResizeClient(size_hints.width, size_hints.height, GRAVITY_NORTHWEST);
  }

  return true;
}

bool Window::FetchAndApplyTransientHint() {
  if (!wm_->xconn()->GetTransientHintForWindow(xid_, &transient_for_xid_))
    return false;
  return true;
}

bool Window::FetchAndApplyWindowType(bool update_shadow) {
  bool result = wm_->wm_ipc()->GetWindowType(xid_, &type_, &type_params_);
  VLOG(1) << "Window " << xid_ << " has type " << type_;
  if (update_shadow)
    UpdateShadowIfNecessary();
  return result;
}

void Window::FetchAndApplyWindowOpacity() {
  static const uint32 kMaxOpacity = 0xffffffffU;

  uint32 opacity = kMaxOpacity;
  wm_->xconn()->GetIntProperty(
      xid_,
      wm_->GetXAtom(ATOM_NET_WM_WINDOW_OPACITY),
      reinterpret_cast<int32*>(&opacity));
  client_opacity_ = (opacity == kMaxOpacity) ?
      1.0 : static_cast<double>(opacity) / kMaxOpacity;

  // TODO: It'd be nicer if we didn't interrupt any in-progress opacity
  // animations.
  SetCompositedOpacity(composited_opacity_, 0);
}

void Window::FetchAndApplyWmProtocols() {
  supports_wm_take_focus_ = false;
  supports_wm_delete_window_ = false;

  std::vector<int> wm_protocols;
  if (!wm_->xconn()->GetIntArrayProperty(
          xid_, wm_->GetXAtom(ATOM_WM_PROTOCOLS), &wm_protocols)) {
    return;
  }

  XAtom wm_take_focus = wm_->GetXAtom(ATOM_WM_TAKE_FOCUS);
  XAtom wm_delete_window = wm_->GetXAtom(ATOM_WM_DELETE_WINDOW);
  for (std::vector<int>::const_iterator it = wm_protocols.begin();
       it != wm_protocols.end(); ++it) {
    if (static_cast<XAtom>(*it) == wm_take_focus) {
      VLOG(2) << "Window " << xid_ << " supports WM_TAKE_FOCUS";
      supports_wm_take_focus_ = true;
    } else if (static_cast<XAtom>(*it) == wm_delete_window) {
      VLOG(2) << "Window " << xid_ << " supports WM_DELETE_WINDOW";
      supports_wm_delete_window_ = true;
    }
  }
}

void Window::FetchAndApplyWmState() {
  wm_state_fullscreen_ = false;
  wm_state_maximized_horz_ = false;
  wm_state_maximized_vert_ = false;
  wm_state_modal_ = false;

  std::vector<int> state_atoms;
  if (!wm_->xconn()->GetIntArrayProperty(
          xid_, wm_->GetXAtom(ATOM_NET_WM_STATE), &state_atoms)) {
    return;
  }

  XAtom fullscreen_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN);
  XAtom max_horz_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ);
  XAtom max_vert_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT);
  XAtom modal_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL);
  for (std::vector<int>::const_iterator it = state_atoms.begin();
       it != state_atoms.end(); ++it) {
    XAtom atom = static_cast<XAtom>(*it);
    if (atom == fullscreen_atom)
      wm_state_fullscreen_ = true;
    if (atom == max_horz_atom)
      wm_state_maximized_horz_ = true;
    if (atom == max_vert_atom)
      wm_state_maximized_vert_ = true;
    else if (atom == modal_atom)
      wm_state_modal_ = true;
  }

  VLOG(1) << "Fetched NET_WM_STATE_ for " << xid_ << ":"
          << " fullscreen=" << wm_state_fullscreen_
          << " maximized_horz=" << wm_state_maximized_horz_
          << " maximized_vert=" << wm_state_maximized_vert_
          << " modal=" << wm_state_modal_;
}

void Window::FetchAndApplyShape(bool update_shadow) {
  shaped_ = false;
  ByteMap bytemap(client_width_, client_height_);

  // We don't grab the server around these two requests, so it's possible
  // that a shaped window will have become unshaped between them and we'll
  // think that the window is shaped but get back an unshaped region.  This
  // should be okay; we should get another ShapeNotify event for the window
  // becoming unshaped and clear the useless mask then.
  if (wm_->xconn()->IsWindowShaped(xid_) &&
      wm_->xconn()->GetWindowBoundingRegion(xid_, &bytemap)) {
    shaped_ = true;
  }

  if (!shaped_) {
    actor_->ClearAlphaMask();
  } else {
    VLOG(1) << "Got shape for " << xid_;
    actor_->SetAlphaMask(bytemap.bytes(), bytemap.width(), bytemap.height());
  }
  if (update_shadow)
    UpdateShadowIfNecessary();
}

bool Window::FetchMapState() {
  XWindowAttributes attr;
  if (!wm_->xconn()->GetWindowAttributes(xid_, &attr))
    return false;
  return (attr.map_state != IsUnmapped);
}

bool Window::HandleWmStateMessage(const XClientMessageEvent& event) {
  XAtom wm_state_atom = wm_->GetXAtom(ATOM_NET_WM_STATE);
  if (event.message_type != wm_state_atom ||
      event.format != XConnection::kLongFormat) {
    return false;
  }

  XAtom fullscreen_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN);
  if (static_cast<XAtom>(event.data.l[1]) == fullscreen_atom ||
      static_cast<XAtom>(event.data.l[2]) == fullscreen_atom) {
    SetWmStateInternal(event.data.l[0], &wm_state_fullscreen_);
  }

  XAtom modal_atom = wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL);
  if (static_cast<XAtom>(event.data.l[1]) == modal_atom ||
      static_cast<XAtom>(event.data.l[2]) == modal_atom) {
    SetWmStateInternal(event.data.l[0], &wm_state_modal_);
  }

  // We don't let clients toggle their maximized state currently.

  return UpdateWmStateProperty();
}

bool Window::ChangeWmState(const std::vector<std::pair<XAtom, bool> >& states) {
  for (std::vector<std::pair<XAtom, bool> >::const_iterator it = states.begin();
       it != states.end(); ++it) {
    XAtom xatom = it->first;
    int action = it->second;  // 0 is remove, 1 is add

    if (xatom == wm_->GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN))
      SetWmStateInternal(action, &wm_state_fullscreen_);
    else if (xatom == wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ))
      SetWmStateInternal(action, &wm_state_maximized_horz_);
    else if (xatom == wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT))
      SetWmStateInternal(action, &wm_state_maximized_vert_);
    else if (xatom == wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL))
      SetWmStateInternal(action, &wm_state_modal_);
    else
      LOG(ERROR) << "Unsupported _NET_WM_STATE " << xatom << " for " << xid_;
  }
  return UpdateWmStateProperty();
}

bool Window::TakeFocus(Time timestamp) {
  VLOG(2) << "Focusing " << xid_ << " using time " << timestamp;
  if (supports_wm_take_focus_) {
    XEvent event;
    XClientMessageEvent* client_event = &(event.xclient);
    client_event->type = ClientMessage;
    client_event->window = xid_;
    client_event->message_type = wm_->GetXAtom(ATOM_WM_PROTOCOLS);
    client_event->format = XConnection::kLongFormat;
    client_event->data.l[0] = wm_->GetXAtom(ATOM_WM_TAKE_FOCUS);
    client_event->data.l[1] = timestamp;
    if (!wm_->xconn()->SendEvent(xid_, &event, 0))
      return false;
  } else {
    if (!wm_->xconn()->FocusWindow(xid_, timestamp))
      return false;
  }
  focused_ = true;
  return true;
}

bool Window::SendDeleteRequest(Time timestamp) {
  VLOG(2) << "Maybe asking " << xid_ << " to delete itself with time "
          << timestamp;
  if (!supports_wm_delete_window_)
    return false;

  XEvent event;
  XClientMessageEvent* client_event = &(event.xclient);
  client_event->type = ClientMessage;
  client_event->window = xid_;
  client_event->message_type = wm_->GetXAtom(ATOM_WM_PROTOCOLS);
  client_event->format = XConnection::kLongFormat;
  client_event->data.l[0] = wm_->GetXAtom(ATOM_WM_DELETE_WINDOW);
  client_event->data.l[1] = timestamp;
  return wm_->xconn()->SendEvent(xid_, &event, 0);
}

bool Window::AddPassiveButtonGrab() {
  VLOG(2) << "Adding passive button grab for " << xid_;
  return wm_->xconn()->AddPassiveButtonGrabOnWindow(
      xid_, AnyButton, ButtonPressMask);
}

bool Window::RemovePassiveButtonGrab() {
  VLOG(2) << "Removing passive button grab for " << xid_;
  return wm_->xconn()->RemovePassiveButtonGrabOnWindow(xid_, AnyButton);
}

void Window::GetMaxSize(int desired_width, int desired_height,
                        int* width_out, int* height_out) const {
  CHECK_GT(desired_width, 0);
  CHECK_GT(desired_height, 0);

  if (max_width_hint_ > 0)
    desired_width = std::min(max_width_hint_, desired_width);
  if (min_width_hint_ > 0)
    desired_width = std::max(min_width_hint_, desired_width);

  if (width_inc_hint_ > 0) {
    int base_width =
        (base_width_hint_ > 0) ? base_width_hint_ :
        (min_width_hint_ > 0) ? min_width_hint_ :
        0;
    *width_out = base_width +
        ((desired_width - base_width) / width_inc_hint_) * width_inc_hint_;
  } else {
    *width_out = desired_width;
  }

  if (max_height_hint_ > 0)
    desired_height = std::min(max_height_hint_, desired_height);
  if (min_height_hint_ > 0)
    desired_height = std::max(min_height_hint_, desired_height);

  if (height_inc_hint_ > 0) {
    int base_height =
        (base_height_hint_ > 0) ? base_height_hint_ :
        (min_height_hint_ > 0) ? min_height_hint_ :
        0;
    *height_out = base_height +
        ((desired_height - base_height) / height_inc_hint_) * height_inc_hint_;
  } else {
    *height_out = desired_height;
  }
  VLOG(2) << "Max size for " << xid_ << " is "
          << *width_out << "x" << *height_out
          << " (desired was " << desired_width << "x" << desired_height << ")";
}

bool Window::MapClient() {
  VLOG(2) << "Mapping " << xid_;
  if (!wm_->xconn()->MapWindow(xid_))
    return false;
  return true;
}

bool Window::UnmapClient() {
  VLOG(2) << "Unmapping " << xid_;
  if (!wm_->xconn()->UnmapWindow(xid_))
    return false;
  return true;
}

void Window::SaveClientAndCompositedSize(int width, int height) {
  VLOG(2) << "Setting " << xid_ << "'s client and composited size to "
          << width << "x" << height;
  client_width_ = width;
  client_height_ = height;
  actor_->SetSize(client_width_, client_height_);
  if (shadow_.get()) {
    shadow_->Resize(composited_scale_x_ * client_width_,
                    composited_scale_y_ * client_height_,
                    0);  // anim_ms
  }
}

bool Window::MoveClient(int x, int y) {
  VLOG(2) << "Moving " << xid_ << "'s client window to ("
          << x << ", " << y << ")";
  if (!wm_->xconn()->MoveWindow(xid_, x, y))
    return false;
  SaveClientPosition(x, y);
  return true;
}

bool Window::MoveClientOffscreen() {
  return MoveClient(wm_->width(), wm_->height());
}

bool Window::MoveClientToComposited() {
  return MoveClient(composited_x_, composited_y_);
}

bool Window::CenterClientOverWindow(Window* win) {
  CHECK(win);
  int center_x = win->client_x() + 0.5 * win->client_width();
  int center_y = win->client_y() + 0.5 * win->client_height();
  return MoveClient(center_x - 0.5 * client_width_,
                    center_y - 0.5 * client_height_);
}

bool Window::ResizeClient(int width, int height, Gravity gravity) {
  int dx = (gravity == GRAVITY_NORTHEAST || gravity == GRAVITY_SOUTHEAST) ?
      width - client_width_ : 0;
  int dy = (gravity == GRAVITY_SOUTHWEST || gravity == GRAVITY_SOUTHEAST) ?
      height - client_height_ : 0;

  VLOG(2) << "Resizing " << xid_ << "'s client window to "
          << width << "x" << height;
  if (dx || dy) {
    // If we need to move the window as well due to gravity, do it all in
    // one ConfigureWindow request to the server.
    if (!wm_->xconn()->ConfigureWindow(
            xid_, client_x_ - dx, client_y_ - dy, width, height)) {
      return false;
    }
    SaveClientPosition(client_x_ - dx, client_y_ - dy);
    // TODO: Test whether this works for scaled windows.
    MoveComposited(composited_x_ - composited_scale_x_ * dx,
                   composited_y_ - composited_scale_y_ * dy,
                   0);
  } else  {
    if (!wm_->xconn()->ResizeWindow(xid_, width, height))
      return false;
  }

  SaveClientAndCompositedSize(width, height);
  return true;
}

bool Window::RaiseClient() {
  bool result = wm_->xconn()->RaiseWindow(xid_);
  return result;
}

bool Window::StackClientAbove(XWindow sibling_xid) {
  CHECK(sibling_xid != None);
  bool result = wm_->xconn()->StackWindow(xid_, sibling_xid, true);
  return result;
}

bool Window::StackClientBelow(XWindow sibling_xid) {
  CHECK(sibling_xid != None);
  bool result = wm_->xconn()->StackWindow(xid_, sibling_xid, false);
  return result;
}

void Window::MoveComposited(int x, int y, int anim_ms) {
  VLOG(2) << "Moving " << xid_ << "'s composited window to ("
          << x << ", " << y << ") over " << anim_ms << " ms";
  composited_x_ = x;
  composited_y_ = y;
  actor_->Move(x, y, anim_ms);
  if (shadow_.get())
    shadow_->Move(x, y, anim_ms);
}

void Window::MoveCompositedX(int x, int anim_ms) {
  VLOG(2) << "Setting " << xid_ << "'s composited window's X position to "
          << x << " over " << anim_ms << " ms";
  composited_x_ = x;
  actor_->MoveX(x, anim_ms);
  if (shadow_.get())
    shadow_->MoveX(x, anim_ms);
}

void Window::MoveCompositedY(int y, int anim_ms) {
  VLOG(2) << "Setting " << xid_ << "'s composited window's Y position to "
          << y << " over " << anim_ms << " ms";
  composited_y_ = y;
  actor_->MoveY(y, anim_ms);
  if (shadow_.get())
    shadow_->MoveY(y, anim_ms);
}

void Window::ShowComposited() {
  VLOG(2) << "Showing " << xid_ << "'s composited window";
  actor_->SetVisibility(true);
  composited_shown_ = true;
  if (shadow_.get())
    shadow_->Show();
}

void Window::HideComposited() {
  VLOG(2) << "Hiding " << xid_ << "'s composited window";
  actor_->SetVisibility(false);
  composited_shown_ = false;
  if (shadow_.get())
    shadow_->Hide();
}

void Window::SetCompositedOpacity(double opacity, int anim_ms) {
  composited_opacity_ = opacity;

  // The client might've already requested that the window be translucent.
  double combined_opacity = composited_opacity_ * client_opacity_;

  // Reset the shadow's opacity as well.
  shadow_opacity_ = combined_opacity;

  VLOG(2) << "Setting " << xid_ << "'s composited window opacity to "
          << opacity << " (combined is " << combined_opacity << ") over "
          << anim_ms << " ms";

  actor_->SetOpacity(combined_opacity, anim_ms);
  if (shadow_.get())
    shadow_->SetOpacity(shadow_opacity_, anim_ms);
}

void Window::ScaleComposited(double scale_x, double scale_y, int anim_ms) {
  VLOG(2) << "Scaling " << xid_ << "'s composited window by ("
          << scale_x << ", " << scale_y << ") over " << anim_ms << " ms";
  composited_scale_x_ = scale_x;
  composited_scale_y_ = scale_y;

  actor_->Scale(scale_x, scale_y, anim_ms);
  if (shadow_.get())
    shadow_->Resize(scale_x * client_width_, scale_y * client_height_, anim_ms);
}

void Window::SetShadowOpacity(double opacity, int anim_ms) {
  VLOG(2) << "Setting " << xid_ << "'s shadow opacity to " << opacity
          << " over " << anim_ms << " ms";
  shadow_opacity_ = opacity;
  if (shadow_.get())
    shadow_->SetOpacity(opacity, anim_ms);
}

void Window::StackCompositedAbove(ClutterInterface::Actor* actor,
                                  ClutterInterface::Actor* shadow_actor) {
  if (actor)
    actor_->Raise(actor);
  if (shadow_.get())
    shadow_->group()->Lower(shadow_actor ? shadow_actor : actor_.get());
}

void Window::StackCompositedBelow(ClutterInterface::Actor* actor,
                                  ClutterInterface::Actor* shadow_actor) {
  if (actor)
    actor_->Lower(actor);
  if (shadow_.get())
    shadow_->group()->Lower(shadow_actor ? shadow_actor : actor_.get());
}

void Window::UpdateShadowIfNecessary() {
  bool should_have_shadow =
      FLAGS_window_drop_shadows &&
      !override_redirect_ &&
      type_ != WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY &&
      type_ != WmIpc::WINDOW_TYPE_CHROME_FLOATING_TAB &&
      type_ != WmIpc::WINDOW_TYPE_CREATE_BROWSER_WINDOW &&
      !shaped_;

  if (!should_have_shadow && shadow_.get()) {
    shadow_.reset(NULL);
  } else if (should_have_shadow && !shadow_.get()) {
    shadow_.reset(new Shadow(wm_->clutter()));

    // Stack it below all of the other windows.
    wm_->stage()->AddActor(shadow_->group());
    shadow_->group()->Lower(wm_->overview_window_depth());

    shadow_->Move(composited_x_, composited_y_, 0);
    shadow_->SetOpacity(shadow_opacity_, 0);
    shadow_->Resize(composited_scale_x_ * client_width_,
                    composited_scale_y_ * client_height_, 0);
    if (composited_shown_)
      shadow_->Show();
  }
}

void Window::SetWmStateInternal(int action, bool* value) {
  switch (action) {
    case 0:  // _NET_WM_STATE_REMOVE
      *value = false;
      break;
    case 1:  // _NET_WM_STATE_ADD
      *value = true;
      break;
    case 2:  // _NET_WM_STATE_TOGGLE
      *value = !(*value);
      break;
    default:
      LOG(WARNING) << "Got _NET_WM_STATE message for " << xid_
                   << " with invalid action " << action;
  }
}

bool Window::UpdateWmStateProperty() {
  XAtom wm_state_atom = wm_->GetXAtom(ATOM_NET_WM_STATE);

  std::vector<int> values;
  if (wm_state_fullscreen_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN));
  if (wm_state_maximized_horz_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ));
  if (wm_state_maximized_vert_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT));
  if (wm_state_modal_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL));

  VLOG(1) << "Updating _NET_WM_STATE for " << xid_ << ":"
          << " fullscreen=" << wm_state_fullscreen_
          << " maximized_horz=" << wm_state_maximized_horz_
          << " maximized_vert=" << wm_state_maximized_vert_
          << " modal=" << wm_state_modal_;
  if (!values.empty()) {
    return wm_->xconn()->SetIntArrayProperty(
        xid_, wm_state_atom, XA_ATOM, values);
  } else {
    return wm_->xconn()->DeletePropertyIfExists(xid_, wm_state_atom);
  }
}

}  // namespace chromeos
