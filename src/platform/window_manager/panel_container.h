// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_CONTAINER_H_
#define WINDOW_MANAGER_PANEL_CONTAINER_H_

extern "C" {
#include <X11/Xlib.h>
}
#include <vector>

#include "base/basictypes.h"

typedef ::Window XWindow;

namespace window_manager {

class Panel;

// Interface for containers that can hold panels.
class PanelContainer {
 public:
  PanelContainer() {}
  virtual ~PanelContainer() {}

  // Fill the passed-in vector with all of this container's input windows
  // (in an arbitrary order).  Input windows belonging to contained panels
  // should not be included.
  //
  // Note that this is only called once, right after the container is
  // constructed.  In other words, containers must create all input windows
  // that they will need in their constructors.
  virtual void GetInputWindows(std::vector<XWindow>* windows_out) = 0;

  // Where did this panel come from?  Determines how it's animated when
  // being added.
  enum PanelSource {
    // Newly-opened panel.
    PANEL_SOURCE_NEW = 0,

    // Panel was attached to this container by being dragged into it, and
    // is still being dragged.
    PANEL_SOURCE_DRAGGED,

    // Panel is being attached to this panel after being dropped.
    PANEL_SOURCE_DROPPED,
  };

  // Add a panel to this container.  Ownership of the object's memory
  // remains with the caller.  The container should add a button grab on
  // the panel if it doesn't focus it.
  virtual void AddPanel(Panel* panel, PanelSource source) = 0;

  // Remove a panel from this container.  Ownership remains with the
  // caller.  Note that this may be a panel that's currently being dragged.
  virtual void RemovePanel(Panel* panel) = 0;

  // Is the passed-in panel (which isn't currently in any container) being
  // dragged to a position such that it should be added to this container?
  virtual bool ShouldAddDraggedPanel(const Panel* panel,
                                     int drag_x,
                                     int drag_y) = 0;

  // Handle pointer events occurring in the container's input windows.
  virtual void HandleInputWindowButtonPress(XWindow xid,
                                            int x, int y,
                                            int x_root, int y_root,
                                            int button,
                                            Time timestamp) = 0;
  virtual void HandleInputWindowButtonRelease(XWindow xid,
                                              int x, int y,
                                              int x_root, int y_root,
                                              int button,
                                              Time timestamp) = 0;
  virtual void HandleInputWindowPointerEnter(XWindow xid,
                                             int x, int y,
                                             int x_root, int y_root,
                                             Time timestamp) = 0;
  virtual void HandleInputWindowPointerLeave(XWindow xid,
                                             int x, int y,
                                             int x_root, int y_root,
                                             Time timestamp) = 0;

  // Handle a button press or pointer enter in a panel.
  virtual void HandlePanelButtonPress(Panel* panel,
                                      int button,
                                      Time timestamp) = 0;
  virtual void HandlePanelTitlebarPointerEnter(Panel* panel,
                                               Time timestamp) = 0;

  // Handle a panel gaining or losing the input focus.
  virtual void HandlePanelFocusChange(Panel* panel, bool focus_in) = 0;

  // Handle a message asking us to expand or collapse one of our panels.
  virtual void HandleSetPanelStateMessage(Panel* panel, bool expand) = 0;

  // Handle a message from Chrome telling us that a panel has been dragged
  // to a particular location.  If false is returned, it indicates that the
  // panel should be removed from this container (i.e. it's been dragged
  // too far away) -- the container's RemovePanel() method will be invoked
  // to accomplish this.
  virtual bool HandleNotifyPanelDraggedMessage(Panel* panel,
                                               int drag_x,
                                               int drag_y) = 0;

  // Handle a message from Chrome telling us that a panel drag is complete.
  virtual void HandleNotifyPanelDragCompleteMessage(Panel* panel) = 0;

  // Handle a message asking us to focus one of our panels.
  virtual void HandleFocusPanelMessage(Panel* panel) = 0;

  // Notification that one of this container's panels has been resized.
  virtual void HandlePanelResize(Panel* panel) = 0;

  // Handle the screen being resized.
  virtual void HandleScreenResize() = 0;

  // Handle a (likely) change to a panel's urgency hint.
  virtual void HandlePanelUrgencyChange(Panel* panel) = 0;

  DISALLOW_COPY_AND_ASSIGN(PanelContainer);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_CONTAINER_H_
