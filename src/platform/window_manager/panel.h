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

class PanelManager;
class WindowManager;

// A panel, representing a pop-up window.  Each panel consists of a content
// window (the panel's contents) and a titlebar window (a small window
// drawn in the bar when the panel is collapsed or at the top of the panel
// when it's expanded).  The right edges of the titlebar and content
// windows are aligned.
class Panel {
 public:
  // The panel's windows will remain untouched until Move() is invoked.
  // (PanelManager would have previously moved the client windows offscreen
  // in response to their map requests, and Window's c'tor makes composited
  // windows invisible.)
  Panel(PanelManager* panel_manager,
        Window* content_win,
        Window* titlebar_win,
        bool is_expanded);
  ~Panel();

  bool is_expanded() const { return is_expanded_; }

  const Window* const_content_win() const { return content_win_; }
  Window* content_win() { return content_win_; }
  Window* titlebar_win() { return titlebar_win_; }

  XWindow content_xid() const { return content_win_->xid(); }
  XWindow titlebar_xid() const { return titlebar_win_->xid(); }

  // Get the X ID of the content window.  This is handy for logging.
  const std::string& xid_str() const { return content_win_->xid_str(); }

  // The current position of one pixel beyond the right edge of the panel.
  int right() const { return content_x() + content_width(); }

  // The current left edge of the content or titlebar window (that is, its
  // composited position).
  int content_x() const { return content_win_->composited_x(); }
  int titlebar_x() const { return titlebar_win_->composited_x(); }
  int content_center() const { return content_x() + 0.5 * content_width(); }

  int titlebar_y() const { return titlebar_win_->composited_y(); }

  // TODO: Remove content and titlebar width.
  int content_width() const { return content_win_->client_width(); }
  int titlebar_width() const { return titlebar_win_->client_width(); }
  int width() const { return content_win_->client_width(); }

  int content_height() const { return content_win_->client_height(); }
  int titlebar_height() const { return titlebar_win_->client_height(); }
  int total_height() const { return content_height() + titlebar_height(); }

  // Fill the passed-in vector with all of the panel's input windows (in an
  // arbitrary order).
  void GetInputWindows(std::vector<XWindow>* windows_out);

  // Handle events occurring in one of our input windows.
  void HandleInputWindowButtonPress(
      XWindow xid, int x, int y, int button, Time timestamp);
  void HandleInputWindowButtonRelease(
      XWindow xid, int x, int y, int button, Time timestamp);
  void HandleInputWindowPointerMotion(XWindow xid, int x, int y);

  // Handle a configure request for the titlebar or content window.
  void HandleWindowConfigureRequest(
      Window* win, int req_x, int req_y, int req_width, int req_height);

  // Move the panel.  'right' is given in terms of one pixel beyond
  // the panel's right edge (since content and titlebar windows share a
  // common right edge), while 'y' is the top of the titlebar window.
  // For example, to place the left column of a 10-pixel-wide panel at
  // X-coordinate 0 and the right column at 9, pass 10 for 'right'.
  //
  // Note: Move() must be called initially to configure the windows (see
  // the constructor's comment).
  void Move(int right, int y, bool move_client_windows, int anim_ms);
  void MoveX(int right, bool move_client_windows, int anim_ms);
  void MoveY(int y, bool move_client_windows, int anim_ms);

  // Set the titlebar window's width (while keeping it right-aligned with
  // the content window).
  void SetTitlebarWidth(int width);

  // Set the opacity of the titlebar and content windows' drop shadows.
  void SetShadowOpacity(double opacity, int anim_ms);

  // Set whether the panel should be resizable by dragging its borders.
  void SetResizable(bool resizable);

  // Stack the panel's client and composited windows at the top of the
  // passed-in layer.  Input windows are included.
  void StackAtTopOfLayer(StackingManager::Layer layer);

  // Stack the panel's client, composited, and input windows directly above
  // another panel.
  void StackAbovePanel(Panel* sibling, StackingManager::Layer layer);

  // Update 'is_expanded_'.  If it has changed, also notify Chrome about the
  // panel's current visibility state and update the content window's
  // _CHROME_STATE property.  Returns false if notifying Chrome fails (but
  // still updates the local variable).
  bool SetExpandedState(bool expanded);

  // Add a button grab on the content window so we'll be notified when it
  // gets clicked.  This is done for unfocused panels.
  void AddButtonGrab();

  // Remove a button grab on the content window, optionally also removing a
  // pointer grab and replaying the click that triggered the pointer grab.
  void RemoveButtonGrab(bool remove_pointer_grab);

  // Resize the content window to the passed-in dimensions.  The titlebar
  // window is moved above the content window if necessary and resized to
  // match the content window's width.  Additionally, the input windows are
  // configured.
  void ResizeContent(int width, int height, Window::Gravity gravity);

 private:
  FRIEND_TEST(PanelBarTest, PackPanelsAfterPanelResize);
  FRIEND_TEST(PanelManagerTest, ChromeInitiatedPanelResize);
  FRIEND_TEST(PanelTest, InputWindows);  // uses '*_input_xid_'
  FRIEND_TEST(PanelTest, Resize);        // uses '*_input_xid_'

  WindowManager* wm();

  // Move and resize the input windows appropriately for the panel's
  // current configuration.
  void ConfigureInputWindows();

  // Stack the input windows directly below the content window.
  void StackInputWindows();

  // Called periodically by 'resize_event_coalescer_'.
  void ApplyResize();

  // Update the content window's _CHROME_STATE property according to the
  // current value of 'is_expanded_'.
  bool UpdateChromeStateProperty();

  PanelManager* panel_manager_;  // not owned
  Window* content_win_;          // not owned
  Window* titlebar_win_;         // not owned

  // Is the panel currently expanded?  The Panel class does little itself
  // with this information; most work is left to PanelContainer
  // implementations.
  bool is_expanded_;

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

  // Should we configure handles around the panel that can be dragged to
  // resize it?
  bool resizable_;

  // Have the composited windows been scaled and shown?  We defer doing
  // this until the first time that Move() is called.
  bool composited_windows_set_up_;

  // XID of the input window currently being dragged to resize the panel,
  // or None if no drag is in progress.
  XWindow drag_xid_;

  // Gravity holding a corner in place as the panel is being resized (e.g.
  // GRAVITY_SOUTHEAST if 'top_left_input_xid_' is being dragged).
  Window::Gravity drag_gravity_;

  // Pointer coordinates where the resize drag started.
  int drag_start_x_;
  int drag_start_y_;

  // Initial content window size at the start of the resize.
  int drag_orig_width_;
  int drag_orig_height_;

  // Most-recent content window size during a resize.
  int drag_last_width_;
  int drag_last_height_;

  DISALLOW_COPY_AND_ASSIGN(Panel);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_H_
