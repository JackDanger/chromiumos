// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SYNAPTICS_H_
#define CHROMEOS_SYNAPTICS_H_

namespace chromeos { // NOLINT

// Synaptics parameters
enum SynapticsParameter {
  // This specifies whether or not a tap is recognized as a click.
  // 1 = true, 0 = false
  PARAM_BOOL_TAP_TO_CLICK,
  // This specifies whether or not the edge of the touchpad can be used for
  // vertical scrolling.
  // 1 = true, 0 = false
  PARAM_BOOL_VERTICAL_EDGE_SCROLLING,
  // This specifies the the sensitivity of the touchpad.
  // 1 = low sensitivity, 10 = high sensitivity
  PARAM_RANGE_TOUCH_SENSITIVITY,
  // This specifies the the speed of the cursor movement relative to touchpad.
  // 1 = slow, 10 = fast
  PARAM_RANGE_SPEED_SENSITIVITY,
};

// This method will set the synaptics setting for the passed in param to the
// value specified. For boolean parameters, the value should be 0 or 1. For
// range parameters, the value should be an integer from 1 to 10.
extern void (*SetSynapticsParameter)(SynapticsParameter param, int value);

}  // namespace chromeos

#endif  // CHROMEOS_SYNAPTICS_H_

