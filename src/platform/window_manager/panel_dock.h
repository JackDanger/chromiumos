// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_DOCK_H_
#define WINDOW_MANAGER_PANEL_DOCK_H_

#include <vector>

extern "C" {
#include <X11/Xlib.h>
}

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/panel_container.h"

typedef ::Window XWindow;

namespace window_manager {

class EventConsumerRegistrar;
class Panel;
class PanelManager;
class Shadow;
class Window;
class WindowManager;

// Panel docks handle panels that are pinned to the left and right sides of
// the screen.
class PanelDock : public PanelContainer {
 public:
  enum DockType {
    DOCK_TYPE_LEFT = 0,
    DOCK_TYPE_RIGHT,
  };
  PanelDock(PanelManager* panel_manager, DockType type, int width);
  ~PanelDock();

  int width() const { return width_; }

  // Is the dock currently visible?
  bool IsVisible() const { return !panels_.empty(); }

  // Begin PanelContainer implementation.
  void GetInputWindows(std::vector<XWindow>* windows_out);
  void AddPanel(Panel* panel, PanelSource source);
  void RemovePanel(Panel* panel);
  bool ShouldAddDraggedPanel(const Panel* panel, int drag_x, int drag_y);
  void HandleInputWindowButtonPress(XWindow xid,
                                    int x, int y,
                                    int x_root, int y_root,
                                    int button,
                                    Time timestamp) {}
  void HandleInputWindowButtonRelease(XWindow xid,
                                      int x, int y,
                                      int x_root, int y_root,
                                      int button,
                                      Time timestamp) {}
  void HandleInputWindowPointerEnter(XWindow xid,
                                     int x, int y,
                                     int x_root, int y_root,
                                     Time timestamp) {}
  void HandleInputWindowPointerLeave(XWindow xid,
                                     int x, int y,
                                     int x_root, int y_root,
                                     Time timestamp) {}
  void HandlePanelButtonPress(Panel* panel, int button, Time timestamp);
  void HandlePanelTitlebarPointerEnter(Panel* panel, Time timestamp) {}
  void HandlePanelFocusChange(Panel* panel, bool focus_in);
  void HandleSetPanelStateMessage(Panel* panel, bool expand);
  bool HandleNotifyPanelDraggedMessage(Panel* panel, int drag_x, int drag_y);
  void HandleNotifyPanelDragCompleteMessage(Panel* panel);
  void HandleFocusPanelMessage(Panel* panel);
  void HandlePanelResize(Panel* panel) {}
  void HandleScreenResize();
  void HandlePanelUrgencyChange(Panel* panel) {}
  // End PanelContainer implementation.

 private:
  WindowManager* wm();

  // Expand or collapse the passed-in panel.  The panels below the modified
  // one are packed as needed.
  void ExpandPanel(Panel* panel);
  void CollapsePanel(Panel* panel);

  // Pack all panels from 'starting_panel' to the bottom of the dock
  // together.  Restacks panels as needed.
  void PackPanels(Panel* starting_panel);

  // Focus a panel.
  void FocusPanel(Panel* panel, bool remove_pointer_grab, Time timestamp);

  // Get the expanded panel that's nearest (in terms of number of
  // intervening collapsed panels) to the passed-in panel, or NULL if
  // there aren't any other expanded panels in the dock.
  Panel* GetNearestExpandedPanel(Panel* panel);

  PanelManager* panel_manager_;  // not owned

  DockType type_;

  // The dock's position and size.  Note that if the dock contains no
  // panels, it will hide to the side of its default position ('type_'
  // determines whether it'll hide to the left or right).
  int x_;
  int y_;
  int width_;
  int height_;

  std::vector<Panel*> panels_;

  // The currently-dragged panel, or NULL if no panel in the dock is being
  // dragged.
  Panel* dragged_panel_;

  // The dock's background image and its drop shadow.
  scoped_ptr<ClutterInterface::Actor> bg_actor_;
  scoped_ptr<Shadow> bg_shadow_;

  // An input window at the same position as the dock.  Currently just used
  // to catch and discard input events so they don't fall through.
  XWindow bg_input_xid_;

  // PanelManager event registrations related to the dock's input windows.
  scoped_ptr<EventConsumerRegistrar> event_consumer_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PanelDock);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_DOCK_H_
