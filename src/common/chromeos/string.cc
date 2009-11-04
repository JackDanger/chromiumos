// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/string.h>

#include <pcrecpp.h>

#include "base/logging.h"

namespace chromeos {

void SplitString(const std::string& str, std::vector<std::string>* parts) {
  CHECK(parts);
  parts->clear();
  static pcrecpp::RE re("\\s*(\\S+)");
  pcrecpp::StringPiece input(str);

  // TODO (seanparent) : This code copies each substring into the vector.
  // look at the gcc implementation of string (is it copy-on-write?). Likely
  // it would be better to insert and empty string then swap the part into
  // place.

  std::string part;
  while (re.Consume(&input, &part))
    parts->push_back(part);
}

void SplitStringUsing(const std::string& str,
                      const std::string& delim,
                      std::vector<std::string>* parts) {
  CHECK(parts);
  CHECK(!delim.empty());
  parts->clear();
  size_t start = 0;
  while (start < str.size()) {
    size_t delim_pos = str.find(delim, start);
    if (delim_pos == std::string::npos) {
      delim_pos = str.size();
    }
    if (delim_pos > start) {
      parts->push_back(str.substr(start, delim_pos - start));
    }
    start = delim_pos + delim.size();
  }
}

}  // namespace chromeos

