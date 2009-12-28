// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_BAR_H_
#define WINDOW_MANAGER_PANEL_BAR_H_

extern "C" {
#include <X11/Xlib.h>
}
#include <deque>
#include <map>
#include <tr1/memory>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/motion_event_coalescer.h"
#include "window_manager/wm_ipc.h"

typedef ::Window XWindow;

namespace window_manager {

class Panel;
class Shadow;
class Window;
class WindowManager;

// The panel bar positions and controls Chrome panel windows.
class PanelBar : public EventConsumer {
 public:
  PanelBar(WindowManager* wm, int x, int y, int width, int height);
  ~PanelBar();

  WindowManager* wm() { return wm_; }
  bool is_visible() const { return is_visible_; }
  int x() const { return x_; }
  int y() const { return y_; }
  int width() const { return width_; }
  int height() const { return height_; }

  // Note: Begin overridden EventConsumer methods.

  bool IsInputWindow(XWindow xid) {
    return xid == anchor_input_win_ || panel_input_windows_.count(xid);
  }

  // Handle a window's map request.  If it's a panel or titlebar, we move
  // it offscreen, restack it, and map it.
  bool HandleWindowMapRequest(Window* win);

  // Handle the addition of a window.  When a panel window is mapped, its
  // titlebar (which must've previously been mapped) is looked up and a new
  // Panel object is created.  Does nothing when passed non-panel windows.
  void HandleWindowMap(Window* win);

  // Handle the removal of a window by deleting its panel.  The window can
  // be either the panel window itself or its titlebar.  Does nothing when
  // passed windows not in the bar.
  void HandleWindowUnmap(Window* win);

  // Handle a request from a client window to get moved or resized.
  bool HandleWindowConfigureRequest(
      Window* win, int req_x, int req_y, int req_width, int req_height);

  // Handle events for windows.
  bool HandleButtonPress(XWindow xid, int x, int y, int button, Time timestamp);
  bool HandleButtonRelease(
      XWindow xid, int x, int y, int button, Time timestamp);
  bool HandlePointerLeave(XWindow xid, Time timestamp);
  bool HandlePointerMotion(XWindow xid, int x, int y, Time timestamp);

  // Handle messages from client apps.
  bool HandleChromeMessage(const WmIpc::Message& msg);
  bool HandleClientMessage(const XClientMessageEvent& e);

  // Handle windows getting or losing the input focus.
  bool HandleFocusChange(XWindow xid, bool focus_in);

  // Note: End overridden EventConsumer methods.

  void MoveAndResize(int x, int y, int width, int height);

  // Store the position where a panel has been dragged to
  // 'queued_dragged_panel_x_' and 'queued_dragged_panel_y_'.  These
  // positions get applied periodically by MoveDraggedPanel(), which is run
  // by 'dragged_panel_timer_id_' (which gets started indirectly by this
  // method).
  void StorePanelPosition(Window* win, int x, int y);

  // Handle the end of a panel drag.  This stops 'dragged_panel_timer_id_'.
  void HandlePanelDragComplete(Window* win);

  // Move the dragged panel to the queued position.  This is invoked
  // periodically by a timer.
  void MoveDraggedPanel();

  // Take the input focus if possible.  Returns 'false' if it doesn't make
  // sense to take the focus (currently, we only take the focus if there's
  // at least one expanded panel).
  bool TakeFocus();

 private:
  FRIEND_TEST(PanelBarTest, ActiveWindowMessage);

  // Returns true if 'center_x' falls within the bounds of a panel's
  // titlebar.
  class PanelTitlebarContainsPoint {
   public:
    PanelTitlebarContainsPoint(int center_x) : center_x_(center_x) {}
    bool operator()(std::tr1::shared_ptr<Panel>& p);
   private:
    int center_x_;
  };

  // Save some typing.
  typedef std::vector<std::tr1::shared_ptr<Panel> > Panels;

  int num_panels() const {
    return expanded_panels_.size() + collapsed_panels_.size();
  }

  // Do some initial setup for windows that we're going to manage.
  // This includes stacking them and moving them offscreen.
  void DoInitialSetupForWindow(Window* win);

  // Create a panel given mapped content and titlebar windows and add it to
  // the panel bar.
  Panel* CreatePanel(Window* panel_win, Window* titlebar_win, bool expanded);

  // Expand a panel.  We move it from 'collapsed_panels_' to
  // 'expanded_panels_' and reposition the other expanded panels to not
  // overlap.  If 'create_anchor' is true, we additionally create an anchor
  // for it.
  void ExpandPanel(Panel* panel, bool create_anchor);

  // Collapse a panel.  We move it from 'expanded_panels_' to
  // 'collapsed_panels_' and pack all of the collapsed panels to the right.
  void CollapsePanel(Panel* panel);

  // Focus the passed-in panel's panel window.  Also removes its passive
  // button grab and updates 'desired_panel_to_focus_'.  If
  // 'remove_pointer_grab' is true, removes the active pointer grab and
  // replays any grabbed events (this is used when the panel is being
  // focused in response to a grabbed click).
  void FocusPanel(Panel* panel, bool remove_pointer_grab);

  // Get the panel with the passed-in panel or titlebar window.
  // Returns NULL for unknown windows.
  Panel* GetPanelByWindow(const Window& win);

  // Get an iterator to the panel containing 'win' (either a panel or
  // titlebar window) from the passed-in vector.  Returns panels.end() if
  // the panel isn't present.
  static Panels::iterator FindPanelInVectorByWindow(
      Panels& panels, const Window& win);

  // Get a panel's index in a vector, or -1 if it's not present.
  static int GetPanelIndex(Panels& panels, const Panel& panel);

  // Start a drag of the passed-in panel.  If there's a previous drag
  // (which there shouldn't be; we should've already gotten notification of
  // it ending), we abort it.  The panel's windows are restacked and we
  // start the MoveDraggedPanel() timer if necessary.
  void StartDrag(Panel* panel);

  // Pack all collapsed panels with the exception of 'dragged_panel_'
  // towards the right.  We reserve space for 'dragged_panel_' and update
  // its snapped position, but we don't update its actual position.
  void PackCollapsedPanels();

  // Reposition all expanded panels other than 'fixed_panel'.  We attempt
  // to make them fit onscreen and not overlap.
  void RepositionExpandedPanels(Panel* fixed_panel);

  // Insert a panel into 'collapsed_panels_' in the correct position (in
  // terms of left-to-right positioning).
  void InsertCollapsedPanel(std::tr1::shared_ptr<Panel> new_panel);

  // Insert a panel into 'expanded_panels_' in the correct position (in
  // terms of left-to-right positioning).
  void InsertExpandedPanel(std::tr1::shared_ptr<Panel> new_panel);

  // Create an anchor for a panel.  If there's a previous anchor, we
  // destroy it.
  void CreateAnchor(Panel* panel);

  // Destroy the anchor.
  void DestroyAnchor();

  // Get the expanded panel closest to 'panel', or NULL if there are no
  // other expanded panels (or if 'panel' isn't expanded).
  Panel* GetNearestExpandedPanel(Panel* panel);

  // Show or hide the panel bar.  Note that this doesn't change the
  // visibility or position of the panels themselves -- this is just called
  // internally to show the bar when the first panel is created and hide it
  // when no panels are present.
  void SetVisibility(bool visible);

  WindowManager* wm_;  // not owned

  // Position and size of the bar.
  int x_;
  int y_;
  int width_;
  int height_;

  // Width of the contents of the bar.
  int collapsed_panel_width_;

  // Collapsed and expanded panels.
  Panels collapsed_panels_;
  Panels expanded_panels_;

  // Actor drawn for the bar's background.
  scoped_ptr<ClutterInterface::Actor> bar_actor_;

  // Drop shadow underneath the bar.
  scoped_ptr<Shadow> bar_shadow_;

  // The panel that's currently being dragged, or NULL if none is.
  Panel* dragged_panel_;

  // Batches motion events for dragged panels so that we can rate-limit the
  // frequency of their processing.
  MotionEventCoalescer dragged_panel_event_coalescer_;

  // Input window used to receive events for the anchor displayed under
  // panels after they're expanded.
  XWindow anchor_input_win_;

  // Panel for which the anchor is currently being displayed.
  Panel* anchor_panel_;

  // Textured actor used to draw the anchor.
  scoped_ptr<ClutterInterface::Actor> anchor_actor_;

  // If we need to give the focus to a panel, we choose this one.
  Panel* desired_panel_to_focus_;

  std::map<XWindow, Panel*> panel_input_windows_;

  // Is the panel bar visible?
  bool is_visible_;

  // Have we already seen a MapRequest event?
  bool saw_map_request_;

  DISALLOW_COPY_AND_ASSIGN(PanelBar);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_BAR_H_
