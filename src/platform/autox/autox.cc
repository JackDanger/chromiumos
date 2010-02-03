// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <string>

extern "C" {
#include <X11/Xlib.h>
}

#include "autox/script_runner.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"

using std::string;


const static char* kUsage =
    "Usage: autox SCRIPT-FILE\n"
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

  autox::ScriptRunner script_runner(display);
  script_runner.RunScript(script);

  XCloseDisplay(display);
  return 0;
}
