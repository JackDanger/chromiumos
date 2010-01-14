// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_MANAGER_H_
#define WINDOW_MANAGER_PANEL_MANAGER_H_

#include <map>
#include <tr1/memory>
#include <vector>

extern "C" {
#include <X11/Xlib.h>
}

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/event_consumer.h"
#include "window_manager/panel_container.h"  // for PanelSource enum
#include "window_manager/util.h"

typedef ::Window XWindow;

namespace window_manager {

class MotionEventCoalescer;
class Panel;
class PanelBar;
class WindowManager;

// Handles map/unmap events for panel windows, owns Panel and
// PanelContainer objects, adds new panels to the appropriate container,
// routes X events to panels and containers, coordinates drags of panels
// between containers, etc.
class PanelManager : public EventConsumer {
 public:
  PanelManager(WindowManager* wm, int panel_bar_height);
  ~PanelManager();

  WindowManager* wm() { return wm_; }

  // Note: Begin overridden EventConsumer methods.

  // Checks whether the passed-in window is an input window belonging to
  // one of our Panels or PanelContainers.
  bool IsInputWindow(XWindow xid);

  // Handle a window's map request.  If it's a panel content or titlebar
  // window, move it offscreen, map it, and return true.
  bool HandleWindowMapRequest(Window* win);

  // Handle a window being mapped.  When a content window is mapped, its
  // titlebar (which must've previously been mapped) is looked up and a new
  // Panel object is created and added to a container.  Does nothing when
  // passed non-content windows.
  void HandleWindowMap(Window* win);

  // Handle the removal of a window by removing its panel from its
  // container and destroying the Panel object.  The window can be either
  // the panel's content window or its titlebar.  Does nothing when passed
  // non-panel windows.
  void HandleWindowUnmap(Window* win);

  // Handle a request from a client window to be moved or resized.
  bool HandleWindowConfigureRequest(
      Window* win, int req_x, int req_y, int req_width, int req_height);

  // Handle events for windows.  If the event occurred in an input window,
  // it is passed through to the Panel or PanelContainer that owns the
  // input window.  If a button press occurs in a panel's content or
  // titlebar window, it just passed directly to the PanelContainer that
  // currently contains the panel via
  // PanelContainer::HandlePanelButtonPress().
  bool HandleButtonPress(XWindow xid,
                         int x, int y,
                         int x_root, int y_root,
                         int button,
                         Time timestamp);
  bool HandleButtonRelease(XWindow xid,
                           int x, int y,
                           int x_root, int y_root,
                           int button,
                           Time timestamp);
  bool HandlePointerLeave(XWindow xid, Time timestamp);
  bool HandlePointerMotion(XWindow xid, int x, int y, Time timestamp);

  // Handle messages from client apps.
  bool HandleChromeMessage(const WmIpc::Message& msg);
  bool HandleClientMessage(const XClientMessageEvent& e);

  // Handle windows gaining or losing the input focus.
  bool HandleFocusChange(XWindow xid, bool focus_in);

  // Note: End overridden EventConsumer methods.

  bool IsPanelBarVisible() const;

  // Handle the screen being resized.
  void HandleScreenResize();

  // Take the input focus if possible.  Returns 'false' if it doesn't make
  // sense to take the focus (currently, we only take the focus if there's
  // at least one expanded panel).
  bool TakeFocus();

 private:
  friend class PanelTest;                 // uses 'panel_bar_'
  friend class PanelBarTest;              // uses 'panel_bar_'
  friend class PanelManagerTest;
  FRIEND_TEST(PanelManagerTest, AttachAndDetach);
  FRIEND_TEST(PanelManagerTest, DragFocusedPanel);
  FRIEND_TEST(WindowManagerTest, RandR);  // uses 'panel_bar_'

  // Get the panel with the passed-in content or titlebar window.
  // Returns NULL for unknown windows.
  Panel* GetPanelByXid(XWindow xid);
  Panel* GetPanelByWindow(const Window& win);

  // Get the container for the passed-in panel.  Returns NULL if the panel
  // isn't currently held by a container.
  PanelContainer* GetContainerForPanel(const Panel& panel) {
    return FindWithDefault(containers_by_panel_,
                           static_cast<const Panel*>(&panel),
                           static_cast<PanelContainer*>(NULL));
  }

  // Register a container's input windows in 'container_input_xids_' and
  // append a pointer to the container to 'containers_'.
  void RegisterContainer(PanelContainer* container);

  // Do some initial setup for windows that we're going to manage.
  // This includes moving them offscreen.
  void DoInitialSetupForWindow(Window* win);

  // Handle coalesced motion events while a panel is being dragged.
  // Invoked by 'dragged_panel_event_coalescer_'.
  void HandlePeriodicPanelDragMotion();

  // Handle a panel drag being completed.  If 'removed' is true, then the
  // panel is in the process of being destroyed, so we don't bother doing
  // things like notifying its container, adding it to a container if it
  // isn't already in one, etc.
  void HandlePanelDragComplete(Panel* panel, bool removed);

  // Helper method.  Calls the container's AddPanel() method with the
  // passed in 'panel', 'source', and 'expanded' parameters and updates
  // 'containers_by_panel_'.
  void AddPanelToContainer(Panel* panel,
                           PanelContainer* container,
                           PanelContainer::PanelSource source,
                           bool expanded);

  // Helper method.  Calls the container's RemovePanel() method, updates
  // 'containers_by_panel_', and removes the panel's button grab (in case
  // the container had installed one).
  void RemovePanelFromContainer(Panel* panel, PanelContainer* container);

  WindowManager* wm_;  // not owned

  // Map from a panel's content window's XID to the Panel object itself.
  std::map<XWindow, std::tr1::shared_ptr<Panel> > panels_;

  // Map from a panel's titlebar window's XID to a pointer to the panel.
  std::map<XWindow, Panel*> panels_by_titlebar_xid_;

  // The panel that's currently being dragged.
  Panel* dragged_panel_;

  // Batches motion events for dragged panels so that we can rate-limit the
  // frequency of their processing.
  scoped_ptr<MotionEventCoalescer> dragged_panel_event_coalescer_;

  // Input windows belonging to panel containers and to panels themselves.
  std::map<XWindow, PanelContainer*> container_input_xids_;
  std::map<XWindow, Panel*> panel_input_xids_;

  std::vector<PanelContainer*> containers_;

  std::map<const Panel*, PanelContainer*> containers_by_panel_;

  scoped_ptr<PanelBar> panel_bar_;

  // Have we already seen a MapRequest event?
  bool saw_map_request_;

  DISALLOW_COPY_AND_ASSIGN(PanelManager);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_MANAGER_H_
