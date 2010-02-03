// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AUTOX_SCRIPT_RUNNER_H_
#define AUTOX_SCRIPT_RUNNER_H_

#include <map>
#include <set>
#include <string>

extern "C" {
#include <X11/Xlib.h>
}

#include "base/values.h"

namespace autox {

// ScriptRunner reads a script and injects events into the X server using
// the XTEST extension.
class ScriptRunner {
 public:
  // Ownership of 'display' remains with the caller.
  ScriptRunner(Display* display);

  // Run the passed-in script, which should be in JSON format as described
  // in autox.cc's usage string.
  void RunScript(const std::string& script);

 private:
  // Update 'keysyms_to_keycodes_' with the X server's current keyboard
  // mapping.
  void LoadKeyboardMapping();

  // Given an ASCII character, find the keysym that represents it.  Returns
  // true on success.
  bool ConvertCharToKeySym(char ch, KeySym* keysym_out);

  // Returns true if shift needs to be held for the passed-in keysym to be
  // entered.
  bool KeySymRequiresShift(KeySym keysym);

  // Get the keycode corresponding to the passed-in keysym (per
  // 'keysyms_to_keycodes_'), or 0 if no keycode maps to it.
  KeyCode GetKeyCodeForKeySym(KeySym keysym);

  // Handle "button_down" and "button_up" commands.  'values' is the
  // complete list consisting of the command name followed by X and Y
  // integer arguments.
  void HandleButtonCommand(int command_num,
                           const ListValue& values,
                           bool button_down);

  // Handle "hotkey" commands.  'values' is the command name and a string
  // consisting of a sequence of keysyms to be pressed at the same time,
  // joined by dashes.  "Ctrl", "Alt", and "Shift" can also be used.
  // "Ctrl-Alt-Tab" will type Tab while Control and Alt are held, for
  // instance.
  void HandleHotkeyCommand(int command_num, const ListValue& values);

  // Handle "key_down" and "key_up" commands.  'values' consists of the
  // command name followed by a keysym name.  The keysym must be
  // produceable without holding the Shift key.
  void HandleKeyCommand(int command_num,
                        const ListValue& values,
                        bool key_down);

  // Handle "motion" and "motion_relative" commands.  'values' consists of
  // the command name followed by X and Y integer arguments, which are
  // interpreted as either absolute or relative coordinates depending on
  // 'absolute'.
  void HandleMotionCommand(int command_num,
                           const ListValue& values,
                           bool absolute);

  // Handle "sleep" commands.  'values' consists of the command name
  // followed by the number of milliseconds to sleep.
  void HandleSleepCommand(int command_num, const ListValue& values);

  // Handle "string" commands.  'values' consists of the command name
  // followed by a string containing the characters that should be typed.
  // Keysym names may be embedded in the string, e.g. "\\(Control_L)".
  void HandleStringCommand(int command_num, const ListValue& values);

  Display* display_;  // not owned

  // Map from non-alphanumeric characters to their keysyms.
  // Used by ConvertCharToKeySym().
  std::map<char, KeySym> chars_to_keysyms_;

  // Map from keysym to the keycode that is used to produce it and whether
  // the Shift key needs to be pressed.
  std::map<KeySym, std::pair<KeyCode, bool> > keysyms_to_keycodes_;
};

}  // namespace autox

#endif  // AUTOX_SCRIPT_RUNNER_H_
