// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_cros_api.h"  // NOLINT

extern "C"
bool ChromeOSCrosVersionCheck(chromeos::CrosAPIVersion version) {
  return chromeos::kCrosAPIMinVersion <= version
      && version <= chromeos::kCrosAPIVersion;
}

