// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "autox/script_runner.h"

#include <cstdlib>
#include <unistd.h>
#include <utility>
#include <vector>

extern "C" {
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
}

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chromeos/string.h"

using chromeos::SplitStringUsing;
using std::make_pair;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::vector;

namespace autox {

// Check that a command got the expected number of arguments, crashing with
// an error otherwise.  Helper function for command handlers.
static void CheckNumArgs(const ListValue& values,
                         int num_args_expected,
                         int command_num) {
  string command_name;
  CHECK(values.GetString(0, &command_name));  // asserted in RunScript()
  int num_args = values.GetSize() - 1;
  CHECK(num_args == num_args_expected)
      << "Command " << command_num << ": " << command_name << " requires "
      << num_args_expected << " argument" << (num_args_expected == 1 ? "" : "s")
      << " (got " << num_args << " instead)";
}

// Given a string beginning with '\', interpret a prefix of the following
// characters as an escaped keysym name (e.g. "\(Return)".  The extracted
// keysym is stored in 'keysym_out', and the number of characters that
// should be skipped to get to the next character in the string (including
// the leading '\') are stored in 'escaped_length_out'.  Returns false if
// unable to interpret the escaped sequence.
static bool ConvertEscapedStringToKeySym(const string& escaped_str,
                                         KeySym* keysym_out,
                                         int* escaped_length_out) {
  CHECK(keysym_out);
  CHECK(escaped_length_out);
  CHECK(escaped_str[0] == '\\');

  if (escaped_str.size() < 2)
    return false;

  if (escaped_str[1] == '\\') {
    *keysym_out = XK_backslash;
    *escaped_length_out = 2;
    return true;
  }

  if (escaped_str[1] != '(')
    return false;
  size_t end_pos = escaped_str.find(')', 2);
  if (end_pos == string::npos || end_pos == 2)
    return false;
  *keysym_out = XStringToKeysym(escaped_str.substr(2, end_pos - 2).c_str());
  *escaped_length_out = end_pos + 1;
  return (*keysym_out != NoSymbol);
}


ScriptRunner::ScriptRunner(Display* display)
    : display_(display) {
  // TODO: This is probably incomplete.  I can't find any existing function
  // that does something similar, though.
  chars_to_keysyms_.insert(make_pair(' ', XK_space));
  chars_to_keysyms_.insert(make_pair('\n', XK_Return));
  chars_to_keysyms_.insert(make_pair('\t', XK_Tab));
  chars_to_keysyms_.insert(make_pair('~', XK_asciitilde));
  chars_to_keysyms_.insert(make_pair('!', XK_exclam));
  chars_to_keysyms_.insert(make_pair('@', XK_at));
  chars_to_keysyms_.insert(make_pair('#', XK_numbersign));
  chars_to_keysyms_.insert(make_pair('$', XK_dollar));
  chars_to_keysyms_.insert(make_pair('%', XK_percent));
  chars_to_keysyms_.insert(make_pair('^', XK_asciicircum));
  chars_to_keysyms_.insert(make_pair('&', XK_ampersand));
  chars_to_keysyms_.insert(make_pair('*', XK_asterisk));
  chars_to_keysyms_.insert(make_pair('(', XK_parenleft));
  chars_to_keysyms_.insert(make_pair(')', XK_parenright));
  chars_to_keysyms_.insert(make_pair('-', XK_minus));
  chars_to_keysyms_.insert(make_pair('_', XK_underscore));
  chars_to_keysyms_.insert(make_pair('+', XK_plus));
  chars_to_keysyms_.insert(make_pair('=', XK_equal));
  chars_to_keysyms_.insert(make_pair('{', XK_braceleft));
  chars_to_keysyms_.insert(make_pair('[', XK_bracketleft));
  chars_to_keysyms_.insert(make_pair('}', XK_braceright));
  chars_to_keysyms_.insert(make_pair(']', XK_bracketright));
  chars_to_keysyms_.insert(make_pair('|', XK_bar));
  chars_to_keysyms_.insert(make_pair(':', XK_colon));
  chars_to_keysyms_.insert(make_pair(';', XK_semicolon));
  chars_to_keysyms_.insert(make_pair('"', XK_quotedbl));
  chars_to_keysyms_.insert(make_pair('\'', XK_apostrophe));
  chars_to_keysyms_.insert(make_pair(',', XK_comma));
  chars_to_keysyms_.insert(make_pair('<', XK_less));
  chars_to_keysyms_.insert(make_pair('.', XK_period));
  chars_to_keysyms_.insert(make_pair('>', XK_greater));
  chars_to_keysyms_.insert(make_pair('/', XK_slash));
  chars_to_keysyms_.insert(make_pair('?', XK_question));

  LoadKeyboardMapping();
}

void ScriptRunner::RunScript(const string& script) {
  CHECK(display_);

  // Reading JSON programmatically is pretty ugly, but the general
  // structure is a dictionary with "script" mapping to a list of commands,
  // where each command is itself a list consisting of a command name
  // followed by the command's arguments:
  //
  // { "script": [
  //     [ "motion", 20, 40 ],
  //     [ "button_down", 1 ],
  //     [ "motion", 400, 300 ],
  //     [ "button_up", 1 ],
  //   ],
  // }
  //
  // TODO: The toplevel dictionary is there to support additional
  // parameters that will inevitably be needed at some point.

  scoped_ptr<Value> toplevel_value(base::JSONReader::Read(script, true));
  CHECK(toplevel_value.get())
      << "Unable to parse script as JSON";
  CHECK(toplevel_value->IsType(Value::TYPE_DICTIONARY))
      << "Toplevel value must be a dictionary";
  DictionaryValue* toplevel_dict =
      static_cast<DictionaryValue*>(toplevel_value.get());

  Value* script_value = NULL;
  CHECK(toplevel_dict->Get(L"script", &script_value))
        << "No \"script\" value in toplevel dictionary";
  CHECK(script_value->IsType(Value::TYPE_LIST))
      << "\"script\" value must be a list";
  ListValue* script_list = static_cast<ListValue*>(script_value);
  const int num_commands = script_list->GetSize();

  for (int command_num = 0; command_num < num_commands; ++command_num) {
    Value* command_value = NULL;
    CHECK(script_list->Get(command_num, &command_value));
    CHECK(command_value->IsType(Value::TYPE_LIST))
        << "Command " << command_num << ": not a list";
    ListValue* command_list = static_cast<ListValue*>(command_value);

    CHECK(!command_list->empty())
        << "Command " << command_num << ": list is empty";
    Value* command_name_value = NULL;
    CHECK(command_list->Get(0, &command_name_value));
    CHECK(command_name_value->IsType(Value::TYPE_STRING))
        << "Command " << command_num << ": list must start with string";
    string command_name;
    CHECK(command_name_value->GetAsString(&command_name));

    if (command_name == "button_down")
      HandleButtonCommand(command_num, *command_list, true);
    else if (command_name == "button_up")
      HandleButtonCommand(command_num, *command_list, false);
    else if (command_name == "hotkey")
      HandleHotkeyCommand(command_num, *command_list);
    else if (command_name == "key_down")
      HandleKeyCommand(command_num, *command_list, true);
    else if (command_name == "key_up")
      HandleKeyCommand(command_num, *command_list, false);
    else if (command_name == "motion")
      HandleMotionCommand(command_num, *command_list, true);
    else if (command_name == "motion_relative")
      HandleMotionCommand(command_num, *command_list, false);
    else if (command_name == "sleep")
      HandleSleepCommand(command_num, *command_list);
    else if (command_name == "string")
      HandleStringCommand(command_num, *command_list);
    else
      CHECK(false) << "Command " << command_num << ": unknown command \""
                   << command_name << "\"";
  }
}

void ScriptRunner::LoadKeyboardMapping() {
  int min_keycode = 0, max_keycode = 0;
  XDisplayKeycodes(display_, &min_keycode, &max_keycode);
  int num_keycodes = max_keycode - min_keycode + 1;

  int keysyms_per_keycode = 0;
  KeySym* keysyms = XGetKeyboardMapping(
      display_, min_keycode, num_keycodes, &keysyms_per_keycode);
  CHECK(keysyms);
  CHECK(keysyms_per_keycode >= 1);

  keysyms_to_keycodes_.clear();
  for (int i = 0; i < num_keycodes; ++i) {
    KeyCode keycode = min_keycode + i;
    for (int j = 0; j < keysyms_per_keycode; ++j) {
      // This is poorly documented, but it appears to match up with
      // xmodmap's documentation: the first keysym is typed without any
      // modifiers, the second keysym is typed with Shift, the third with
      // Mode_switch, and the fourth with both Shift and Mode_switch ("Up
      // to eight keysyms may be attached to a key, however the last four
      // are not used in any major X server implementation").  We only care
      // about the first two.
      if (j > 1)
        continue;

      KeySym keysym = keysyms[i * keysyms_per_keycode + j];
      if (keysym == NoSymbol)
        continue;

      // If we already found a way to type this keysym, only replace it if
      // the old way required Shift but the new one doesn't.
      if (keysyms_to_keycodes_.count(keysym) &&
          (!keysyms_to_keycodes_[keysym].second || j != 0))
        continue;

      bool shift_required = (j == 1);
      keysyms_to_keycodes_[keysym] = make_pair(keycode, shift_required);
    }
  }
  XFree(keysyms);
}

bool ScriptRunner::ConvertCharToKeySym(char ch, KeySym* keysym_out) {
  CHECK(keysym_out);

  if (isalnum(ch)) {
    string keysym_name(1, ch);
    *keysym_out = XStringToKeysym(keysym_name.c_str());
    return (keysym_out != NoSymbol);
  }

  map<char, KeySym>::const_iterator it = chars_to_keysyms_.find(ch);
  if (it == chars_to_keysyms_.end())
    return false;
  *keysym_out = it->second;
  return true;
}

bool ScriptRunner::KeySymRequiresShift(KeySym keysym) {
  map<KeySym, pair<KeyCode, bool> >::const_iterator it =
      keysyms_to_keycodes_.find(keysym);
  return (it != keysyms_to_keycodes_.end()) ? it->second.second : false;
}

KeyCode ScriptRunner::GetKeyCodeForKeySym(KeySym keysym) {
  map<KeySym, pair<KeyCode, bool> >::const_iterator it =
      keysyms_to_keycodes_.find(keysym);
  return (it != keysyms_to_keycodes_.end()) ? it->second.first : 0;
}

void ScriptRunner::HandleButtonCommand(int command_num,
                                       const ListValue& values,
                                       bool button_down) {
  CheckNumArgs(values, 1, command_num);
  int button = 1;
  CHECK(values.GetInteger(1, &button)) << "Command " << command_num;
  XTestFakeButtonEvent(display_, button, button_down ? True : False, 0);
  XFlush(display_);
}

void ScriptRunner::HandleHotkeyCommand(int command_num,
                                       const ListValue& values) {
  CheckNumArgs(values, 1, command_num);
  string text;
  CHECK(values.GetString(1, &text)) << "Command " << command_num;
  CHECK(!text.empty()) << "Command " << command_num;

  vector<string> parts;
  SplitStringUsing(text, "-", &parts);
  CHECK(parts.size() >= 2) << "Command " << command_num;

  bool saw_shift = false;

  vector<KeyCode> keycodes;
  for (vector<string>::const_iterator it = parts.begin();
       it != parts.end(); ++it) {
    string keysym_name = *it;
    // Map some convenient short names to full keysym names.
    if (keysym_name == "Ctrl")
      keysym_name = "Control_L";
    else if (keysym_name == "Alt")
      keysym_name = "Alt_L";
    else if (keysym_name == "Shift")
      keysym_name = "Shift_L";

    KeySym keysym = XStringToKeysym(keysym_name.c_str());
    CHECK(keysym != NoSymbol)
        << "Command " << command_num << ": Unable to look "
        << "up keysym with name \"" << keysym_name << "\"";

    if (keysym == XK_Shift_L || keysym ==  XK_Shift_R)
      saw_shift = true;

    KeyCode keycode = GetKeyCodeForKeySym(keysym);
    CHECK(keycode != 0) << "Command " << command_num << ": Unable to convert "
                        << "keysym " << keysym << " (\"" << keysym_name
                        << "\") to keycode";

    // Crash if we're being asked to press a key that requires Shift and the
    // Shift key wasn't pressed already (but let it slide if they're just
    // asking for an uppercase letter).
    CHECK(!KeySymRequiresShift(keysym) || saw_shift ||
          (isupper(keysym_name[0]) && keysym_name[1] == '\0'))
        << "Command " << command_num << ": Keysym " << keysym_name
        << " requires the Shift key to be held, but it wasn't seen earlier in "
        << "the key combo.  Either press Shift first or use the keycode's "
        << "non-shifted keysym";

    keycodes.push_back(keycode);
  }

  // Press the keys in order and then release them in reverse order.
  for (vector<KeyCode>::const_iterator it = keycodes.begin();
       it != keycodes.end(); ++it) {
    XTestFakeKeyEvent(display_, *it, True, 0);
  }
  for (vector<KeyCode>::const_reverse_iterator it = keycodes.rbegin();
       it != keycodes.rend(); ++it) {
    XTestFakeKeyEvent(display_, *it, False, 0);
  }
  XFlush(display_);
}

void ScriptRunner::HandleKeyCommand(int command_num,
                                    const ListValue& values,
                                    bool key_down) {
  CheckNumArgs(values, 1, command_num);
  string keysym_name;
  CHECK(values.GetString(1, &keysym_name)) << "Command " << command_num;

  KeySym keysym = XStringToKeysym(keysym_name.c_str());
  CHECK(keysym != NoSymbol) << "Command " << command_num << ": Unable to look "
                            << "up keysym with name \"" << keysym_name << "\"";
  KeyCode keycode = GetKeyCodeForKeySym(keysym);
  CHECK(keycode != 0) << "Command " << command_num << ": Unable to convert "
                      << "keysym " << keysym << " to keycode";

  CHECK(!KeySymRequiresShift(keysym))
      << "Command " << command_num << ": Keysym " << keysym_name << " cannot "
      << "be typed with the \"key\" command since it requires the Shift key "
      << "to be held.  Either use \"string\" or use separate \"key\" commands, "
      << "one with Shift and then one with the keycode's non-shifted keysym";

  XTestFakeKeyEvent(display_, keycode, key_down ? True : False, 0);
  XFlush(display_);
}

void ScriptRunner::HandleMotionCommand(int command_num,
                                       const ListValue& values,
                                       bool absolute) {
  CheckNumArgs(values, 2, command_num);
  int x = 0, y = 0;
  CHECK(values.GetInteger(1, &x)) << "Command " << command_num;
  CHECK(values.GetInteger(2, &y)) << "Command " << command_num;
  if (absolute)
    XTestFakeMotionEvent(display_, 0, x, y, 0);
  else
    XTestFakeRelativeMotionEvent(display_, x, y, 0);
  XFlush(display_);
}

void ScriptRunner::HandleSleepCommand(int command_num,
                                      const ListValue& values) {
  CheckNumArgs(values, 1, command_num);
  int time_ms = 0;
  CHECK(values.GetInteger(1, &time_ms)) << "Command " << command_num;
  usleep(1000LL * time_ms);
}

void ScriptRunner::HandleStringCommand(int command_num,
                                       const ListValue& values) {
  CheckNumArgs(values, 1, command_num);
  string text;
  CHECK(values.GetString(1, &text)) << "Command " << command_num;

  KeyCode shift_keycode = GetKeyCodeForKeySym(XK_Shift_L);
  CHECK(shift_keycode) << "Unable to look up keycode for XK_Shift_L";

  for (size_t i = 0; i < text.size(); ++i) {
    char ch = text[i];

    KeySym keysym;
    if (ch == '\\') {
      int num_chars_to_skip = 1;
      CHECK(ConvertEscapedStringToKeySym(
                text.substr(i), &keysym, &num_chars_to_skip))
          << "Command " << command_num << ": Unable to convert escaped "
          << "sequence at beginning of \"" << text.substr(i) << "\" to keysym";
      CHECK(num_chars_to_skip >= 1);
      i += num_chars_to_skip - 1;  // - 1 to compensate for loop's ++i
    } else {
      CHECK(ConvertCharToKeySym(ch, &keysym))
          << "Command " << command_num << ": Unable to convert character '"
          << ch << "' to keysym";
    }
    KeyCode keycode = GetKeyCodeForKeySym(keysym);
    CHECK(keycode != 0) << "Command " << command_num << ": Unable to convert "
                        << "keysym " << keysym << " to keycode";

    bool shift_required = KeySymRequiresShift(keysym);
    if (shift_required)
      XTestFakeKeyEvent(display_, shift_keycode, True, 0);
    XTestFakeKeyEvent(display_, keycode, True, 0);
    XTestFakeKeyEvent(display_, keycode, False, 0);
    if (shift_required)
      XTestFakeKeyEvent(display_, shift_keycode, False, 0);
  }
  XFlush(display_);
}

}  // namespace autox
