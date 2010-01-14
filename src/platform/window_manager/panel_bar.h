// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_PANEL_BAR_H_
#define WINDOW_MANAGER_PANEL_BAR_H_

extern "C" {
#include <X11/Xlib.h>
}
#include <map>
#include <tr1/memory>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/panel_container.h"
#include "window_manager/wm_ipc.h"

typedef ::Window XWindow;

namespace window_manager {

class Panel;
class Shadow;
class Window;
class WindowManager;

// The panel bar handles panels that are pinned to the bottom of the
// screen.
class PanelBar : public PanelContainer {
 public:
  PanelBar(WindowManager* wm, int x, int y, int width, int height);
  ~PanelBar();

  WindowManager* wm() { return wm_; }
  bool is_visible() const { return is_visible_; }
  int x() const { return x_; }
  int y() const { return y_; }
  int width() const { return width_; }
  int height() const { return height_; }

  // Begin overridden PanelContainer methods.
  void GetInputWindows(std::vector<XWindow>* windows_out);
  void AddPanel(Panel* panel, PanelSource source, bool expanded);
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
  void HandleInputWindowPointerLeave(XWindow xid, Time timestamp);
  void HandlePanelButtonPress(Panel* panel, int button, Time timestamp);
  void HandlePanelFocusChange(Panel* panel, bool focus_in);
  void HandleSetPanelStateMessage(Panel* panel, bool expand);
  bool HandleNotifyPanelDraggedMessage(Panel* panel, int drag_x, int drag_y);
  void HandleNotifyPanelDragCompleteMessage(Panel* panel);
  void HandleFocusPanelMessage(Panel* panel);
  // End overridden PanelContainer methods.

  // Move and resize the panel bar, and adjust the positions of all of its
  // panels accordingly.
  void MoveAndResize(int x, int y, int width, int height);

  // Take the input focus if possible.  Returns 'false' if it doesn't make
  // sense to take the focus (currently, we only take the focus if there's
  // at least one expanded panel).
  bool TakeFocus();

 private:
  FRIEND_TEST(PanelBarTest, ActiveWindowMessage);
  FRIEND_TEST(PanelBarTest, FocusNewPanel);

  // PanelBar-specific information about a panel.
  struct PanelInfo {
    PanelInfo() : is_expanded(false), snapped_right(0) {}

    // Is the panel currently expanded?
    bool is_expanded;

    // X position of the right edge of where the titlebar wants to be when
    // collapsed.  For collapsed panels that are being dragged, this may be
    // different from the actual composited position -- we only snap the
    // panels to this position when the drag is complete.
    int snapped_right;
  };

  // Save some typing.
  typedef std::vector<Panel*> Panels;

  int num_panels() const {
    return expanded_panels_.size() + collapsed_panels_.size();
  }

  // Get the PanelInfo object for a panel, crashing if it's not present.
  PanelInfo* GetPanelInfoOrDie(Panel* panel);

  // Expand a panel.  We move it from 'collapsed_panels_' to
  // 'expanded_panels_' and reposition the other expanded panels to not
  // overlap.  If 'create_anchor' is true, we additionally create an anchor
  // for it.
  void ExpandPanel(Panel* panel,
                   bool create_anchor,
                   bool reposition_other_panels,
                   int anim_ms);

  // Collapse a panel.  We move it from 'expanded_panels_' to
  // 'collapsed_panels_' and pack all of the collapsed panels to the right.
  void CollapsePanel(Panel* panel);

  // Configure a panel that's being collapsed.  This includes stacking it,
  // resizing its titlebar, moving it to its final position, making it
  // non-resizable, notifying Chrome, updating its PanelInfo object, etc.
  // The caller is still responsible for getting the panel into
  // 'collapsed_panels_'.  Helper method used by both CollapsePanel() and
  // AddPanel().
  void ConfigureCollapsedPanel(Panel* panel);

  // Focus the passed-in panel's content window.  Also removes its passive
  // button grab and updates 'desired_panel_to_focus_'.  If
  // 'remove_pointer_grab' is true, removes the active pointer grab and
  // replays any grabbed events (this is used when the panel is being
  // focused in response to a grabbed click).
  void FocusPanel(Panel* panel, bool remove_pointer_grab);

  // Get the panel with the passed-in content or titlebar window.
  // Returns NULL for unknown windows.
  Panel* GetPanelByWindow(const Window& win);

  // Get an iterator to the panel containing 'win' (either a content or
  // titlebar window) from the passed-in vector.  Returns panels.end() if
  // the panel isn't present.
  static Panels::iterator FindPanelInVectorByWindow(
      Panels& panels, const Window& win);

  // Get a panel's index in a vector, or -1 if it's not present.
  static int GetPanelIndex(Panels& panels, const Panel& panel);

  // Handle the end of a panel drag.
  void HandlePanelDragComplete(Panel* panel);

  // Pack all collapsed panels with the exception of 'dragged_panel_'
  // towards the right.  We reserve space for 'dragged_panel_' and update
  // its snapped position, but we don't update its actual position.
  void PackCollapsedPanels();

  // Reposition all expanded panels other than 'fixed_panel'.  We attempt
  // to make them fit onscreen and not overlap.
  void RepositionExpandedPanels(Panel* fixed_panel);

  // Insert a panel into 'collapsed_panels_' in the correct position (in
  // terms of left-to-right positioning).
  void InsertCollapsedPanel(Panel* new_panel);

  // Insert a panel into 'expanded_panels_' in the correct position (in
  // terms of left-to-right positioning).
  void InsertExpandedPanel(Panel* new_panel);

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

  // Move the passed-in expanded panel onscreen if it isn't already.
  void MoveExpandedPanelOnscreen(Panel* panel, int anim_ms);

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

  // Information about panels in 'collapsed_panels_' and 'expanded_panels_'
  // that doesn't belong in the Panel class itself.
  std::map<Panel*, std::tr1::shared_ptr<PanelInfo> > panel_infos_;

  // Actor drawn for the bar's background.
  scoped_ptr<ClutterInterface::Actor> bar_actor_;

  // Drop shadow underneath the bar.
  scoped_ptr<Shadow> bar_shadow_;

  // The panel that's currently being dragged, or NULL if none is.
  Panel* dragged_panel_;

  // Input window used to receive events for the anchor displayed under
  // panels after they're expanded.
  XWindow anchor_input_xid_;

  // Panel for which the anchor is currently being displayed.
  Panel* anchor_panel_;

  // Textured actor used to draw the anchor.
  scoped_ptr<ClutterInterface::Actor> anchor_actor_;

  // If we need to give the focus to a panel, we choose this one.
  Panel* desired_panel_to_focus_;

  std::map<XWindow, Panel*> panel_input_windows_;

  // Is the panel bar visible?
  bool is_visible_;

  DISALLOW_COPY_AND_ASSIGN(PanelBar);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_BAR_H_
