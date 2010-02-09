// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/panel.h"

extern "C" {
#include <X11/cursorfont.h>
}

#include <gflags/gflags.h>
#include "chromeos/obsolete_logging.h"

#include "window_manager/atom_cache.h"
#include "window_manager/panel_manager.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"
#include "window_manager/x_connection.h"

DEFINE_int32(panel_max_width, -1,
             "Maximum width for panels (0 or less means unconstrained)");
DEFINE_int32(panel_max_height, -1,
             "Maximum height for panels (0 or less means unconstrained)");
DEFINE_bool(panel_opaque_resize, false, "Resize panels opaquely");

namespace window_manager {

using chromeos::NewPermanentCallback;
using std::make_pair;
using std::max;
using std::min;
using std::pair;
using std::vector;

// Amount of time to take to fade in the actor used for non-opaque resizes.
static const int kResizeActorOpacityAnimMs = 150;

// Minimum dimensions to which a panel content window can be resized.
static const int kPanelMinWidth = 20;
static const int kPanelMinHeight = 20;

// Frequency with which we should update the size of panels as they're
// being resized.
static const int kResizeUpdateMs = 25;

// Appearance of the box used for non-opaque resizing.

// Equivalent to "#4181f5" X color.
static const ClutterInterface::Color kResizeBoxBgColor(0.254902,
                                                       0.505882,
                                                       0.960784);

// Equivalent to "#234583" X color.
static const ClutterInterface::Color kResizeBoxBorderColor(0.137255,
                                                           0.270588,
                                                           0.513725);
static const double kResizeBoxOpacity = 0.3;

const int Panel::kResizeBorderWidth = 5;
const int Panel::kResizeCornerSize = 25;

Panel::Panel(PanelManager* panel_manager,
             Window* content_win,
             Window* titlebar_win)
    : panel_manager_(panel_manager),
      content_win_(content_win),
      titlebar_win_(titlebar_win),
      resize_actor_(NULL),
      resize_event_coalescer_(
          NewPermanentCallback(this, &Panel::ApplyResize),
          kResizeUpdateMs),
      // We don't need to select events on any of the drag borders; we'll
      // just install button grabs later.
      top_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1, 0)),
      top_left_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1, 0)),
      top_right_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1, 0)),
      left_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1, 0)),
      right_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1, 0)),
      resizable_(false),
      composited_windows_set_up_(false),
      drag_xid_(0),
      drag_start_x_(0),
      drag_start_y_(0),
      drag_orig_width_(1),
      drag_orig_height_(1),
      drag_last_width_(1),
      drag_last_height_(1) {
  CHECK(content_win_);
  CHECK(titlebar_win_);

  wm()->xconn()->SelectInputOnWindow(titlebar_win_->xid(),
                                     EnterWindowMask,
                                     true);  // preserve_existing

  // Install passive button grabs on all the resize handles, using
  // asynchronous mode so that we'll continue to receive mouse events while
  // the pointer grab is in effect.  (Note that these button grabs are
  // necessary to avoid a race condition: if we explicitly request an
  // active grab when seeing a button press, the button might already be
  // released by the time that the grab is installed.)
  int event_mask = ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
  wm()->xconn()->AddButtonGrabOnWindow(top_input_xid_, 1, event_mask, false);
  wm()->xconn()->AddButtonGrabOnWindow(
      top_left_input_xid_, 1, event_mask, false);
  wm()->xconn()->AddButtonGrabOnWindow(
      top_right_input_xid_, 1, event_mask, false);
  wm()->xconn()->AddButtonGrabOnWindow(left_input_xid_, 1, event_mask, false);
  wm()->xconn()->AddButtonGrabOnWindow(right_input_xid_, 1, event_mask, false);

  // Constrain the size of the content if we've been requested to do so.
  int capped_width = (FLAGS_panel_max_width > 0) ?
      min(content_win_->client_width(), FLAGS_panel_max_width) :
      content_win_->client_width();
  int capped_height = (FLAGS_panel_max_height > 0) ?
      min(content_win_->client_height(), FLAGS_panel_max_height) :
      content_win_->client_height();
  if (capped_width != content_win_->client_width() ||
      capped_height != content_win_->client_height()) {
    content_win_->ResizeClient(
        capped_width, capped_height, Window::GRAVITY_NORTHWEST);
  }

  wm()->xconn()->SetWindowCursor(top_input_xid_, XC_top_side);
  wm()->xconn()->SetWindowCursor(top_left_input_xid_, XC_top_left_corner);
  wm()->xconn()->SetWindowCursor(top_right_input_xid_, XC_top_right_corner);
  wm()->xconn()->SetWindowCursor(left_input_xid_, XC_left_side);
  wm()->xconn()->SetWindowCursor(right_input_xid_, XC_right_side);
}

Panel::~Panel() {
  if (drag_xid_) {
    wm()->xconn()->RemovePointerGrab(false, CurrentTime);
    drag_xid_ = None;
  }
  wm()->xconn()->DeselectInputOnWindow(titlebar_win_->xid(), EnterWindowMask);
  wm()->xconn()->DestroyWindow(top_input_xid_);
  wm()->xconn()->DestroyWindow(top_left_input_xid_);
  wm()->xconn()->DestroyWindow(top_right_input_xid_);
  wm()->xconn()->DestroyWindow(left_input_xid_);
  wm()->xconn()->DestroyWindow(right_input_xid_);
  content_win_->RemoveButtonGrab();
  panel_manager_ = NULL;
  content_win_ = NULL;
  titlebar_win_ = NULL;
  top_input_xid_ = None;
  top_left_input_xid_ = None;
  top_right_input_xid_ = None;
  left_input_xid_ = None;
  right_input_xid_ = None;
}

void Panel::GetInputWindows(vector<XWindow>* windows_out) {
  CHECK(windows_out);
  windows_out->clear();
  windows_out->reserve(5);
  windows_out->push_back(top_input_xid_);
  windows_out->push_back(top_left_input_xid_);
  windows_out->push_back(top_right_input_xid_);
  windows_out->push_back(left_input_xid_);
  windows_out->push_back(right_input_xid_);
}

void Panel::HandleInputWindowButtonPress(
    XWindow xid, int x, int y, int button, Time timestamp) {
  if (button != 1)
    return;
  DCHECK(drag_xid_ == None)
      << "Panel " << xid_str() << " got button press in " << xid
      << " but already has drag XID " << XidStr(drag_xid_);

  drag_xid_ = xid;
  drag_start_x_ = x;
  drag_start_y_ = y;
  drag_orig_width_ = drag_last_width_ = content_width();
  drag_orig_height_ = drag_last_height_ = content_height();
  resize_event_coalescer_.Start();

  if (!FLAGS_panel_opaque_resize) {
    DCHECK(!resize_actor_.get());
    resize_actor_.reset(wm()->clutter()->CreateRectangle(kResizeBoxBgColor,
                                                         kResizeBoxBorderColor,
                                                         1));  // border_width
    wm()->stage()->AddActor(resize_actor_.get());
    resize_actor_->Move(
        titlebar_win_->client_x(), titlebar_win_->client_y(), 0);
    resize_actor_->SetSize(
        content_width(),
        content_win_->client_height() + titlebar_win_->client_height());
    resize_actor_->SetOpacity(0, 0);
    resize_actor_->SetOpacity(kResizeBoxOpacity, kResizeActorOpacityAnimMs);
    wm()->stacking_manager()->StackActorAtTopOfLayer(
        resize_actor_.get(), StackingManager::LAYER_STATIONARY_PANEL);
    resize_actor_->SetVisibility(true);
  }
}

void Panel::HandleInputWindowButtonRelease(
    XWindow xid, int x, int y, int button, Time timestamp) {
  if (button != 1)
    return;
  if (xid != drag_xid_) {
    LOG(WARNING) << "Ignoring button release for unexpected input window "
                 << XidStr(xid) << " (currently in drag initiated by "
                 << XidStr(drag_xid_) << ")";
    return;
  }
  // GrabButton-initiated asynchronous pointer grabs are automatically removed
  // by the X server on button release.
  resize_event_coalescer_.StorePosition(x, y);
  resize_event_coalescer_.Stop();
  drag_xid_ = None;

  if (!FLAGS_panel_opaque_resize) {
    DCHECK(resize_actor_.get());
    resize_actor_.reset(NULL);
    Resize(drag_last_width_, drag_last_height_, drag_gravity_);
  }

  ConfigureInputWindows();
}

void Panel::HandleInputWindowPointerMotion(XWindow xid, int x, int y) {
  if (xid != drag_xid_) {
    LOG(WARNING) << "Ignoring motion event for unexpected input window "
                 << XidStr(xid) << " (currently in drag initiated by "
                 << XidStr(drag_xid_) << ")";
    return;
  }
  resize_event_coalescer_.StorePosition(x, y);
}

void Panel::HandleWindowConfigureRequest(
    Window* win, int req_x, int req_y, int req_width, int req_height) {
  DCHECK(win);
  if (drag_xid_ != None) {
    VLOG(1) << "Ignoring configure request for " << win->xid_str()
            << " in panel " << xid_str() << " because the panel is being "
            << "resized by the user";
    return;
  }
  if (win != content_win_) {
    LOG(WARNING) << "Ignoring configure request for non-content window "
                 << win->xid_str() << " in panel " << xid_str();
    return;
  }

  if (req_width != content_win_->client_width() ||
      req_height != content_win_->client_height()) {
    Resize(req_width, req_height, Window::GRAVITY_SOUTHEAST);
  }
}

void Panel::Move(int right, int y, bool move_client_windows, int anim_ms) {
  titlebar_win_->MoveComposited(right - titlebar_width(), y, anim_ms);
  content_win_->MoveComposited(
      right - content_width(), y + titlebar_win_->client_height(), anim_ms);
  if (!composited_windows_set_up_) {
    titlebar_win_->ScaleComposited(1.0, 1.0, 0);
    titlebar_win_->SetCompositedOpacity(1.0, 0);
    titlebar_win_->ShowComposited();
    content_win_->ScaleComposited(1.0, 1.0, 0);
    content_win_->SetCompositedOpacity(1.0, 0);
    content_win_->ShowComposited();
    composited_windows_set_up_ = true;
  }
  if (move_client_windows) {
    titlebar_win_->MoveClientToComposited();
    content_win_->MoveClientToComposited();
    ConfigureInputWindows();
  }
}

void Panel::MoveX(int right, bool move_client_windows, int anim_ms) {
  DCHECK(composited_windows_set_up_)
      << "Move() must be called initially to configure composited windows";
  titlebar_win_->MoveCompositedX(right - titlebar_width(), anim_ms);
  content_win_->MoveCompositedX(right - content_width(), anim_ms);
  if (move_client_windows) {
    titlebar_win_->MoveClientToComposited();
    content_win_->MoveClientToComposited();
    ConfigureInputWindows();
  }
}

void Panel::MoveY(int y, bool move_client_windows, int anim_ms) {
  DCHECK(composited_windows_set_up_)
      << "Move() must be called initially to configure composited windows";
  titlebar_win_->MoveCompositedY(y, anim_ms);
  content_win_->MoveCompositedY(y + titlebar_win_->client_height(), anim_ms);
  if (move_client_windows) {
    titlebar_win_->MoveClientToComposited();
    content_win_->MoveClientToComposited();
    ConfigureInputWindows();
  }
}

void Panel::SetTitlebarWidth(int width) {
  CHECK(width > 0);
  titlebar_win_->ResizeClient(
      width, titlebar_win_->client_height(), Window::GRAVITY_NORTHEAST);
}

void Panel::SetContentShadowOpacity(double opacity, int anim_ms) {
  content_win_->SetShadowOpacity(opacity, anim_ms);
}

void Panel::SetResizable(bool resizable) {
  if (resizable != resizable_) {
    resizable_ = resizable;
    ConfigureInputWindows();
  }
}

void Panel::StackAtTopOfLayer(StackingManager::Layer layer) {
  // Put the titlebar and content in the same layer, but stack the titlebar
  // higher (the stacking between the two is arbitrary but needs to stay in
  // sync with the input window code below).
  wm()->stacking_manager()->StackWindowAtTopOfLayer(content_win_, layer);
  wm()->stacking_manager()->StackWindowAtTopOfLayer(titlebar_win_, layer);

  // Stack all of the input windows directly below the content window
  // (which is stacked beneath the titlebar) -- we don't want the
  // corner windows to occlude the titlebar.
  wm()->xconn()->StackWindow(top_input_xid_, content_win_->xid(), false);
  wm()->xconn()->StackWindow(top_left_input_xid_, content_win_->xid(), false);
  wm()->xconn()->StackWindow(top_right_input_xid_, content_win_->xid(), false);
  wm()->xconn()->StackWindow(left_input_xid_, content_win_->xid(), false);
  wm()->xconn()->StackWindow(right_input_xid_, content_win_->xid(), false);
}

bool Panel::NotifyChromeAboutState(bool expanded) {
  WmIpc::Message msg(WmIpc::Message::CHROME_NOTIFY_PANEL_STATE);
  msg.set_param(0, expanded);
  bool success = wm()->wm_ipc()->SendMessage(content_win_->xid(), msg);

  XAtom atom = wm()->GetXAtom(ATOM_CHROME_STATE_COLLAPSED_PANEL);
  vector<pair<XAtom, bool> > states;
  states.push_back(make_pair(atom, expanded ? false : true));
  success &= content_win_->ChangeChromeState(states);

  return success;
}

void Panel::AddButtonGrab() {
  // We grab button presses on the panel so we'll know when it gets clicked
  // and can focus it.  (We can't just listen on ButtonPressMask, since
  // only one client is allowed to do so for a given window and the app is
  // probably doing it itself.)
  content_win_->AddButtonGrab();
}

void Panel::RemoveButtonGrab(bool remove_pointer_grab) {
  content_win_->RemoveButtonGrab();
  if (remove_pointer_grab)
    wm()->xconn()->RemovePointerGrab(true, CurrentTime);  // replay_events
}

WindowManager* Panel::wm() {
  return panel_manager_->wm();
}

void Panel::Resize(int width, int height, Window::Gravity gravity) {
  DCHECK_GT(width, 0);
  DCHECK_GT(height, 0);

  bool changing_height = (height != content_win_->client_height());

  content_win_->ResizeClient(width, height, gravity);
  titlebar_win_->ResizeClient(width, titlebar_win_->client_height(), gravity);

  // TODO: This is broken if we start resizing scaled windows.
  if (changing_height) {
    titlebar_win_->MoveCompositedY(
        content_win_->composited_y() - titlebar_win_->client_height(), 0);
    titlebar_win_->MoveClientToComposited();
  }

  panel_manager_->HandlePanelResize(this);
}

void Panel::ConfigureInputWindows() {
  if (!resizable_) {
    wm()->xconn()->ConfigureWindowOffscreen(top_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(top_left_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(top_right_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(left_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(right_input_xid_);
    return;
  }

  if (content_width() + 2 * (kResizeBorderWidth - kResizeCornerSize) <= 0) {
    wm()->xconn()->ConfigureWindowOffscreen(top_input_xid_);
  } else {
    wm()->xconn()->ConfigureWindow(
        top_input_xid_,
        content_x() - kResizeBorderWidth + kResizeCornerSize,
        titlebar_win_->client_y() - kResizeBorderWidth,
        content_width() + 2 * (kResizeBorderWidth - kResizeCornerSize),
        kResizeBorderWidth);
  }

  wm()->xconn()->ConfigureWindow(
      top_left_input_xid_,
      content_x() - kResizeBorderWidth,
      titlebar_win_->client_y() - kResizeBorderWidth,
      kResizeCornerSize,
      kResizeCornerSize);
  wm()->xconn()->ConfigureWindow(
      top_right_input_xid_,
      right() + kResizeBorderWidth - kResizeCornerSize,
      titlebar_win_->client_y() - kResizeBorderWidth,
      kResizeCornerSize,
      kResizeCornerSize);

  int total_height =
      titlebar_win_->client_height() + content_win_->client_height();
  int resize_edge_height =
      total_height + kResizeBorderWidth - kResizeCornerSize;
  if (resize_edge_height <= 0) {
    wm()->xconn()->ConfigureWindowOffscreen(left_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(right_input_xid_);
  } else {
    wm()->xconn()->ConfigureWindow(
        left_input_xid_,
        content_x() - kResizeBorderWidth,
        titlebar_win_->client_y() - kResizeBorderWidth + kResizeCornerSize,
        kResizeBorderWidth,
        resize_edge_height);
    wm()->xconn()->ConfigureWindow(
        right_input_xid_,
        right(),
        titlebar_win_->client_y() - kResizeBorderWidth + kResizeCornerSize,
        kResizeBorderWidth,
        resize_edge_height);
  }
}

void Panel::ApplyResize() {
  int dx = resize_event_coalescer_.x() - drag_start_x_;
  int dy = resize_event_coalescer_.y() - drag_start_y_;
  drag_gravity_ = Window::GRAVITY_NORTHWEST;

  if (drag_xid_ == top_input_xid_) {
    drag_gravity_ = Window::GRAVITY_SOUTHWEST;
    dx = 0;
    dy *= -1;
  } else if (drag_xid_ == top_left_input_xid_) {
    drag_gravity_ = Window::GRAVITY_SOUTHEAST;
    dx *= -1;
    dy *= -1;
  } else if (drag_xid_ == top_right_input_xid_) {
    drag_gravity_ = Window::GRAVITY_SOUTHWEST;
    dy *= -1;
  } else if (drag_xid_ == left_input_xid_) {
    drag_gravity_ = Window::GRAVITY_NORTHEAST;
    dx *= -1;
    dy = 0;
  } else if (drag_xid_ == right_input_xid_) {
    drag_gravity_ = Window::GRAVITY_NORTHWEST;
    dy = 0;
  }

  drag_last_width_ = max(drag_orig_width_ + dx, kPanelMinWidth);
  drag_last_height_ = max(drag_orig_height_ + dy, kPanelMinHeight);

  if (FLAGS_panel_opaque_resize) {
    Resize(drag_last_width_, drag_last_height_, drag_gravity_);
  } else {
    if (resize_actor_.get()) {
      int actor_x = titlebar_win_->client_x();
      if (drag_gravity_ == Window::GRAVITY_SOUTHEAST ||
          drag_gravity_ == Window::GRAVITY_NORTHEAST) {
        actor_x -= (drag_last_width_ - drag_orig_width_);
      }
      int actor_y = titlebar_win_->client_y();
      if (drag_gravity_ == Window::GRAVITY_SOUTHWEST ||
          drag_gravity_ == Window::GRAVITY_SOUTHEAST) {
        actor_y -= (drag_last_height_ - drag_orig_height_);
      }
      resize_actor_->Move(actor_x, actor_y, 0);
      resize_actor_->SetSize(
          drag_last_width_, drag_last_height_ + titlebar_win_->client_height());
    }
  }
}

}  // namespace window_manager
