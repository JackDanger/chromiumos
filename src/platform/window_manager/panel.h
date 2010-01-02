// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_H_
#define WINDOW_MANAGER_PANEL_H_

extern "C" {
#include <X11/Xlib.h>
}

#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/motion_event_coalescer.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/window.h"  // for Window::Gravity

typedef ::Window XWindow;

namespace window_manager {

class PanelBar;
class WindowManager;

// A single panel.  Each panel consists of both a content window (the
// panel's contents) and a titlebar window (a small window drawn in the bar
// when the panel is collapsed or at the top of the panel when it's
// expanded).  'initial_right' is the initial position of the right edge of
// the panel.
class Panel {
 public:
  Panel(PanelBar* panel_bar,
        Window* content_win,
        Window* titlebar_win,
        int initial_right);
  ~Panel();

  const Window* const_content_win() const { return content_win_; }
  Window* content_win() { return content_win_; }
  Window* titlebar_win() { return titlebar_win_; }
  int snapped_right() const { return snapped_right_; }
  void set_snapped_right(int x) { snapped_right_ = x; }
  bool is_expanded() const { return is_expanded_; }

  // Get the X ID of the content window.  This is handy for logging.
  const std::string& xid_str() const { return content_win_->xid_str(); }

  // The current position of one pixel beyond the right edge of the panel.
  int cur_right() const;

  // The current left edge of the content or titlebar window (that is, its
  // composited position).
  int cur_content_left() const;
  int cur_titlebar_left() const;
  int cur_content_center() const;

  // The snapped left edge of the content or titlebar window (see
  // 'snapped_right_' for details).
  int snapped_content_left() const;
  int snapped_titlebar_left() const;

  // Width of the content or titlebar windows.
  int content_width() const;
  int titlebar_width() const;

  // Fill the passed-in vector with all of the panel's input windows (in an
  // arbitrary order).
  void GetInputWindows(std::vector<XWindow>* windows_out);

  // Handle events occurring in on one of our input windows.
  void HandleInputWindowButtonPress(
      XWindow xid, int x, int y, int button, Time timestamp);
  void HandleInputWindowButtonRelease(
      XWindow xid, int x, int y, int button, Time timestamp);
  void HandleInputWindowPointerMotion(XWindow xid, int x, int y);

  // Expand or collapse the panel, notifying the client app of the change.
  void SetState(bool is_expanded);

  // Move the panel.  Positions are given in terms of panels' right
  // edges (since content and titlebar windows share a common right edge).
  // TODO: This is weird; 'right' is actually one pixel beyond the panel's
  // right edge.
  void Move(int right, int anim_ms);

  // Handle the panel bar being moved.  This just updates our Y value; the
  // panel bar is responsible for moving all of the panels left or right as
  // needed.
  void HandlePanelBarMove();

  // Stack the panel's client and composited windows at the top of the
  // passed-in layer.
  void StackAtTopOfLayer(StackingManager::Layer layer);

 private:
  FRIEND_TEST(PanelTest, InputWindows);  // uses '*_input_xid_'
  FRIEND_TEST(PanelTest, Resize);        // uses '*_input_xid_'

  WindowManager* wm();

  void Resize(int width, int height,
              Window::Gravity gravity,
              bool configure_input_windows);

  // Update the content window's _CHROME_STATE property to reflect the
  // current expanded/collapsed state.
  bool UpdateChromeStateProperty();

  // Notify Chrome about the panel's current visibility state.
  bool NotifyChromeAboutState();

  // Called periodically by 'resize_event_coalescer_'.
  void ApplyResize();

  // Position, resize, and stack the input windows appropriately for the
  // panel's current configuration.
  void ConfigureInputWindows();

  PanelBar* panel_bar_;   // not owned
  Window* content_win_;   // not owned
  Window* titlebar_win_;  // not owned

  // Translucent resize box used when opaque resizing is disabled.
  scoped_ptr<ClutterInterface::Actor> resize_actor_;

  // Batches motion events for resized panels so that we can rate-limit the
  // frequency of their processing.
  MotionEventCoalescer resize_event_coalescer_;

  // Width of the invisible border drawn around a window for use in resizing,
  // in pixels.
  static const int kResizeBorderWidth;

  // Size in pixels of the corner parts of the resize border.
  //
  //       C              W is kResizeBorderWidth
  //   +-------+----      C is kResizeCornerSize
  //   |       | W
  // C |   +---+----
  //   |   |
  //   +---+  titlebar window
  //   | W |
  static const int kResizeCornerSize;

  // Used to catch clicks for resizing.
  XWindow top_input_xid_;
  XWindow top_left_input_xid_;
  XWindow top_right_input_xid_;
  XWindow left_input_xid_;
  XWindow right_input_xid_;

  // X position of the right edge of where the titlebar wants to be when
  // collapsed.  For collapsed panels that are being dragged, this may be
  // different from the actual composited position -- we only snap the
  // panels to this position when the drag is complete.
  int snapped_right_;

  // Is the panel expanded or collapsed?
  bool is_expanded_;

  XWindow drag_xid_;
  Window::Gravity drag_gravity_;
  int drag_start_x_;
  int drag_start_y_;
  int drag_orig_width_;
  int drag_orig_height_;
  int drag_last_width_;
  int drag_last_height_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_H_
