// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UTILITY_H_
#define CHROMEOS_UTILITY_H_

#include <cstring>
#include <string>
#include <vector>

// For use in a switch statement to return the string from a label. Like:
// const char* CommandToName(CommandType command) {
//    switch (command) {
//       CHROMEOS_CASE_RETURN_LABEL(CMD_DELETE);
//       CHROMEOS_CASE_RETURN_LABEL(CMD_OPEN);
//    }
//    return "Unknown commmand";
// }
#define CHROMEOS_CASE_RETURN_LABEL(label) \
    case label: return #label

namespace chromeos {

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

// Secure memset - volatile qualifier prevents a call to memset from being
// optimized away.
//
// Based on memset_s in:
// https://buildsecurityin.us-cert.gov/daisy/bsi-rules/home/g1/771-BSI.html
void* SecureMemset(void *v, int c, size_t n);

}  // namespace chromeos


#endif /* CHROMEOS_UTILITY_H_ */
