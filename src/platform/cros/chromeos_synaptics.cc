// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_synaptics.h"  // NOLINT

#include <glog/logging.h>
#include <string>
#include "base/strutil.h"

extern "C" {
#include "synclient.h"
}

namespace chromeos {  // NOLINT

namespace { // NOLINT

const double kMaxTapTimeMin = 0;
const double kMaxTapTimeMax = 180;
const double kVertEdgeScrollMin = 0;
const double kVertEdgeScrollMax = 1;
const double kFingerHighMin = 25;
const double kFingerHighMax = 70;
const double kMaxSpeedMin = 0.2;
const double kMaxSpeedMax = 1.1;

// This method will covert a range value between 1 and 10 to a value between
// low and high. The values will be linearly mapped, where 1 will be mapped to
// low and 10 will be mapped to high. And anything in between will be mapped to
// a value between low and high. If low>high, this method will still work where
// a larger rangevalue will result in a smaller number.
double ConvertRange(double low, double high, int rangevalue) {
  // We just use y = mx + b to solve this.
  // since there are 9 steps in range values between 1 and 10.
  // b = low
  // m = (high - low) / 9
  // x = rangevalue - 1
  double b = low;
  double m = (high - low) / 9.0;
  double x = static_cast<double>(rangevalue - 1);
  return m * x + b;
}

}  // namespace

extern "C"
void ChromeOSSetSynapticsParameter(SynapticsParameter param, int value) {
  string command;
  switch (param) {
    case PARAM_BOOL_TAP_TO_CLICK:
      // To enable/disable tap-to-click (i.e. a tap on the touchpad is
      // recognized as a left mouse click event), we use MaxTapTime.
      // MaxTapTime is the maximum time (in milliseconds) for detecting a tap.
      // To specify on, we set MaxTapTime to kMaxTapTimeMax.
      // To specify off, we set MaxTapTime to kMaxTapTimeMin.
      command = StringPrintf("MaxTapTime=%f", (value == 0) ? kMaxTapTimeMin :
                                                             kMaxTapTimeMax);
      break;
    case PARAM_BOOL_VERTICAL_EDGE_SCROLLING:
      // To enable/disable vertical edge scroll, we use VertEdgeScroll.
      // Vertical edge scroll lets you use the right edge of the touchpad to
      // control the movement of the vertical scroll bar.
      // To specify on, we set VertEdgeScroll to kVertEdgeScrollMax.
      // To specify off, we set VertEdgeScroll to kVertEdgeScrollMin.
      command = StringPrintf("VertEdgeScroll=%f", (value == 0) ?
                                                  kVertEdgeScrollMin :
                                                  kVertEdgeScrollMax);
      break;
    case PARAM_RANGE_TOUCH_SENSITIVITY:
      // To set the touch sensitivity, we use FingerHigh, which represents the
      // the pressure needed for a tap to be registered. The range of FingerHigh
      // goes from kFingerHighMax to kFingerHighMin. We store the sensitivity
      // preference as an int from 1 to 10.
      // A range value of 1 represent a FingerHigh value of kFingerHighMax.
      // A range value of 10 represent a FingerHigh value of kFingerHighMin.
      command = StringPrintf("FingerHigh=%f", ConvertRange(kFingerHighMax,
                                                           kFingerHighMin,
                                                           value));
      break;
    case PARAM_RANGE_SPEED_SENSITIVITY:
      // To set speed factor, we use MaxSpeed. MinSpeed is set to 0.2
      // MaxSpeed can go from kMaxSpeedMin to kMaxSpeedMax. The preference is
      // an integer between 1 and 10.
      // A range value of 1 represent a MaxSpeed value of kMaxSpeedMin.
      // A range value of 10 represent a MaxSpeed value of kMaxSpeedMax.
      command = StringPrintf("MaxSpeed=%f", ConvertRange(kMaxSpeedMin,
                                                         kMaxSpeedMax,
                                                         value));
      break;
  }

  if (!command.empty()) {
    LOG(INFO) << "Setting synaptics parameter " << command.c_str();
    SynclientSetParameter(command.c_str());
  }
}

}  // namespace chromeos
