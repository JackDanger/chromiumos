// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_offline/utils.h"

#include <fcntl.h>
#include <stdlib.h>

#include "base/basictypes.h"
#include "chromeos/obsolete_logging.h"

namespace pam_offline {

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

string PathAppend(const string &base_path, const string &leaf)
{
  string rv(base_path);
  if (rv[rv.length() - 1] != '/') {
    rv.append("/");
  }

  rv.append(leaf);
  return rv;
}

bool LoadFileBytes(const string &filename, Blob *blob) {
  CHECK(blob);

  int fd = open(filename.c_str(), O_RDONLY);
  if (fd == -1) {
    PLOG(ERROR) << "Unable to open file '" << filename << "'";
    return false;
  }

  struct stat st;
  if (fstat(fd, &st) == -1) {
    PLOG(ERROR) << "Error stating file '" << filename << "'";
    close(fd);
    return false;
  }

  off_t file_size = st.st_size;
  unsigned char *buf = new unsigned char[file_size];
  if (buf == 0) {
    PLOG(ERROR) << "Error allocating buffer";
    return false;
  }

  int buf_size = read(fd, buf, file_size);
  if (buf_size != file_size) {
    PLOG(ERROR) << "Error reading file '" << filename << "'";
    free(buf);
    close(fd);
    return false;
  }

  close(fd);

  blob->assign(buf, buf + buf_size);

  delete buf;

  return true;
}

bool LoadFileString(const string &filename, string *str) {
  CHECK(str);

  Blob bytes;

  if (!LoadFileBytes(filename, &bytes)) {
    return false;
  }

  str->assign(bytes.begin(), bytes.end());
  return true;
}

} // namespace pam_offline
