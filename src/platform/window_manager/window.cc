// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/window.h"

#include <algorithm>

#include <gflags/gflags.h>

#include "base/logging.h"
#include "base/string_util.h"
#include "chromeos/obsolete_logging.h"

#include "window_manager/atom_cache.h"
#include "window_manager/shadow.h"
#include "window_manager/util.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_bool(window_drop_shadows, true, "Display drop shadows under windows");

DECLARE_bool(wm_use_compositing);  // from window_manager.cc

namespace window_manager {

Window::Window(WindowManager* wm, XWindow xid, bool override_redirect)
    : xid_(xid),
      xid_str_(XidStr(xid_)),
      wm_(wm),
      actor_(wm_->clutter()->CreateTexturePixmap()),
      shadow_(FLAGS_window_drop_shadows ? new Shadow(wm->clutter()) : NULL),
      transient_for_xid_(None),
      override_redirect_(override_redirect),
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
      using_shadow_(false),
      shadow_opacity_(1.0),
      supports_wm_take_focus_(false),
      supports_wm_delete_window_(false),
      wm_state_fullscreen_(false),
      wm_state_maximized_horz_(false),
      wm_state_maximized_vert_(false),
      wm_state_modal_(false) {
  // Listen for focus, property, and shape changes on this window.
  wm_->xconn()->SelectInputOnWindow(
      xid_, FocusChangeMask | PropertyChangeMask, true);
  wm_->xconn()->SelectShapeEventsOnWindow(xid_);

  // We update 'mapped_' when we get the MapNotify event instead of doing
  // it here; things get tricky otherwise since there's a race as to
  // whether override-redirect windows are mapped or not at this point.

  XConnection::WindowGeometry geometry;
  if (wm_->xconn()->GetWindowGeometry(xid_, &geometry)) {
    client_x_ = composited_x_ = geometry.x;
    client_y_ = composited_y_ = geometry.y;
    client_width_ = geometry.width;
    client_height_ = geometry.height;

    // If the window has a border, remove it -- they make things more confusing
    // (we need to include the border when telling Clutter the window's size,
    // but it's not included when telling X to resize the window, etc.).
    if (geometry.border_width > 0)
      wm_->xconn()->SetWindowBorderWidth(xid_, 0);
  }

  // We don't need to redirect the window for compositing; Clutter already
  // does it for us.
  VLOG(1) << "Constructing object to track "
          << (override_redirect_ ? "override-redirect " : "")
          << "window " << xid_str() << " "
          << "at (" << client_x_ << ", " << client_y_ << ") "
          << "with dimensions " << client_width_ << "x" << client_height_;

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
  actor_->SetName(std::string("window ") + xid_str());
  wm_->stage()->AddActor(actor_.get());

  if (shadow_.get()) {
    shadow_->group()->SetName(
        std::string("shadow group for window " + xid_str()));
    wm_->stage()->AddActor(shadow_->group());
    shadow_->Move(composited_x_, composited_y_, 0);
    shadow_->SetOpacity(shadow_opacity_, 0);
    shadow_->Resize(composited_scale_x_ * client_width_,
                    composited_scale_y_ * client_height_, 0);
  }

  // Properties could've been set on this window after it was created but
  // before we selected on PropertyChangeMask, so we need to query them
  // here.  Don't create a shadow yet; we still need to check if it's
  // shaped.
  FetchAndApplyWindowType(false);

  // Check if the window is shaped.
  FetchAndApplyShape(true);

  // Check if the client window has set _NET_WM_WINDOW_OPACITY.
  FetchAndApplyWindowOpacity();

  // Apply the size hints, which may resize the actor.
  FetchAndApplySizeHints();

  // Load other properties that might've gotten set before we started
  // listening for property changes on the window.
  FetchAndApplyWmProtocols();
  FetchAndApplyWmState();
  FetchAndApplyChromeState();
  FetchAndApplyTransientHint();
}

Window::~Window() {
}

bool Window::FetchAndApplySizeHints() {
  if (!wm_->xconn()->GetSizeHintsForWindow(xid_, &size_hints_))
    return false;

  // If windows are override-redirect or have already been mapped, they
  // should just make/request any desired changes directly.  Also ignore
  // position, aspect ratio, etc. hints for now.
  if (!mapped_ && !override_redirect_ &&
      (size_hints_.width > 0 && size_hints_.height > 0)) {
    VLOG(1) << "Got size hints for " << xid_str() << ": "
            << size_hints_.width << "x" << size_hints_.height;
    ResizeClient(size_hints_.width, size_hints_.height, GRAVITY_NORTHWEST);
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
  VLOG(1) << "Window " << xid_str() << " has type " << type_;
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
      VLOG(2) << "Window " << xid_str() << " supports WM_TAKE_FOCUS";
      supports_wm_take_focus_ = true;
    } else if (static_cast<XAtom>(*it) == wm_delete_window) {
      VLOG(2) << "Window " << xid_str() << " supports WM_DELETE_WINDOW";
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

  VLOG(1) << "Fetched _NET_WM_STATE for " << xid_str() << ":"
          << " fullscreen=" << wm_state_fullscreen_
          << " maximized_horz=" << wm_state_maximized_horz_
          << " maximized_vert=" << wm_state_maximized_vert_
          << " modal=" << wm_state_modal_;
}

void Window::FetchAndApplyChromeState() {
  XAtom state_xatom = wm_->GetXAtom(ATOM_CHROME_STATE);
  chrome_state_xatoms_.clear();
  std::vector<int> state_xatoms;
  if (!wm_->xconn()->GetIntArrayProperty(xid_, state_xatom, &state_xatoms))
    return;

  std::string debug_str;
  for (std::vector<int>::const_iterator it = state_xatoms.begin();
       it != state_xatoms.end(); ++it) {
    chrome_state_xatoms_.insert(static_cast<XAtom>(*it));
    if (!debug_str.empty())
      debug_str += " ";
    debug_str += wm_->GetXAtomName(static_cast<XAtom>(*it));
  }
  VLOG(1) << "Fetched " << wm_->GetXAtomName(state_xatom) << " for "
          << xid_str() << ": " << debug_str;
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
    VLOG(1) << "Got shape for " << xid_str();
    actor_->SetAlphaMask(bytemap.bytes(), bytemap.width(), bytemap.height());
  }
  if (update_shadow)
    UpdateShadowIfNecessary();
}

bool Window::FetchMapState() {
  XConnection::WindowAttributes attr;
  if (!wm_->xconn()->GetWindowAttributes(xid_, &attr))
    return false;
  return (attr.map_state != XConnection::WindowAttributes::MAP_STATE_UNMAPPED);
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
      LOG(ERROR) << "Unsupported _NET_WM_STATE " << xatom
                 << " for " << xid_str();
  }
  return UpdateWmStateProperty();
}

bool Window::ChangeChromeState(
    const std::vector<std::pair<XAtom, bool> >& states) {
  for (std::vector<std::pair<XAtom, bool> >::const_iterator it = states.begin();
       it != states.end(); ++it) {
    if (it->second)
      chrome_state_xatoms_.insert(it->first);
    else
      chrome_state_xatoms_.erase(it->first);
  }
  return UpdateChromeStateProperty();
}


bool Window::TakeFocus(Time timestamp) {
  VLOG(2) << "Focusing " << xid_str() << " using time " << timestamp;
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
  VLOG(2) << "Maybe asking " << xid_str() << " to delete itself with time "
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

bool Window::AddButtonGrab() {
  VLOG(2) << "Adding button grab for " << xid_str();
  return wm_->xconn()->AddButtonGrabOnWindow(
      xid_, AnyButton, ButtonPressMask, true);  // synchronous=true
}

bool Window::RemoveButtonGrab() {
  VLOG(2) << "Removing button grab for " << xid_str();
  return wm_->xconn()->RemoveButtonGrabOnWindow(xid_, AnyButton);
}

void Window::GetMaxSize(int desired_width, int desired_height,
                        int* width_out, int* height_out) const {
  CHECK_GT(desired_width, 0);
  CHECK_GT(desired_height, 0);

  if (size_hints_.max_width > 0)
    desired_width = std::min(size_hints_.max_width, desired_width);
  if (size_hints_.min_width > 0)
    desired_width = std::max(size_hints_.min_width, desired_width);

  if (size_hints_.width_increment > 0) {
    int base_width =
        (size_hints_.base_width > 0) ? size_hints_.base_width :
        (size_hints_.min_width > 0) ? size_hints_.min_width :
        0;
    *width_out = base_width +
        ((desired_width - base_width) / size_hints_.width_increment) *
        size_hints_.width_increment;
  } else {
    *width_out = desired_width;
  }

  if (size_hints_.max_height > 0)
    desired_height = std::min(size_hints_.max_height, desired_height);
  if (size_hints_.min_height > 0)
    desired_height = std::max(size_hints_.min_height, desired_height);

  if (size_hints_.height_increment > 0) {
    int base_height =
        (size_hints_.base_height > 0) ? size_hints_.base_height :
        (size_hints_.min_height > 0) ? size_hints_.min_height :
        0;
    *height_out = base_height +
        ((desired_height - base_height) / size_hints_.height_increment) *
        size_hints_.height_increment;
  } else {
    *height_out = desired_height;
  }

  VLOG(2) << "Max size for " << xid_str() << " is "
          << *width_out << "x" << *height_out
          << " (desired was " << desired_width << "x" << desired_height << ")";
}

bool Window::MapClient() {
  VLOG(2) << "Mapping " << xid_str();
  if (!wm_->xconn()->MapWindow(xid_))
    return false;
  return true;
}

bool Window::UnmapClient() {
  VLOG(2) << "Unmapping " << xid_str();
  if (!wm_->xconn()->UnmapWindow(xid_))
    return false;
  return true;
}

void Window::SaveClientAndCompositedSize(int width, int height) {
  VLOG(2) << "Setting " << xid_str() << "'s client and composited size to "
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
  VLOG(2) << "Moving " << xid_str() << "'s client window to ("
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

  VLOG(2) << "Resizing " << xid_str() << "'s client window to "
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
  VLOG(2) << "Moving " << xid_str() << "'s composited window to ("
          << x << ", " << y << ") over " << anim_ms << " ms";
  composited_x_ = x;
  composited_y_ = y;
  actor_->Move(x, y, anim_ms);
  if (shadow_.get())
    shadow_->Move(x, y, anim_ms);
}

void Window::MoveCompositedX(int x, int anim_ms) {
  VLOG(2) << "Setting " << xid_str() << "'s composited window's X position to "
          << x << " over " << anim_ms << " ms";
  composited_x_ = x;
  actor_->MoveX(x, anim_ms);
  if (shadow_.get())
    shadow_->MoveX(x, anim_ms);
}

void Window::MoveCompositedY(int y, int anim_ms) {
  VLOG(2) << "Setting " << xid_str() << "'s composited window's Y position to "
          << y << " over " << anim_ms << " ms";
  composited_y_ = y;
  actor_->MoveY(y, anim_ms);
  if (shadow_.get())
    shadow_->MoveY(y, anim_ms);
}

void Window::ShowComposited() {
  VLOG(2) << "Showing " << xid_str() << "'s composited window";
  actor_->SetVisibility(true);
  composited_shown_ = true;
  if (shadow_.get() && using_shadow_)
    shadow_->Show();
}

void Window::HideComposited() {
  VLOG(2) << "Hiding " << xid_str() << "'s composited window";
  actor_->SetVisibility(false);
  composited_shown_ = false;
  if (shadow_.get() && using_shadow_)
    shadow_->Hide();
}

void Window::SetCompositedOpacity(double opacity, int anim_ms) {
  composited_opacity_ = opacity;

  // The client might've already requested that the window be translucent.
  double combined_opacity = composited_opacity_ * client_opacity_;

  // Reset the shadow's opacity as well.
  shadow_opacity_ = combined_opacity;

  VLOG(2) << "Setting " << xid_str() << "'s composited window opacity to "
          << opacity << " (combined is " << combined_opacity << ") over "
          << anim_ms << " ms";

  actor_->SetOpacity(combined_opacity, anim_ms);
  if (shadow_.get())
    shadow_->SetOpacity(shadow_opacity_, anim_ms);
}

void Window::ScaleComposited(double scale_x, double scale_y, int anim_ms) {
  VLOG(2) << "Scaling " << xid_str() << "'s composited window by ("
          << scale_x << ", " << scale_y << ") over " << anim_ms << " ms";
  composited_scale_x_ = scale_x;
  composited_scale_y_ = scale_y;

  actor_->Scale(scale_x, scale_y, anim_ms);
  if (shadow_.get())
    shadow_->Resize(scale_x * client_width_, scale_y * client_height_, anim_ms);
}

void Window::SetShadowOpacity(double opacity, int anim_ms) {
  VLOG(2) << "Setting " << xid_str() << "'s shadow opacity to " << opacity
          << " over " << anim_ms << " ms";
  shadow_opacity_ = opacity;
  if (shadow_.get())
    shadow_->SetOpacity(opacity, anim_ms);
}

void Window::StackCompositedAbove(ClutterInterface::Actor* actor,
                                  ClutterInterface::Actor* shadow_actor,
                                  bool stack_above_shadow_actor) {
  if (actor)
    actor_->Raise(actor);
  if (shadow_.get()) {
    if (!shadow_actor || !stack_above_shadow_actor) {
      shadow_->group()->Lower(shadow_actor ? shadow_actor : actor_.get());
    } else {
      shadow_->group()->Raise(shadow_actor);
    }
  }
}

void Window::StackCompositedBelow(ClutterInterface::Actor* actor,
                                  ClutterInterface::Actor* shadow_actor,
                                  bool stack_above_shadow_actor) {
  if (actor)
    actor_->Lower(actor);
  if (shadow_.get()) {
    if (!shadow_actor || !stack_above_shadow_actor) {
      shadow_->group()->Lower(shadow_actor ? shadow_actor : actor_.get());
    } else {
      shadow_->group()->Raise(shadow_actor);
    }
  }
}

ClutterInterface::Actor* Window::GetBottomActor() {
  return (shadow_.get() ? shadow_->group() : actor_.get());
}

void Window::UpdateShadowIfNecessary() {
  if (!shadow_.get())
    return;

  bool should_use_shadow =
      !override_redirect_ &&
      type_ != WmIpc::WINDOW_TYPE_CHROME_FLOATING_TAB &&
      type_ != WmIpc::WINDOW_TYPE_CHROME_INFO_BUBBLE &&
      type_ != WmIpc::WINDOW_TYPE_CHROME_TAB_SUMMARY &&
      type_ != WmIpc::WINDOW_TYPE_CREATE_BROWSER_WINDOW &&
      !shaped_;

  if (!should_use_shadow && using_shadow_) {
    shadow_->Hide();
    using_shadow_ = false;
  } else if (should_use_shadow && !using_shadow_) {
    if (composited_shown_)
      shadow_->Show();
    using_shadow_ = true;
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
      LOG(WARNING) << "Got _NET_WM_STATE message for " << xid_str()
                   << " with invalid action " << action;
  }
}

bool Window::UpdateWmStateProperty() {
  std::vector<int> values;
  if (wm_state_fullscreen_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_FULLSCREEN));
  if (wm_state_maximized_horz_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_HORZ));
  if (wm_state_maximized_vert_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_MAXIMIZED_VERT));
  if (wm_state_modal_)
    values.push_back(wm_->GetXAtom(ATOM_NET_WM_STATE_MODAL));

  VLOG(1) << "Updating _NET_WM_STATE for " << xid_str() << ":"
          << " fullscreen=" << wm_state_fullscreen_
          << " maximized_horz=" << wm_state_maximized_horz_
          << " maximized_vert=" << wm_state_maximized_vert_
          << " modal=" << wm_state_modal_;
  XAtom wm_state_atom = wm_->GetXAtom(ATOM_NET_WM_STATE);
  if (!values.empty()) {
    return wm_->xconn()->SetIntArrayProperty(
        xid_, wm_state_atom, XA_ATOM, values);
  } else {
    return wm_->xconn()->DeletePropertyIfExists(xid_, wm_state_atom);
  }
}

bool Window::UpdateChromeStateProperty() {
  std::vector<int> values;
  for (std::set<XAtom>::const_iterator it = chrome_state_xatoms_.begin();
       it != chrome_state_xatoms_.end(); ++it) {
    values.push_back(static_cast<int>(*it));
  }

  XAtom state_xatom = wm_->GetXAtom(ATOM_CHROME_STATE);
  if (!values.empty()) {
    return wm_->xconn()->SetIntArrayProperty(
        xid_, state_xatom, XA_ATOM, values);
  } else {
    return wm_->xconn()->DeletePropertyIfExists(xid_, state_xatom);
  }
}

}  // namespace window_manager
