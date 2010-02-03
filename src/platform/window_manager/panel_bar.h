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
#include "window_manager/panel_container.h"

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
  PanelBar(WindowManager* wm);
  ~PanelBar();

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
  void HandleScreenResize();
  // End overridden PanelContainer methods.

  // Take the input focus if possible.  Returns 'false' if it doesn't make
  // sense to take the focus (currently, we only take the focus if there's
  // at least one expanded panel).
  bool TakeFocus();

  // Amount of horizontal padding to place between panels, in pixels.
  static const int kPixelsBetweenPanels;

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

  // Get the PanelInfo object for a panel, crashing if it's not present.
  PanelInfo* GetPanelInfoOrDie(Panel* panel);

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

  WindowManager* wm_;  // not owned

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

  // If we need to give the focus to a panel, we choose this one.
  Panel* desired_panel_to_focus_;

  DISALLOW_COPY_AND_ASSIGN(PanelBar);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_PANEL_BAR_H_
