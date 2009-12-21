// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_HOTKEY_OVERLAY_H_
#define WINDOW_MANAGER_HOTKEY_OVERLAY_H_

#include <map>
#include <string>
#include <tr1/memory>

extern "C" {
#include <X11/Xlib.h>
}

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"

namespace window_manager {

class ClutterInterface;
class XConnection;

// Display overlay images to help users learn keyboard shortcuts.
// The actor returned by group() should be added to the stage and moved to
// its center; the group's visibility and opacity can be manipulated
// directly.
class HotkeyOverlay {
 public:
  HotkeyOverlay(ClutterInterface* clutter);
  ~HotkeyOverlay();

  ClutterInterface::Actor* group() { return group_.get(); }

  // Handle keys being pressed and released.
  void HandleKeyPress(KeySym keysym);
  void HandleKeyRelease(KeySym keysym);

  // Reset key state and switch to the base image.
  // Useful to call when the overlay is first being shown and we don't know
  // whether some keys may already be held down.
  void Reset();

 private:
  // Helper method to choose the correct image based on the current key
  // state.
  void UpdateImage();

  // Show the image located at 'filename'.
  void ShowImage(const std::string& filename);

  // Hide the current image.
  void HideCurrentImage();

  ClutterInterface* clutter_;  // not owned

  scoped_ptr<ClutterInterface::ContainerActor> group_;

  // Map filenames to image actors.
  std::map<std::string, std::tr1::shared_ptr<ClutterInterface::Actor> > images_;

  // The currently-shown image, or NULL if no image is currently shown.
  // Points at a value in 'images_'.
  ClutterInterface::Actor* current_image_;

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
