// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/metrics_reporter.h"

#include <stdio.h>

#include "window_manager/layout_manager.h"
#include "window_manager/system_metrics.pb.h"
#include "window_manager/window.h"

namespace chromeos {

// This is the path to a file that's created by boot scripts, which contains
// the boot time drawn from bootchart.
// TODO: communicate this information over something like DBus.
const char kBootTimeFilename[] = "/tmp/boot-time";

class ScopedFilePointer {
 public:
  explicit ScopedFilePointer(FILE* const fp) : fp_(fp) {}
  ~ScopedFilePointer() {
    if (!fp_)
      return;
    CHECK_EQ(0, fclose(fp_));
  }
  FILE* get() { return fp_; }
 private:
  FILE* const fp_;
  DISALLOW_COPY_AND_ASSIGN(ScopedFilePointer);
};

void MetricsReporter::AttemptReport() {
  Window *chrome_window = lm_->GetChromeWindow();
  if (!chrome_window) {  // no top-level chrome windows open right now.
    return;
  }
  LayoutManager::Metrics *metrics = lm_->GetMetrics();
  if (!metrics) {
    return;
  }
  chrome_os_pb::SystemMetrics metrics_pb;
  metrics->Populate(&metrics_pb);

  if (GatherBootTime(kBootTimeFilename, &metrics_pb)) {
    remove(kBootTimeFilename);
  }

  std::string encoded_metrics;
  metrics_pb.SerializeToString(&encoded_metrics);
  if (ipc_->SetSystemMetricsProperty(chrome_window->xid(), encoded_metrics)) {
    metrics->Reset();
  }
}

bool MetricsReporter::GatherBootTime(const std::string& filename,
                                     chrome_os_pb::SystemMetrics *metrics) {
  ScopedFilePointer fp(fopen(filename.c_str(), "r"));
  if (!fp.get()) {
    return false;
  }
  // The file should be one line, with one integer on it.
  char line[12];  // 10 digits plus \n and \0.
  if (!fgets(line, sizeof(line), fp.get())) {
    return false;
  }
  int boot_time;
  if (0 == sscanf(line, "%d", &boot_time)) {  // should ignore trailing \n
    return false;
  }
  metrics->set_boot_time_ms(boot_time);
  return true;
}

}  // namespace chromeos
