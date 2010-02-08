// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_DOCK_H_
#define WINDOW_MANAGER_PANEL_DOCK_H_

extern "C" {
#include <X11/Xlib.h>
}

#include "base/basictypes.h"
#include "window_manager/panel_container.h"

typedef ::Window XWindow;

namespace window_manager {

class Panel;
class Window;
class WindowManager;

// Panel docks handle panels that are pinned to the left and right sides of
// the screen.
class PanelDock : public PanelContainer {
 public:
  enum DockPosition {
    DOCK_POSITION_LEFT = 0,
    DOCK_POSITION_RIGHT,
  };
  PanelDock(WindowManager* wm, DockPosition position);
  ~PanelDock();

  // Begin overridden PanelContainer methods.
  void GetInputWindows(std::vector<XWindow>* windows_out) {}
  void AddPanel(Panel* panel, PanelSource source, bool expanded);
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
  void HandleScreenResize() {}
  // End overridden PanelContainer methods.

 private:
  void FocusPanel(Panel* panel, bool remove_pointer_grab, Time timestamp);

  WindowManager* wm_;  // not owned

  DockPosition position_;

  Panel* dragged_panel_;

  DISALLOW_COPY_AND_ASSIGN(PanelDock);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_DOCK_H_
