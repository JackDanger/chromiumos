// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <base/logging.h>

#include <iostream>  // NOLINT

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_mount.h" // NOLINT
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which will monitor the mount
// status to std::cout and disconnect after it has reported the status
// 20 times.

void PrintMountStatus(const chromeos::MountStatus& status) {
  using std::cout;
  using std::endl;
  cout << "--------------------------------------------------" << endl;
  cout << "Total number of devices: " << status.size << endl;
  for (int x = 0; x < status.size; x++) {
    cout << "    Item Path:" <<  status.disks[x].path;
    if (status.disks[x].mountpath) {
      cout << "    Mount Path:" <<  status.disks[x].mountpath;
    }
    cout << endl;
  }
  cout << "--------------------------------------------------" << endl;
}

// Callback is an example object which can be passed to MonitorMountStatus.

class Callback {
 public:
  // You can store whatever state is needed in the function object.
  explicit Callback(GMainLoop* loop) :
    count_(0),
    loop_(loop) {
  }

  static void Run(void* object,
                  const chromeos::MountStatus& x,
                  chromeos::MountEventType evt,
                  const char* path) {
    Callback* self = static_cast<Callback*>(object);

    PrintMountStatus(x);

    ++self->count_;
    if (self->count_ == 20)
      ::g_main_loop_quit(self->loop_);
  }

 private:
  int count_;
  GMainLoop* loop_;
};

int main(int argc, const char** argv) {
  // Initialize the g_type systems an g_main event loop, normally this would be
  // done by chrome.
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);

  DCHECK(LoadCrosLibrary(argv)) << "Failed to load cros .so";

  // Display information about the mount system
  chromeos::MountStatus* info;
  info = chromeos::RetrieveMountInformation();
  DCHECK(info != NULL) << "RetrieveMountInformation failed.";

  PrintMountStatus(*info);

  // Connect the callback to monitor the mount status. The monitor function will
  // be called once immediately on connection, and then any time the status
  // changes. (Currently only called when the mount status changes.) The
  // callback must have a lifetime at least until after the call to
  // DisconnectMountStatus.

  Callback callback(loop);

  chromeos::MountStatusConnection connection =
      chromeos::MonitorMountStatus(&Callback::Run, &callback);

  ::g_main_loop_run(loop);

  // When we're done, we disconnect the mount status.
  chromeos::DisconnectMountStatus(connection);

  return 0;
}
