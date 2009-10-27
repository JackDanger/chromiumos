// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MONITOR_RECONFIGURE_MAIN_H_
#define MONITOR_RECONFIGURE_MAIN_H_

#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <iostream>
#include <cstdio>
#include <map>

namespace chromeos_monitor_reconfig {

// MonitorReconfigureMain is the class responsible for setting the external
// monitor to the max resolution based on the modes supported by the native
// monitor and the external monitor
class MonitorReconfigureMain {
 public:
  MonitorReconfigureMain(Display* display, XRRScreenResources* screen_info);
  virtual ~MonitorReconfigureMain() {}
  // Main entry point
  void Run();
  // Returns whether an external monitor is connected
  bool IsExternalMonitorConnected();
 private:
  // Finds the max resolution mode for the given |output|
  XRRModeInfo* FindMaxResolution(XRROutputInfo* output);
  // Finds the best matching resolution as compared to the |matching_mode|
  // |is_wider| should be set if matching_mode output has a wider
  // perspective (i.e. 16:9 is wider than 4:3)
  XRRModeInfo* FindBestMatchingResolution(XRRModeInfo* matching_mode,
      bool is_wider);
  // Initializes the |notebook_output_| and |external_output_| fields
  void DetermineOutputs();
  // Sets the resolution of the notebook's screen, the external monitors screen
  // and the overall virtual screen to the gives sizes
  void SetResolutions(XRRModeInfo* notebook_mode,
                      XRRModeInfo* external_mode,
                      XRRModeInfo* overall_screen_size);
  // Returns whether |output| has a wider width/height ratio than the
  // |target_output|
  bool IsWider(XRROutputInfo* output, XRROutputInfo* target_output);
  // Mapping between mode XID's and mode information structures
  std::map <int, XRRModeInfo*> mode_map_;
  // X Resources needed between functions
  Display* display_;
  XRRScreenResources* screen_info_;
  XRROutputInfo* notebook_output_;
  XRROutputInfo* external_output_;
};
}

#endif /* MONITOR_RECONFIGURE_MAIN_H_ */
