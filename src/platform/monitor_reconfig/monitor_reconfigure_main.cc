// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/monitor_reconfig/monitor_reconfigure_main.h"

using namespace std;

static const char* kDisplay = ":0.0";

namespace chromeos_monitor_reconfig {

static inline bool isBetween(int num, int min, int max) {
  return num >= min && num <= max;
}

MonitorReconfigureMain::MonitorReconfigureMain(Display* display,
    XRRScreenResources* screen_info)
      : display_(display), screen_info_(screen_info) {
  // Initialize hash table with modes
  for (int m = 0; m < screen_info_->nmode; m++) {
    XRRModeInfo* current_mode = &screen_info_->modes[m];
    mode_map_[current_mode->id] = current_mode;
  }
  DetermineOutputs();
}

void MonitorReconfigureMain::DetermineOutputs() {
  static const string kNotebookOutputName = "LVDS1";
  XRROutputInfo* current_output = XRRGetOutputInfo(display_, screen_info_,
      screen_info_->outputs[0]);
  if (strcmp(current_output->name, kNotebookOutputName.c_str()) == 0) {
    notebook_output_ = current_output;
    external_output_ = XRRGetOutputInfo(display_, screen_info_,
        screen_info_->outputs[1]);
  } else {
    notebook_output_ = XRRGetOutputInfo(display_, screen_info_,
        screen_info_->outputs[1]);
    external_output_ = current_output;
  }
}

XRRModeInfo* MonitorReconfigureMain::FindMaxResolution(XRROutputInfo* output) {
  XRRModeInfo* mode_return = NULL;
  for (int m = 0; m < output->nmode; m++) {
    XRRModeInfo* current_mode = mode_map_[notebook_output_->modes[m]];
    if (mode_return == NULL) {
      mode_return = current_mode;
    } else {
      int n_size = mode_return->height * mode_return->width;
      int c_size = current_mode->height * current_mode->width;
      if (c_size > n_size) {
        mode_return = current_mode;
      }
    }
  }
  return mode_return;
}

bool MonitorReconfigureMain::IsWider(XRROutputInfo* output,
    XRROutputInfo* target_output) {
  bool is_wider = true;
  // Need to find the best resolution for the external monitor
  unsigned long output_wh_ratio =
    output->mm_width / output->mm_height;
  unsigned long target_output_wh_ratio =
    target_output->mm_width / target_output->mm_height;
  // Notebook is wider than external output
  if (output_wh_ratio < target_output_wh_ratio) {
    is_wider = false;
  }
  return is_wider;
}

XRRModeInfo* MonitorReconfigureMain::FindBestMatchingResolution(
    XRRModeInfo* matching_mode, bool is_notebook_wider) {
  // Need a min mode to increase from
  XRRModeInfo min_mode;
  min_mode.height = 0;
  min_mode.width = 0;
  XRRModeInfo* best_mode = &min_mode;
  // Match horizontal if notebook is wider, o/w match vertical
  for (int m = 0; m < external_output_->nmode; m++) {
    XRRModeInfo* current_mode =
      mode_map_[external_output_->modes[m]];
    if (is_notebook_wider) {
      if (isBetween(current_mode->width, best_mode->width,
                    matching_mode->width) &&
             (current_mode->height > best_mode->height)) {
               best_mode = current_mode;
      }
    } else {
      // Notebook is not wider and external mode not null
      if (isBetween(current_mode->height, best_mode->height,
                    matching_mode->width) &&
             (current_mode->width > best_mode->width)) {
        best_mode = current_mode;
      }
    }
  }
  if (best_mode->width == 0 && best_mode->height == 0)  best_mode = NULL;
  return best_mode;
}

void MonitorReconfigureMain::SetResolutions(XRRModeInfo* notebook_mode,
                                            XRRModeInfo* external_mode,
                                            XRRModeInfo* overall_screen_size) {
  // We use xrandr script to set modes
  char buffer[256];
  snprintf(buffer, sizeof(buffer), "xrandr --output %s --mode %s",
    external_output_->name, external_mode->name);
  system(buffer);
  snprintf(buffer, sizeof(buffer), "xrandr --output %s --mode %s",
    notebook_output_->name, notebook_mode->name);
  system(buffer);
  snprintf(buffer, sizeof(buffer), "xrandr --fb %s", overall_screen_size->name);
  system(buffer);
}

void MonitorReconfigureMain::Run() {
  // Find the max resolution for the notebook
  XRRModeInfo* notebook_mode = FindMaxResolution(notebook_output_);
  // Find the best mode for external output relative to above mode
  XRRModeInfo* external_mode = FindBestMatchingResolution(notebook_mode,
      IsWider(notebook_output_, external_output_));
  // Set the resolutions accordingly
  SetResolutions(notebook_mode, external_mode, notebook_mode);
}

bool MonitorReconfigureMain::IsExternalMonitorConnected() {
  return (external_output_->connection == RR_Connected);
}
}  // end namespace chromeos_monitor_reconfig

int main(int argc, char** argv) {
  Display* display = XOpenDisplay(kDisplay);
  if (display == NULL) {
    cerr << "Could not open display '"
         << kDisplay << "'" << endl;
    return 1;
  } else {
    Window window = RootWindow(display, DefaultScreen(display));
    XRRScreenResources* screen_info = XRRGetScreenResources(display, window);
    chromeos_monitor_reconfig::MonitorReconfigureMain
        main_app(display, screen_info);
    if (!main_app.IsExternalMonitorConnected()) {
      return 0;
    } else {
      main_app.Run();
      return 0;
    }
  }
}
