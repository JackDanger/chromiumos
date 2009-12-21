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
#include "window_manager/panel_bar.h"
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

// Width of titlebars for collapsed panels.  Expanded panels' titlebars are
// resized to match the width of the panel contents.
static const int kCollapsedTitlebarWidth = 200;

// Amount of time to take for animations.
static const int kAnimMs = 150;

// Minimum dimensions to which a panel's contents can be resized.
static const int kPanelMinWidth = 20;
static const int kPanelMinHeight = 20;

// Frequency with which we should update the size of resized panels.
static const int kResizeUpdateMs = 25;

// Appearance of the box used for non-opaque resizing.
static const char* kResizeBoxBgColor = "#4181f5";
static const char* kResizeBoxBorderColor = "#234583";
static const double kResizeBoxOpacity = 0.3;

const int Panel::kResizeBorderWidth = 5;
const int Panel::kResizeCornerSize = 25;

Panel::Panel(PanelBar* panel_bar,
             Window* panel_win,
             Window* titlebar_win,
             int initial_right)
    : panel_bar_(panel_bar),
      panel_win_(panel_win),
      titlebar_win_(titlebar_win),
      resize_actor_(NULL),
      resize_event_coalescer_(
          NewPermanentCallback(this, &Panel::ApplyResize),
          kResizeUpdateMs),
      top_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1)),
      top_left_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1)),
      top_right_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1)),
      left_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1)),
      right_input_xid_(wm()->CreateInputWindow(-1, -1, 1, 1)),
      snapped_right_(initial_right),
      is_expanded_(false),
      drag_xid_(0),
      drag_start_x_(0),
      drag_start_y_(0),
      drag_orig_width_(1),
      drag_orig_height_(1),
      drag_last_width_(1),
      drag_last_height_(1) {
  CHECK(panel_bar_);
  CHECK(panel_win_);
  CHECK(titlebar_win_);

  // We need to grab button presses on the panel so we'll know when it gets
  // clicked and can focus it.  (We can't just listen on ButtonPressMask,
  // since only one client is allowed to do so for a given window and the
  // app is probably doing it itself.)
  panel_win_->AddPassiveButtonGrab();

  // Constrain the size of the panel if we've been requested to do so.
  int panel_width = (FLAGS_panel_max_width > 0) ?
      std::min(panel_win_->client_width(), FLAGS_panel_max_width) :
      panel_win_->client_width();
  int panel_height = (FLAGS_panel_max_height > 0) ?
      std::min(panel_win_->client_height(), FLAGS_panel_max_height) :
      panel_win_->client_height();
  if (panel_width != panel_win_->client_width() ||
      panel_height != panel_win_->client_height()) {
    panel_win_->ResizeClient(
        panel_width, panel_height, Window::GRAVITY_NORTHWEST);
  }

  titlebar_win_->ResizeClient(
      kCollapsedTitlebarWidth, titlebar_win_->client_height(),
      Window::GRAVITY_NORTHWEST);

  titlebar_win_->ScaleComposited(1.0, 1.0, 0);
  titlebar_win_->SetCompositedOpacity(1.0, 0);
  titlebar_win_->MoveComposited(
      snapped_titlebar_left(), panel_bar_->y() + panel_bar_->height(), 0);
  titlebar_win_->ShowComposited();

  int titlebar_y = panel_bar_->y() + panel_bar_->height() -
      titlebar_win_->client_height();
  titlebar_win_->MoveComposited(snapped_titlebar_left(), titlebar_y, kAnimMs);
  titlebar_win_->MoveClientToComposited();

  panel_win_->ScaleComposited(1.0, 1.0, 0);
  panel_win_->SetCompositedOpacity(1.0, 0);
  panel_win_->MoveComposited(
      snapped_panel_left(), panel_bar_->y() + panel_bar_->height(), 0);
  panel_win_->SetShadowOpacity(0, 0);
  panel_win_->MoveClientToComposited();
  panel_win_->ShowComposited();

  wm()->xconn()->SetWindowCursor(top_input_xid_, XC_top_side);
  wm()->xconn()->SetWindowCursor(top_left_input_xid_, XC_top_left_corner);
  wm()->xconn()->SetWindowCursor(top_right_input_xid_, XC_top_right_corner);
  wm()->xconn()->SetWindowCursor(left_input_xid_, XC_left_side);
  wm()->xconn()->SetWindowCursor(right_input_xid_, XC_right_side);
  ConfigureInputWindows();

  UpdateChromeStateProperty();
  NotifyChromeAboutState();
}

Panel::~Panel() {
  if (drag_xid_) {
    wm()->xconn()->RemoveActivePointerGrab(
        false, CurrentTime);  // replay_events=false
    drag_xid_ = None;
  }
  wm()->xconn()->DestroyWindow(top_input_xid_);
  wm()->xconn()->DestroyWindow(top_left_input_xid_);
  wm()->xconn()->DestroyWindow(top_right_input_xid_);
  wm()->xconn()->DestroyWindow(left_input_xid_);
  wm()->xconn()->DestroyWindow(right_input_xid_);
  panel_win_->RemovePassiveButtonGrab();
  panel_bar_ = NULL;
  panel_win_ = NULL;
  titlebar_win_ = NULL;
  top_input_xid_ = None;
  top_left_input_xid_ = None;
  top_right_input_xid_ = None;
  left_input_xid_ = None;
  right_input_xid_ = None;
}

int Panel::cur_right() const {
  return cur_panel_left() + panel_width();
}

int Panel::cur_panel_left() const {
  return panel_win_->composited_x();
}

int Panel::cur_titlebar_left() const {
  return titlebar_win_->composited_x();
}

int Panel::cur_panel_center() const {
  return cur_panel_left() + 0.5 * panel_width();
}

int Panel::snapped_panel_left() const {
  return snapped_right_ - panel_width();
}

int Panel::snapped_titlebar_left() const {
  return snapped_right_ - titlebar_width();
}

int Panel::panel_width() const {
  return panel_win_->client_width();
}

int Panel::titlebar_width() const {
  return titlebar_win_->client_width();
}

void Panel::GetInputWindows(std::vector<XWindow>* windows_out) {
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
  DCHECK(drag_xid_ == None);

  if (!wm()->xconn()->AddActivePointerGrabForWindow(
          xid, ButtonReleaseMask|PointerMotionMask, timestamp)) {
    return;
  }

  drag_xid_ = xid;
  drag_start_x_ = x;
  drag_start_y_ = y;
  drag_orig_width_ = drag_last_width_ = panel_width();
  drag_orig_height_ = drag_last_height_ = panel_win_->client_height();
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
        panel_width(),
        panel_win_->client_height() + titlebar_win_->client_height());
    resize_actor_->SetOpacity(0, 0);
    resize_actor_->SetOpacity(kResizeBoxOpacity, kAnimMs);
    wm()->stacking_manager()->StackActorAtTopOfLayer(
        resize_actor_.get(), StackingManager::LAYER_EXPANDED_PANEL);
    resize_actor_->SetVisibility(true);
  }
}

void Panel::HandleInputWindowButtonRelease(
    XWindow xid, int x, int y, int button, Time timestamp) {
  if (button != 1) {
    return;
  }
  if (xid != drag_xid_) {
    LOG(WARNING) << "Ignoring button release for unexpected input window "
                 << XidStr(xid) << " (currently in drag initiated by "
                 << XidStr(drag_xid_) << ")";
    return;
  }
  wm()->xconn()->RemoveActivePointerGrab(
      false, timestamp);  // replay_events=false
  resize_event_coalescer_.StorePosition(x, y);
  resize_event_coalescer_.Stop();
  drag_xid_ = None;

  if (!FLAGS_panel_opaque_resize) {
    DCHECK(resize_actor_.get());
    resize_actor_.reset(NULL);
    Resize(drag_last_width_, drag_last_height_, drag_gravity_, false);
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

  drag_last_width_ = std::max(drag_orig_width_ + dx, kPanelMinWidth);
  drag_last_height_ = std::max(drag_orig_height_ + dy, kPanelMinHeight);

  if (FLAGS_panel_opaque_resize) {
    Resize(drag_last_width_, drag_last_height_, drag_gravity_, false);
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

void Panel::SetState(bool is_expanded) {
  if (is_expanded_ == is_expanded) {
    return;
  }

  if (is_expanded) {
    StackAtTopOfLayer(StackingManager::LAYER_EXPANDED_PANEL);

    // Animate the panel sliding up.
    panel_win_->MoveComposited(
        cur_panel_left(),
        panel_bar_->y() - panel_win_->client_height(),
        kAnimMs);
    panel_win_->SetShadowOpacity(1.0, kAnimMs);
    panel_win_->MoveClientToComposited();

    // Move the titlebar right above the panel.  We left-justify it with
    // the panel before animating it to match the panel's width so it won't
    // end up sticking out to the right after the resize.
    titlebar_win_->MoveCompositedX(cur_panel_left(), 0);
    titlebar_win_->ResizeClient(
        panel_win_->client_width(), titlebar_win_->client_height(),
        Window::GRAVITY_NORTHWEST);
    titlebar_win_->MoveComposited(
        cur_panel_left(),
        panel_bar_->y() -
          panel_win_->client_height() - titlebar_win_->client_height(),
        kAnimMs);
    titlebar_win_->MoveClientToComposited();
  } else {
    StackAtTopOfLayer(StackingManager::LAYER_COLLAPSED_PANEL);

    panel_win_->MoveComposited(
        cur_panel_left(),
        panel_bar_->y() + panel_bar_->height(),
        kAnimMs);
    // Hide the shadow so it's not peeking up at the bottom of the screen.
    panel_win_->SetShadowOpacity(0, kAnimMs);
    panel_win_->MoveClientToComposited();

    // Resize and right-justify the titlebar before animating it.
    titlebar_win_->ResizeClient(
        kCollapsedTitlebarWidth, titlebar_win_->client_height(),
        Window::GRAVITY_NORTHWEST);
    titlebar_win_->MoveCompositedX(cur_right() - kCollapsedTitlebarWidth, 0);
    titlebar_win_->MoveComposited(
        cur_right() - titlebar_win_->client_width(),
        panel_bar_->y() + panel_bar_->height() - titlebar_win_->client_height(),
        kAnimMs);
    titlebar_win_->MoveClientToComposited();
  }

  // Notify Chrome about the changed state.
  is_expanded_ = is_expanded;
  UpdateChromeStateProperty();
  NotifyChromeAboutState();

  ConfigureInputWindows();
}

void Panel::Move(int right, int anim_ms) {
  // TODO: If the user is dragging the panel, we should probably only move
  // the X windows (titlebar, panel, and input) when the drag is complete.
  titlebar_win_->MoveComposited(
      right - titlebar_width(),
      titlebar_win_->composited_y(),
      anim_ms);
  titlebar_win_->MoveClientToComposited();

  panel_win_->MoveComposited(
      right - panel_width(),
      panel_win_->composited_y(),
      anim_ms);
  panel_win_->MoveClientToComposited();

  ConfigureInputWindows();
}

void Panel::HandlePanelBarMove() {
  if (is_expanded_) {
    panel_win_->MoveCompositedY(
        panel_bar_->y() - panel_win_->client_height(), 0);
    panel_win_->MoveClientToComposited();
    titlebar_win_->MoveCompositedY(
        panel_bar_->y() -
          panel_win_->client_height() - titlebar_win_->client_height(),
        0);
    titlebar_win_->MoveClientToComposited();
  } else {
    panel_win_->MoveCompositedY(panel_bar_->y() + panel_bar_->height(), 0);
    panel_win_->MoveClientToComposited();
    titlebar_win_->MoveCompositedY(
        panel_bar_->y() + panel_bar_->height() - titlebar_win_->client_height(),
        0);
    titlebar_win_->MoveClientToComposited();
  }
}

void Panel::StackAtTopOfLayer(StackingManager::Layer layer) {
  // Put the titlebar and panel in the same layer, but stack the titlebar
  // higher (the stacking between the two is arbitrary but needs to stay
  // in sync with ConfigureInputWindows()).
  wm()->stacking_manager()->StackWindowAtTopOfLayer(panel_win_, layer);
  wm()->stacking_manager()->StackWindowAtTopOfLayer(titlebar_win_, layer);

  /// Ensure that the resize windows are stacked correctly.
  ConfigureInputWindows();
}

WindowManager* Panel::wm() {
  return panel_bar_->wm();
}

void Panel::Resize(int width, int height,
                   Window::Gravity gravity,
                   bool configure_input_windows) {
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);

  bool changing_height = (height != panel_win_->client_height());

  panel_win_->ResizeClient(width, height, gravity);
  titlebar_win_->ResizeClient(width, titlebar_win_->client_height(), gravity);

  // TODO: This is broken if we start resizing scaled windows.
  // Is this a concern?
  if (changing_height) {
    titlebar_win_->MoveCompositedY(
        panel_win_->composited_y() - titlebar_win_->client_height(), 0);
    titlebar_win_->MoveClientToComposited();
  }

  if (configure_input_windows) {
    ConfigureInputWindows();
  }
}

bool Panel::UpdateChromeStateProperty() {
  XAtom atom = wm()->GetXAtom(ATOM_CHROME_STATE_COLLAPSED_PANEL);
  std::vector<std::pair<XAtom, bool> > states;
  states.push_back(std::make_pair(atom, is_expanded_ ? false : true));
  return panel_win_->ChangeChromeState(states);
}

bool Panel::NotifyChromeAboutState() {
  WmIpc::Message msg(WmIpc::Message::CHROME_NOTIFY_PANEL_STATE);
  msg.set_param(0, is_expanded_);
  return wm()->wm_ipc()->SendMessage(panel_win_->xid(), msg);
}

void Panel::ConfigureInputWindows() {
  if (is_expanded_) {
    if (panel_width() + 2 * (kResizeBorderWidth - kResizeCornerSize) <= 0) {
      wm()->xconn()->ConfigureWindowOffscreen(top_input_xid_);
    } else {
      // Stack all of the input windows directly below the panel window
      // (which is stacked beneath the titlebar) -- we don't want the
      // corner windows to occlude the titlebar.
      wm()->xconn()->StackWindow(top_input_xid_, panel_win_->xid(), false);
      wm()->xconn()->ConfigureWindow(
          top_input_xid_,
          cur_panel_left() - kResizeBorderWidth + kResizeCornerSize,
          titlebar_win_->client_y() - kResizeBorderWidth,
          panel_width() + 2 * (kResizeBorderWidth - kResizeCornerSize),
          kResizeBorderWidth);
    }

    wm()->xconn()->StackWindow(top_left_input_xid_, panel_win_->xid(), false);
    wm()->xconn()->ConfigureWindow(
        top_left_input_xid_,
        cur_panel_left() - kResizeBorderWidth,
        titlebar_win_->client_y() - kResizeBorderWidth,
        kResizeCornerSize,
        kResizeCornerSize);

    wm()->xconn()->StackWindow(top_right_input_xid_, panel_win_->xid(), false);
    wm()->xconn()->ConfigureWindow(
        top_right_input_xid_,
        cur_right() + kResizeBorderWidth - kResizeCornerSize,
        titlebar_win_->client_y() - kResizeBorderWidth,
        kResizeCornerSize,
        kResizeCornerSize);

    int total_height =
        titlebar_win_->client_height() + panel_win_->client_height();
    int resize_edge_height =
        total_height + kResizeBorderWidth - kResizeCornerSize;

    if (resize_edge_height <= 0) {
      wm()->xconn()->ConfigureWindowOffscreen(left_input_xid_);
      wm()->xconn()->ConfigureWindowOffscreen(right_input_xid_);
    } else {
      wm()->xconn()->StackWindow(left_input_xid_, panel_win_->xid(), false);
      wm()->xconn()->ConfigureWindow(
          left_input_xid_,
          cur_panel_left() - kResizeBorderWidth,
          titlebar_win_->client_y() - kResizeBorderWidth + kResizeCornerSize,
          kResizeBorderWidth,
          resize_edge_height);

      wm()->xconn()->StackWindow(right_input_xid_, panel_win_->xid(), false);
      wm()->xconn()->ConfigureWindow(
          right_input_xid_,
          cur_right(),
          titlebar_win_->client_y() - kResizeBorderWidth + kResizeCornerSize,
          kResizeBorderWidth,
          resize_edge_height);
    }
  } else {
    // Move the windows offscreen if the panel is collapsed.
    wm()->xconn()->ConfigureWindowOffscreen(top_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(top_left_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(top_right_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(left_input_xid_);
    wm()->xconn()->ConfigureWindowOffscreen(right_input_xid_);
  }
}

}  // namespace window_manager
