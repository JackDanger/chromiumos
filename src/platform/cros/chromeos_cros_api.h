// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_CROS_API_VERSION_H_
#define CHROMEOS_CROS_API_VERSION_H_

// This file defines two version numbers for the CrosAPI.
//
// We have two components, the libcros.so (so) and the libcros API which is
// built into chrome (app). The version of the two components may not match.
// The so will support any app version in the range
// [kCrosAPIMinVersion, kCrosAPIVersion]. Currently the app will not support
// older versions of the so.

// The expected workflow is that the so is updated to be backward compatible
// by at least one version of the app. This allows the so to be updated
// then the app without breaking either build. Support for older than n-1
// versions should only be done if there isn't any advantage to breaking the
// older versions (for example, cleaning out otherwise dead code).

// Current version numbers:
//  0: Version number prior to the version scheme.
//  1: Added CrosVersionCheck API.
//     Changed load to take file path instead of so handle.
//  2: Changed the interface for network monitoring callbacks.
//  3: Added support disconnecting the network monitor.

namespace chromeos {  // NOLINT

enum CrosAPIVersion {
  kCrosAPIMinVersion = 2,
  kCrosAPIVersion = 3
};

// Default path to pass to LoadCros: "/opt/google/chrome/chromeos/libcros.so"
extern char const * const kCrosDefaultPath;

// \param path_to_libcros is the path to the libcros.so file.
// \result true indicates success, false failure.
bool LoadCros(const char* path_to_libcros);

}  // namespace chromeos

#endif /* CHROMEOS_CROS_API_VERSION_H_ */

