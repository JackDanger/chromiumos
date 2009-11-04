// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_UTILITY_H_
#define CHROMEOS_UTILITY_H_

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

#endif /* CHROMEOS_UTILITY_H_ */

