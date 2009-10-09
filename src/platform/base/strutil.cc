// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glog/logging.h>  // For CHECK
#include <pcrecpp.h>
#include <sstream>
#include <cstdio>

#include "base/strutil.h"

void SplitString(const string& str, vector<string>* parts) {
  CHECK(parts);
  parts->clear();
  static pcrecpp::RE re("\\s*(\\S+)");
  pcrecpp::StringPiece input(str);
  string part;
  while (re.Consume(&input, &part)) parts->push_back(part);
}


vector<string> SplitString(const string& str) {
  vector<string> parts;
  SplitString(str, &parts);
  return parts;
}


void SplitStringUsing(const string& str,
                      const string& delim,
                      vector<string>* parts) {
  CHECK(parts);
  CHECK(!delim.empty());
  parts->clear();
  size_t start = 0;
  while (start < str.size()) {
    size_t delim_pos = str.find(delim, start);
    if (delim_pos == string::npos) {
      delim_pos = str.size();
    }
    if (delim_pos > start) {
      parts->push_back(str.substr(start, delim_pos - start));
    }
    start = delim_pos + delim.size();
  }
}


string StringPrintf(const char* format, ...) {
  char buffer[1024];  // TODO: remove magic number?
  va_list argp;
  va_start(argp, format);
  vsnprintf(buffer, sizeof(buffer), format, argp);
  va_end(argp);
  return string(buffer);
}


void JoinString(const vector<string>& parts,
                const string& delim,
                string* output) {
  CHECK(output);
  ostringstream stream;
  bool first = true;
  for (vector<string>::const_iterator part = parts.begin();
       part != parts.end(); ++part) {
    if (!first) {
      stream << delim;
    }
    stream << *part;
    first = true;
  }
  *output = stream.str();
}


string JoinString(const vector<string>& parts, const string& delim) {
  string out;
  JoinString(parts, delim, &out);
  return out;
}
