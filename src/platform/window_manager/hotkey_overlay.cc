// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/hotkey_overlay.h"

#include <gflags/gflags.h>

#include "window_manager/clutter_interface.h"

DEFINE_string(hotkey_overlay_image_dir, "../assets/images",
              "Path to directory containing hotkey overlay images");

namespace chromeos {

// Time to spend in image transitions, in milliseconds.
// TODO: It'd be nice to do a fade here, but fading from one image to
// another looks terrible because of the way that alpha compositing is
// currently working.
static const int kAnimMs = 0;

HotkeyOverlay::HotkeyOverlay(ClutterInterface* clutter)
    : clutter_(clutter),
      group_(clutter_->CreateGroup()),
      current_image_(NULL),
      left_ctrl_pressed_(false),
      right_ctrl_pressed_(false),
      left_alt_pressed_(false),
      right_alt_pressed_(false),
      left_shift_pressed_(false),
      right_shift_pressed_(false) {
}

HotkeyOverlay::~HotkeyOverlay() {
  clutter_ = NULL;
}

void HotkeyOverlay::HandleKeyPress(KeySym keysym) {
  switch (keysym) {
    case XK_Control_L: left_ctrl_pressed_ = true; break;
    case XK_Control_R: right_ctrl_pressed_ = true; break;
    case XK_Alt_L: left_alt_pressed_ = true; break;
    case XK_Alt_R: right_alt_pressed_ = true; break;
    case XK_Shift_L: left_shift_pressed_ = true; break;
    case XK_Shift_R: right_shift_pressed_ = true; break;
    default: return;
  }
  UpdateImage();
}

void HotkeyOverlay::HandleKeyRelease(KeySym keysym) {
  switch (keysym) {
    case XK_Control_L: left_ctrl_pressed_ = false; break;
    case XK_Control_R: right_ctrl_pressed_ = false; break;
    case XK_Alt_L: left_alt_pressed_ = false; break;
    case XK_Alt_R: right_alt_pressed_ = false; break;
    case XK_Shift_L: left_shift_pressed_ = false; break;
    case XK_Shift_R: right_shift_pressed_ = false; break;
    default: return;
  }
  UpdateImage();
}

void HotkeyOverlay::Reset() {
  left_ctrl_pressed_ = false;
  right_ctrl_pressed_ = false;
  left_alt_pressed_ = false;
  right_alt_pressed_ = false;
  left_shift_pressed_ = false;
  right_shift_pressed_ = false;
  UpdateImage();
}

void HotkeyOverlay::UpdateImage() {
  bool ctrl_pressed = (left_ctrl_pressed_ || right_ctrl_pressed_);
  bool alt_pressed = (left_alt_pressed_ || right_alt_pressed_);
  bool shift_pressed = (left_shift_pressed_ || right_shift_pressed_);

  if (ctrl_pressed) {
    if (alt_pressed) {
      if (shift_pressed) {
        ShowImage(FLAGS_hotkey_overlay_image_dir +
                  "/hotkeys_ctrl_alt_shift.png");
      } else {
        ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_ctrl_alt.png");
      }
    } else if (shift_pressed) {
      ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_ctrl_shift.png");
    } else {
      ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_ctrl.png");
    }
  } else if (alt_pressed) {
    if (shift_pressed) {
      ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_alt_shift.png");
    } else {
      ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_alt.png");
    }
  } else if (shift_pressed) {
    ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_shift.png");
  } else {
    // TODO: Handle the "Search" key.
    ShowImage(FLAGS_hotkey_overlay_image_dir + "/hotkeys_base.png");
  }
}

void HotkeyOverlay::ShowImage(const std::string& filename) {
  std::map<std::string,
           std::tr1::shared_ptr<ClutterInterface::Actor> >::iterator
      it = images_.find(filename);
  if (it == images_.end()) {
    std::tr1::shared_ptr<ClutterInterface::Actor>
        image(clutter_->CreateImage(filename));
    image->SetOpacity(0, 0);
    image->SetVisibility(true);
    group_->AddActor(image.get());
    image->Move(-1 * image->GetWidth() / 2, -1 * image->GetHeight() / 2, 0);
    it = images_.insert(make_pair(filename, image)).first;
  }

  if (current_image_ != it->second.get()) {
    HideCurrentImage();
    current_image_ = it->second.get();
  }
  current_image_->SetOpacity(1, kAnimMs);
}

void HotkeyOverlay::HideCurrentImage() {
  if (!current_image_)
    return;

  current_image_->SetOpacity(0, kAnimMs);
  current_image_ = NULL;
}

}  // namespace chromeos
