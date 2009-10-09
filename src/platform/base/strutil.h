// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __PLATFORM_BASE_STRUTIL_H__
#define __PLATFORM_BASE_STRUTIL_H__

#include <string>
#include <vector>

#include "base/basictypes.h"

using namespace std;

// For use in a switch statement to return the string form of a label. Like:
// const char* CommandToName(CommandType command) {
//    switch (command) {
//       CASE_RETURN_LABEL(CMD_DELETE);
//       CASE_RETURN_LABEL(CMD_OPEN);
//    }
//    return "Unknown commmand";
// }
#define CASE_RETURN_LABEL(label)                \
  case label: return #label

// Split a string on whitespace, saving the individual pieces to 'parts'.
void SplitString(const string& str, vector<string>* parts);

// Split a string on whitespace, returning the individual pieces as a new
// vector.
vector<string> SplitString(const string& str);

void SplitStringUsing(const string& str,
                      const string& delim,
                      vector<string>* parts);


void JoinString(const vector<string>& parts,
                const string& delim,
                string* output);


string JoinString(const vector<string>& parts, const string& delim);


string StringPrintf(const char* format, ...);

#endif  // __PLATFORM_BASE_STRUTIL_H__
