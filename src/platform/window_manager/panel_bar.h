// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_BAR_H_
#define WINDOW_MANAGER_PANEL_BAR_H_

extern "C" {
#include <X11/Xlib.h>
}
#include <map>
#include <tr1/memory>

#include <glib.h>  // for gboolean and gint
#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/panel_container.h"

typedef ::Window XWindow;

namespace window_manager {

class EventConsumerRegistrar;
class Panel;
class PanelManager;
class PointerPositionWatcher;
class Shadow;
class Window;
class WindowManager;

// The panel bar handles panels that are pinned to the bottom of the
// screen.
class PanelBar : public PanelContainer {
 public:
  PanelBar(PanelManager* panel_manager);
  ~PanelBar();

  WindowManager* wm();

  // Begin PanelContainer implementation.
  void GetInputWindows(std::vector<XWindow>* windows_out);
  void AddPanel(Panel* panel, PanelSource source);
  void RemovePanel(Panel* panel);
  bool ShouldAddDraggedPanel(const Panel* panel, int drag_x, int drag_y);
  void HandleInputWindowButtonPress(XWindow xid,
                                    int x, int y,
                                    int x_root, int y_root,
                                    int button,
                                    Time timestamp);
  void HandleInputWindowButtonRelease(XWindow xid,
                                      int x, int y,
                                      int x_root, int y_root,
                                      int button,
                                      Time timestamp) {}
  void HandleInputWindowPointerEnter(XWindow xid,
                                     int x, int y,
                                     int x_root, int y_root,
                                     Time timestamp);
  void HandleInputWindowPointerLeave(XWindow xid,
                                     int x, int y,
                                     int x_root, int y_root,
                                     Time timestamp);
  void HandlePanelButtonPress(Panel* panel, int button, Time timestamp);
  void HandlePanelTitlebarPointerEnter(Panel* panel, Time timestamp);
  void HandlePanelFocusChange(Panel* panel, bool focus_in);
  void HandleSetPanelStateMessage(Panel* panel, bool expand);
  bool HandleNotifyPanelDraggedMessage(Panel* panel, int drag_x, int drag_y);
  void HandleNotifyPanelDragCompleteMessage(Panel* panel);
  void HandleFocusPanelMessage(Panel* panel);
  void HandlePanelResize(Panel* panel);
  void HandleScreenResize();
  void HandlePanelUrgencyChange(Panel* panel);
  // End PanelContainer implementation.

  // Take the input focus if possible.  Returns 'false' if it doesn't make
  // sense to take the focus (currently, we only take the focus if there's
  // at least one expanded panel).
  bool TakeFocus();

  // Amount of horizontal padding to place between panels, in pixels.
  static const int kPixelsBetweenPanels;

  // How close does the pointer need to get to the bottom of the screen
  // before we show hidden collapsed panels?
  static const int kShowCollapsedPanelsDistancePixels;

  // How far away from the bottom of the screen can the pointer get before
  // we hide collapsed panels?
  static const int kHideCollapsedPanelsDistancePixels;

  // How much of the top of a collapsed panel's titlebar should peek up
  // from the bottom of the screen when it is hidden?
  static const int kHiddenCollapsedPanelHeightPixels;

 private:
  friend class BasicWindowManagerTest;
  FRIEND_TEST(PanelBarTest, FocusNewPanel);
  FRIEND_TEST(PanelBarTest, HideCollapsedPanels);
  FRIEND_TEST(PanelBarTest, DeferHidingDraggedCollapsedPanel);

  // PanelBar-specific information about a panel.
  struct PanelInfo {
    PanelInfo() : snapped_right(0), is_urgent(false) {}

    // X position of the right edge of where the titlebar wants to be when
    // collapsed.  For collapsed panels that are being dragged, this may be
    // different from the actual composited position -- we only snap the
    // panels to this position when the drag is complete.
    int snapped_right;

    // Was the content window's urgency hint set the last time that we
    // looked at it?  If so, we avoid hiding the panel offscreen when it's
    // collapsed.
    bool is_urgent;
  };

  typedef std::vector<Panel*> Panels;

  // Is 'collapsed_panel_state_' such that collapsed panels are currently
  // hidden offscreen?
  bool CollapsedPanelsAreHidden() const {
    return collapsed_panel_state_ == COLLAPSED_PANEL_STATE_HIDDEN ||
           collapsed_panel_state_ == COLLAPSED_PANEL_STATE_WAITING_TO_SHOW;
  }

  // Get the PanelInfo object for a panel, crashing if it's not present.
  PanelInfo* GetPanelInfoOrDie(Panel* panel);

  // Get the current number of collapsed panels.
  int GetNumCollapsedPanels();

  // Compute the Y-position where the top of the passed-in panel
  // should be placed (depending on whether it's expanded or collapsed,
  // whether collapsed panels are currently hidden, whether the panel's
  // urgent flag is set, etc.).
  int ComputePanelY(const Panel& panel, const PanelInfo& info);

  // Expand a panel.  If 'create_anchor' is true, we additionally create an
  // anchor for it.
  void ExpandPanel(Panel* panel, bool create_anchor, int anim_ms);

  // Collapse a panel.
  void CollapsePanel(Panel* panel);

  // Focus the passed-in panel's content window.  Also removes its passive
  // button grab and updates 'desired_panel_to_focus_'.  If
  // 'remove_pointer_grab' is true, removes the active pointer grab and
  // replays any grabbed events (this is used when the panel is being
  // focused in response to a grabbed click).
  void FocusPanel(Panel* panel, bool remove_pointer_grab, Time timestamp);

  // Get the panel with the passed-in content or titlebar window.
  // Returns NULL for unknown windows.
  Panel* GetPanelByWindow(const Window& win);

  // Get an iterator to the panel containing 'win' (either a content or
  // titlebar window) from the passed-in vector.  Returns panels.end() if
  // the panel isn't present.
  static Panels::iterator FindPanelInVectorByWindow(
      Panels& panels, const Window& win);

  // Handle the end of a panel drag.
  void HandlePanelDragComplete(Panel* panel);

  // Update the position of 'fixed_panel' within 'panels_' based on its
  // current position and repack the other panels if necessary.
  void ReorderPanel(Panel* fixed_panel);

  // Pack all collapsed panels with the exception of 'fixed_panel' (if
  // non-NULL) towards the right.  We reserve space for 'fixed_panel' and
  // update its snapped position, but we don't update its actual position.
  void PackPanels(Panel* fixed_panel);

  // Create an anchor for a panel.  If there's a previous anchor, we
  // destroy it.
  void CreateAnchor(Panel* panel);

  // Destroy the anchor.
  void DestroyAnchor();

  // Get the expanded panel closest to 'panel', or NULL if there are no
  // other expanded panels (or if 'panel' isn't expanded).
  Panel* GetNearestExpandedPanel(Panel* panel);

  // Move 'show_collapsed_panels_input_xid_' onscreen or offscreen.
  void ConfigureShowCollapsedPanelsInputWindow(bool move_onscreen);

  // Initialize 'hide_collapsed_panels_pointer_watcher_' to call
  // HideCollapsedPanels() as soon as it sees the pointer move too far away
  // from the bottom of the screen.
  void StartHideCollapsedPanelsWatcher();

  // Show collapsed panels' full titlebars at the bottom of the screen.
  void ShowCollapsedPanels();

  // Hide everything but the very top of collapsed panels' titlebars.  If a
  // collapsed panel is being dragged, defers hiding them and sets
  // 'collapsed_panel_state_' to COLLAPSED_PANEL_STATE_WAITING_TO_HIDE
  // instead.
  void HideCollapsedPanels();

  // If 'show_collapsed_panels_timer_id_' is set, disable the timer and
  // clear the variable.
  void DisableShowCollapsedPanelsTimer();

  // Stop 'show_collapsed_panels_timer_id_' and invoke ShowCollapsedPanels().
  static gboolean HandleShowCollapsedPanelsTimerThunk(gpointer self) {
    return reinterpret_cast<PanelBar*>(self)->HandleShowCollapsedPanelsTimer();
  }
  gboolean HandleShowCollapsedPanelsTimer();

  PanelManager* panel_manager_;  // not owned

  // Total width of all panels (including padding between them).
  int total_panel_width_;

  // Panels, in left-to-right order.
  Panels panels_;

  // Information about our panels that doesn't belong in the Panel class
  // itself.
  std::map<Panel*, std::tr1::shared_ptr<PanelInfo> > panel_infos_;

  // The panel that's currently being dragged, or NULL if none is.
  Panel* dragged_panel_;

  // Input window used to receive events for the anchor displayed under
  // panels after they're expanded.
  XWindow anchor_input_xid_;

  // Panel for which the anchor is currently being displayed.
  Panel* anchor_panel_;

  // Textured actor used to draw the anchor.
  scoped_ptr<ClutterInterface::Actor> anchor_actor_;

  // Watches the pointer's position so we know when to destroy the anchor.
  scoped_ptr<PointerPositionWatcher> anchor_pointer_watcher_;

  // If we need to give the focus to a panel, we choose this one.
  Panel* desired_panel_to_focus_;

  // Different states that we can be in with regard to showing collapsed
  // panels at the bottom of the screen.
  enum CollapsedPanelState {
    // Showing the panels' full titlebars.
    COLLAPSED_PANEL_STATE_SHOWN = 0,

    // Just showing the tops of the titlebars.
    COLLAPSED_PANEL_STATE_HIDDEN,

    // Hiding the titlebars, but we'll show them after
    // 'show_collapsed_panels_timer_id_' fires.
    COLLAPSED_PANEL_STATE_WAITING_TO_SHOW,

    // Showing the titlebars, but the pointer has moved up from the bottom
    // of the screen while dragging a collapsed panel and we'll hide the
    // collapsed panels as soon as the drag finishes.
    COLLAPSED_PANEL_STATE_WAITING_TO_HIDE,
  };
  CollapsedPanelState collapsed_panel_state_;

  // Input window used to detect when the mouse is at the bottom of the
  // screen so that we can show collapsed panels.
  XWindow show_collapsed_panels_input_xid_;

  // ID of a timer that we use to delay calling ShowCollapsedPanels() after
  // the pointer enters 'show_collapsed_panels_input_xid_', or 0 if unset.
  gint show_collapsed_panels_timer_id_;

  // Used to monitor the pointer position when we're showing collapsed
  // panels so that we'll know to hide them when the pointer far enough
  // away.
  scoped_ptr<PointerPositionWatcher> hide_collapsed_panels_pointer_watcher_;

  // PanelManager event registrations related to the panel bar's input
  // windows.
  scoped_ptr<EventConsumerRegistrar> event_consumer_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PanelBar);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_BAR_H_
