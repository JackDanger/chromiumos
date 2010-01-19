// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAM_OFFLINE_UTILS_H_
#define PAM_OFFLINE_UTILS_H_

#include <string>
#include <vector>

namespace pam_offline {

typedef std::vector<unsigned char> Blob;

// Returns a string that represents the hexadecimal encoded contents of blob.
// String will contain only the characters 0-9 and a-f.
//
// Parameters
//  blob   - The bytes to encode.
//  string - ASCII representation of the blob in hex.
//
std::string AsciiEncode(const Blob &blob);

// Converts a string representing a sequence of bytes in hex into the actual
// bytes.
//
// Parameters
//  str - The bytes to decode.
// Returns
//  The decoded bytes as a Blob.
//
Blob AsciiDecode(const std::string &str);

// Appends a path element to the end of a path, without creating duplicate
// forward-slash characters.
//
// Parameters
//  base_path - The path prefix, optionally ending in a trailing slash.
//  leaf      - The path suffix.
// Returns
//  A string containing base_path concatenated with leaf, joined by just
//  one '/'.
//
std::string PathAppend(const std::string &base_path, const std::string &leaf);

// Loads the contents of a file into a Blob.
//
// Parameters
//  filename - The file to load.
//  blob     - This blob will be .assign()ed the contents of the file.
// Returns
//  Returns true if the file was loaded successfully, false if not.  This
//  will also PLOG(ERROR) if unsuccessful.
//
bool LoadFileBytes(const std::string &filename, Blob *blob);

// Loads the contents of a file into a string.
//
// Parameters
//  filename - The file to load.
//  str      - This string will be .assign()ed the contents of the file.
// Returns
//  Returns true if the file was loaded successfully, false if not.  This
//  will also PLOG(ERROR) if unsuccessful.
//
bool LoadFileString(const std::string &filename, std::string *str);

} // namespace pam_offline

#endif // PAM_OFFLINE_UTILS_H_
