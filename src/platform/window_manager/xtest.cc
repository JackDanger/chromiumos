// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <map>
#include <set>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

extern "C" {
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
}

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chromeos/string.h"

using chromeos::SplitStringUsing;
using std::make_pair;
using std::map;
using std::set;
using std::string;
using std::vector;

// TODO: If this gets significantly more complicated, it really needs to
// be OO-ified.

const static char* kUsage =
    "Usage: xtest SCRIPT-FILE\n"
    "\n"
    "SCRIPT-FILE is a JSON file (with trailing commas allowed) consisting\n"
    "of a list of input events that should be injected into the X server\n"
    "using the XTEST extension.  Each event is described by a list containing\n"
    "the following:\n"
    "\n"
    "  COMMAND, ARG1, ARG2, ...\n"
    "\n"
    "The following commands are available:\n"
    "\n"
    "  button_down, BUTTON   - mouse button press for given button\n"
    "  button_up, BUTTON     - mouse button release for given button\n"
    "  hotkey, TEXT          - hotkey combo (e.g. \"Ctrl-Alt-Tab\")\n"
    "  key_down, KEYSYM      - key press for named keysym (e.g. from xev)\n"
    "  key_up, KEYSYM        - key release for named keysym\n"
    "  motion, X, Y          - mouse motion to absolute coordinates\n"
    "  motion_relative, X, Y - mouse motion relative to current position\n"
    "  sleep, TIME_MS        - sleep for given number of milliseconds\n"
    "  string, TEXT          - ASCII characters (keysyms may be also\n"
    "                          be included, e.g. \"\\(Control_L)\")\n"
    "\n"
    "The following is a valid script file:\n"
    "\n"
    "  { \"script\": [\n"
    "      [ \"motion\", 10, 20 ],\n"
    "      [ \"button_down\", 1 ],\n"
    "      [ \"motion_relative\", 500, 20 ],\n"
    "      [ \"button_up\", 1 ],\n"
    "      [ \"sleep\", 500 ],\n"
    "      [ \"string\", \"one line\\nand a second line\\\\(Return)\" ],\n"
    "      [ \"key_down\", \"Alt_L\" ],\n"
    "      [ \"key_down\", \"Tab\" ],\n"
    "      [ \"key_up\", \"Tab\" ],\n"
    "      [ \"key_up\", \"Alt_L\" ],\n"
    "      [ \"hotkey\", \"Alt-Tab\" ],  // faster\n"
    "    ],\n"
    "  }\n";

// Check that a command got the expected number of arguments, crashing with
// an error otherwise.  Helper method for command handlers.
void CheckNumArgs(const ListValue& values,
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
bool ConvertEscapedStringToKeySym(const string& escaped_str,
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

// Given an ASCII character, find the keysym that represents it.  Returns
// true on success.
bool ConvertCharToKeySym(char ch, KeySym* keysym_out) {
  CHECK(keysym_out);

  if (isalnum(ch)) {
    string keysym_name(1, ch);
    *keysym_out = XStringToKeysym(keysym_name.c_str());
    return (keysym_out != NoSymbol);
  }

  // TODO: This is probably incomplete.  I can't find any existing function
  // that does something similar, though.
  static map<char, KeySym> char_map;
  if (char_map.empty()) {
    char_map.insert(make_pair(' ', XK_space));
    char_map.insert(make_pair('\n', XK_Return));
    char_map.insert(make_pair('\t', XK_Tab));
    char_map.insert(make_pair('~', XK_asciitilde));
    char_map.insert(make_pair('!', XK_exclam));
    char_map.insert(make_pair('@', XK_at));
    char_map.insert(make_pair('#', XK_numbersign));
    char_map.insert(make_pair('$', XK_dollar));
    char_map.insert(make_pair('%', XK_percent));
    char_map.insert(make_pair('^', XK_asciicircum));
    char_map.insert(make_pair('&', XK_ampersand));
    char_map.insert(make_pair('*', XK_asterisk));
    char_map.insert(make_pair('(', XK_parenleft));
    char_map.insert(make_pair(')', XK_parenright));
    char_map.insert(make_pair('-', XK_minus));
    char_map.insert(make_pair('_', XK_underscore));
    char_map.insert(make_pair('+', XK_plus));
    char_map.insert(make_pair('=', XK_equal));
    char_map.insert(make_pair('{', XK_braceleft));
    char_map.insert(make_pair('[', XK_bracketleft));
    char_map.insert(make_pair('}', XK_braceright));
    char_map.insert(make_pair(']', XK_bracketright));
    char_map.insert(make_pair('|', XK_bar));
    char_map.insert(make_pair(':', XK_colon));
    char_map.insert(make_pair(';', XK_semicolon));
    char_map.insert(make_pair('"', XK_quotedbl));
    char_map.insert(make_pair('\'', XK_apostrophe));
    char_map.insert(make_pair(',', XK_comma));
    char_map.insert(make_pair('<', XK_less));
    char_map.insert(make_pair('.', XK_period));
    char_map.insert(make_pair('>', XK_greater));
    char_map.insert(make_pair('/', XK_slash));
    char_map.insert(make_pair('?', XK_question));
  }
  map<char, KeySym>::const_iterator it = char_map.find(ch);
  if (it == char_map.end())
    return false;
  *keysym_out = it->second;
  return true;
}

// Returns true if shift needs to be held for the passed-in keysym to be
// entered.
bool KeySymRequiresShift(KeySym keysym) {
  const char* keysym_name = XKeysymToString(keysym);
  CHECK(keysym_name);
  if (keysym_name[0] != '\0' &&
      keysym_name[1] == '\0' &&
      isupper(keysym_name[0]))
    return true;

  // TODO: Again, cheesy but seems like the only way to do it.
  static set<KeySym> keysym_set;
  if (keysym_set.empty()) {
    keysym_set.insert(XK_asciitilde);
    keysym_set.insert(XK_exclam);
    keysym_set.insert(XK_at);
    keysym_set.insert(XK_numbersign);
    keysym_set.insert(XK_dollar);
    keysym_set.insert(XK_percent);
    keysym_set.insert(XK_asciicircum);
    keysym_set.insert(XK_ampersand);
    keysym_set.insert(XK_asterisk);
    keysym_set.insert(XK_parenleft);
    keysym_set.insert(XK_parenright);
    keysym_set.insert(XK_underscore);
    keysym_set.insert(XK_plus);
    keysym_set.insert(XK_braceleft);
    keysym_set.insert(XK_braceright);
    keysym_set.insert(XK_bar);
    keysym_set.insert(XK_colon);
    keysym_set.insert(XK_quotedbl);
    keysym_set.insert(XK_less);
    keysym_set.insert(XK_greater);
    keysym_set.insert(XK_question);
  }
  return (keysym_set.find(keysym) != keysym_set.end());
}

// Handle "button_down" and "button_up" commands.  'values' is the complete
// list consisting of the command name followed by X and Y integer
// arguments.
void HandleButtonCommand(Display* display,
                         int command_num,
                         const ListValue& values,
                         bool button_down) {
  CheckNumArgs(values, 1, command_num);
  int button = 1;
  CHECK(values.GetInteger(1, &button)) << "Command " << command_num;
  XTestFakeButtonEvent(display, button, button_down ? True : False, 0);
  XFlush(display);
}

// Handle "hotkey" commands.  'values' is the command name and a string
// consisting of a sequence of keysyms to be pressed at the same time,
// joined by dashes.  "Ctrl", "Alt", and "Shift" can also be used.
// "Ctrl-Alt-Tab" will type Tab while Control and Alt are held, for
// instance.
void HandleHotkeyCommand(Display* display,
                         int command_num,
                         const ListValue& values) {
  CheckNumArgs(values, 1, command_num);
  string text;
  CHECK(values.GetString(1, &text)) << "Command " << command_num;
  CHECK(!text.empty()) << "Command " << command_num;

  vector<string> parts;
  SplitStringUsing(text, "-", &parts);
  CHECK(parts.size() >= 2) << "Command " << command_num;

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
    KeyCode keycode = XKeysymToKeycode(display, keysym);
    CHECK(keycode != 0) << "Command " << command_num << ": Unable to convert "
                        << "keysym " << keysym << " (\"" << keysym_name
                        << "\") to keycode";
    keycodes.push_back(keycode);
  }

  // Press the keys in order and then release them in reverse order.
  for (vector<KeyCode>::const_iterator it = keycodes.begin();
       it != keycodes.end(); ++it) {
    XTestFakeKeyEvent(display, *it, True, 0);
  }
  for (vector<KeyCode>::const_reverse_iterator it = keycodes.rbegin();
       it != keycodes.rend(); ++it) {
    XTestFakeKeyEvent(display, *it, False, 0);
  }
  XFlush(display);
}

// Handle "key_down" and "key_up" commands.  'values' consists of the
// command name followed by a KeySym name.
void HandleKeyCommand(Display* display,
                      int command_num,
                      const ListValue& values,
                      bool key_down) {
  CheckNumArgs(values, 1, command_num);
  string keysym_name;
  CHECK(values.GetString(1, &keysym_name)) << "Command " << command_num;

  KeySym keysym = XStringToKeysym(keysym_name.c_str());
  CHECK(keysym != NoSymbol) << "Command " << command_num << ": Unable to look "
                            << "up keysym with name \"" << keysym_name << "\"";
  KeyCode keycode = XKeysymToKeycode(display, keysym);
  CHECK(keycode != 0) << "Command " << command_num << ": Unable to convert "
                      << "keysym " << keysym << " to keycode";
  XTestFakeKeyEvent(display, keycode, key_down ? True : False, 0);
  XFlush(display);
}

// Handle "motion" and "motion_relative" commands.  'values' consists of the
// command name followed by X and Y integer arguments, which are
// interpreted as either absolute or relative coordinates depending on
// 'absolute'.
void HandleMotionCommand(Display* display,
                         int command_num,
                         const ListValue& values,
                         bool absolute) {
  CheckNumArgs(values, 2, command_num);
  int x = 0, y = 0;
  CHECK(values.GetInteger(1, &x)) << "Command " << command_num;
  CHECK(values.GetInteger(2, &y)) << "Command " << command_num;
  if (absolute)
    XTestFakeMotionEvent(display, 0, x, y, 0);
  else
    XTestFakeRelativeMotionEvent(display, x, y, 0);
  XFlush(display);
}

// Handle "sleep" commands.  'values' consists of the command name followed
// by the number of milliseconds to sleep.
void HandleSleepCommand(int command_num, const ListValue& values) {
  CheckNumArgs(values, 1, command_num);
  int time_ms = 0;
  CHECK(values.GetInteger(1, &time_ms)) << "Command " << command_num;
  usleep(1000LL * time_ms);
}

// Handle "string" commands.  'values' consists of the command name
// followed by a string containing the characters that should be typed.
void HandleStringCommand(Display* display,
                         int command_num,
                         const ListValue& values) {
  CheckNumArgs(values, 1, command_num);
  string text;
  CHECK(values.GetString(1, &text)) << "Command " << command_num;

  static KeyCode shift_keycode = 0;
  if (!shift_keycode)
    shift_keycode = XKeysymToKeycode(display, XK_Shift_L);
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
    KeyCode keycode = XKeysymToKeycode(display, keysym);
    CHECK(keycode != 0) << "Command " << command_num << ": Unable to convert "
                        << "keysym " << keysym << " to keycode";

    bool shift_required = KeySymRequiresShift(keysym);
    if (shift_required)
      XTestFakeKeyEvent(display, shift_keycode, True, 0);
    XTestFakeKeyEvent(display, keycode, True, 0);
    XTestFakeKeyEvent(display, keycode, False, 0);
    if (shift_required)
      XTestFakeKeyEvent(display, shift_keycode, False, 0);
  }
  XFlush(display);
}

void RunScript(const string& filename, Display* display) {
  CHECK(display);

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

  scoped_ptr<Value> toplevel_value(base::JSONReader::Read(filename, true));
  CHECK(toplevel_value.get())
      << "Unable to parse \"" << filename << "\" as JSON";
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
      HandleButtonCommand(display, command_num, *command_list, true);
    else if (command_name == "button_up")
      HandleButtonCommand(display, command_num, *command_list, false);
    else if (command_name == "hotkey")
      HandleHotkeyCommand(display, command_num, *command_list);
    else if (command_name == "key_down")
      HandleKeyCommand(display, command_num, *command_list, true);
    else if (command_name == "key_up")
      HandleKeyCommand(display, command_num, *command_list, false);
    else if (command_name == "motion")
      HandleMotionCommand(display, command_num, *command_list, true);
    else if (command_name == "motion_relative")
      HandleMotionCommand(display, command_num, *command_list, false);
    else if (command_name == "sleep")
      HandleSleepCommand(command_num, *command_list);
    else if (command_name == "string")
      HandleStringCommand(display, command_num, *command_list);
    else
      CHECK(false) << "Command " << command_num << ": unknown command \""
                   << command_name << "\"";
  }
}

int main(int argc, char** argv) {
  if (argc != 2 || argv[1][0] == '-') {
    fprintf(stderr, "%s", kUsage);
    return 1;
  }

  FilePath script_path = FilePath(argv[1]);
  string script;
  CHECK(file_util::ReadFileToString(script_path, &script))
      << "Unable to read script file \"" << script_path.value() << "\"";
  Display* display = XOpenDisplay(NULL);
  CHECK(display) << "Couldn't open connection to X server";
  RunScript(script, display);
  XCloseDisplay(display);
  return 0;
}
