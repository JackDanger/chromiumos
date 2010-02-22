// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_HOTKEY_OVERLAY_H_
#define WINDOW_MANAGER_HOTKEY_OVERLAY_H_

#include <map>
#include <string>
#include <tr1/memory>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/x_types.h"

namespace window_manager {

class ClutterInterface;
class XConnection;

// Display overlay images to help users learn keyboard shortcuts.
// The actor returned by group() should be added to the stage and moved to
// its center; the group's visibility and opacity can be manipulated
// directly.
class HotkeyOverlay {
 public:
  HotkeyOverlay(XConnection* xconn, ClutterInterface* clutter);
  ~HotkeyOverlay();

  ClutterInterface::Actor* group() { return group_.get(); }

  // Called when key mappings change to update internal state.
  void RefreshKeyMappings();

  // Update the overlay in response to XConnection::QueryKeyboardState()'s
  // output.
  void HandleKeyboardState(const std::vector<uint8_t>& keycodes);

 private:
  // Helper method to choose the correct image based on the current key
  // state.
  void UpdateImage();

  // Show the image located at 'filename'.
  void ShowImage(const std::string& filename);

  // Hide the current image.
  void HideCurrentImage();

  XConnection* xconn_;  // not owned

  ClutterInterface* clutter_;  // not owned

  scoped_ptr<ClutterInterface::ContainerActor> group_;

  // Map filenames to image actors.
  std::map<std::string, std::tr1::shared_ptr<ClutterInterface::Actor> > images_;

  // The currently-shown image, or NULL if no image is currently shown.
  // Points at a value in 'images_'.
  ClutterInterface::Actor* current_image_;

  // X11 keycodes corresponding to various keysyms.
  KeyCode left_ctrl_keycode_;
  KeyCode right_ctrl_keycode_;
  KeyCode left_alt_keycode_;
  KeyCode right_alt_keycode_;
  KeyCode left_shift_keycode_;
  KeyCode right_shift_keycode_;

  // The state of various keys.
  bool left_ctrl_pressed_;
  bool right_ctrl_pressed_;
  bool left_alt_pressed_;
  bool right_alt_pressed_;
  bool left_shift_pressed_;
  bool right_shift_pressed_;

  DISALLOW_COPY_AND_ASSIGN(HotkeyOverlay);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_HOTKEY_OVERLAY_H_
