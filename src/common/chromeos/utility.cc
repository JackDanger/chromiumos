// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/utility.h>

#include <cstring>
#include <string>
#include <vector>

#include "chromeos/obsolete_logging.h"

namespace chromeos {

using std::string;

char DecodeChar(char in) {
  in = tolower(in);
  if ((in <= '9') && (in >= '0')) {
    return in - '0';
  } else {
    CHECK_GE(in, 'a');
    CHECK_LE(in, 'f');
    return in - 'a' + 10;
  }
}

string AsciiEncode(const Blob &blob) {
  static const char table[] = "0123456789abcdef";
  string out;
  for (Blob::const_iterator it = blob.begin(); blob.end() != it; ++it) {
    out += table[((*it) >> 4) & 0xf];
    out += table[*it & 0xf];
  }
  CHECK_EQ(blob.size() * 2, out.size());
  return out;
}

Blob AsciiDecode(const string &str) {
  Blob out;
  if (str.size() % 2)
    return out;
  for (string::const_iterator it = str.begin(); it != str.end(); ++it) {
    char append = DecodeChar(*it);
    append <<= 4;

    ++it;

    append |= DecodeChar(*it);
    out.push_back(append);
  }
  CHECK_EQ(out.size() * 2, str.size());
  return out;
}

void* SecureMemset(void *v, int c, size_t n) {
  volatile unsigned char *p = static_cast<unsigned char *>(v);
  while (n--)
    *p++ = c;
  return v;
}

}  // namespace chromeos
