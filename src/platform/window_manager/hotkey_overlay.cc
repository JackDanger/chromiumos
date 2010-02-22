// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/hotkey_overlay.h"

extern "C" {
#include <X11/keysym.h>
}
#include <gflags/gflags.h>

#include "window_manager/clutter_interface.h"
#include "window_manager/x_connection.h"

DEFINE_string(hotkey_overlay_image_dir, "../assets/images",
              "Path to directory containing hotkey overlay images");

namespace window_manager {

// Time to spend in image transitions, in milliseconds.
// TODO: It'd be nice to do a fade here, but fading from one image to
// another looks terrible because of the way that alpha compositing is
// currently working.
static const int kAnimMs = 0;

HotkeyOverlay::HotkeyOverlay(XConnection* xconn, ClutterInterface* clutter)
    : xconn_(xconn),
      clutter_(clutter),
      group_(clutter_->CreateGroup()),
      current_image_(NULL),
      left_ctrl_keycode_(0),
      right_ctrl_keycode_(0),
      left_alt_keycode_(0),
      right_alt_keycode_(0),
      left_shift_keycode_(0),
      right_shift_keycode_(0),
      left_ctrl_pressed_(false),
      right_ctrl_pressed_(false),
      left_alt_pressed_(false),
      right_alt_pressed_(false),
      left_shift_pressed_(false),
      right_shift_pressed_(false) {
  group_->SetName("hotkey overlay group");
  RefreshKeyMappings();
}

HotkeyOverlay::~HotkeyOverlay() {
  clutter_ = NULL;
}

void HotkeyOverlay::RefreshKeyMappings() {
  left_ctrl_keycode_ = xconn_->GetKeyCodeFromKeySym(XK_Control_L);
  right_ctrl_keycode_ = xconn_->GetKeyCodeFromKeySym(XK_Control_R);
  left_alt_keycode_ = xconn_->GetKeyCodeFromKeySym(XK_Alt_L);
  right_alt_keycode_ = xconn_->GetKeyCodeFromKeySym(XK_Alt_R);
  left_shift_keycode_ = xconn_->GetKeyCodeFromKeySym(XK_Shift_L);
  right_shift_keycode_ = xconn_->GetKeyCodeFromKeySym(XK_Shift_R);
}

// Helper function for HandleKeyboardState().  Looks up a keycode's new
// state in a bit vector and updates the corresponding data member in the
// overlay and returns true if it changed.
static bool UpdateState(
    const std::vector<uint8_t>& states, KeyCode keycode, bool* old_pressed) {
  bool pressed = XConnection::GetKeyCodeState(states, keycode);
  if (pressed != *old_pressed) {
    *old_pressed = pressed;
    return true;
  } else {
    return false;
  }
}

void HotkeyOverlay::HandleKeyboardState(const std::vector<uint8_t>& states) {
  bool changed = false;
  changed |= UpdateState(states, left_ctrl_keycode_, &left_ctrl_pressed_);
  changed |= UpdateState(states, right_ctrl_keycode_, &right_ctrl_pressed_);
  changed |= UpdateState(states, left_alt_keycode_, &left_alt_pressed_);
  changed |= UpdateState(states, right_alt_keycode_, &right_alt_pressed_);
  changed |= UpdateState(states, left_shift_keycode_, &left_shift_pressed_);
  changed |= UpdateState(states, right_shift_keycode_, &right_shift_pressed_);
  if (changed || !current_image_)
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
    image->SetName("hotkey overlay image");
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

}  // namespace window_manager
