// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_METRICS_REPORTER_H_
#define WINDOW_MANAGER_METRICS_REPORTER_H_

#include <glib.h>
#include <string>

namespace chrome_os_pb {
class SystemMetrics;
}

namespace window_manager {

class LayoutManager;
class WmIpc;

// Gathers metrics and attempts to send them to Chrome for reporting.
//
// Currently asks the LayoutManager for any metrics it has been keeping,
// and then uses a WmIpc instance to talk to chrome.  Eventually, we want
// to use DBus instead.
class MetricsReporter {
 public:
  MetricsReporter(LayoutManager *lm, WmIpc *ipc)
      : lm_(lm),
        ipc_(ipc) {
  }
  ~MetricsReporter() {}

  // Gathers metrics non-destructively and then attempts to send them to
  // Chrome.  If successful, clears current metric counts.
  void AttemptReport();

  static const int kMetricsReportingIntervalInSeconds = 60;

 private:
  // The boot time is currently left on disk in a known location by
  // boot scripts.  Given the fully-specified path, this method reads in
  // the boot time and puts it into the provided protobuffer.
  // Returns true on success, false otherwise.
  bool GatherBootTime(const std::string& filename,
                      chrome_os_pb::SystemMetrics *metrics);

  LayoutManager *lm_;  // does not take ownership.
  WmIpc *ipc_;  // does not take ownership.
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_METRICS_REPORTER_H_
