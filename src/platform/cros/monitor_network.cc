// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <glog/logging.h>
#include <vector>

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT

// Dumps the contents of a single service to the logs.
void DumpService(const chromeos::ServiceInfo& info) {
  LOG(INFO) << "Name => " << info.ssid;
  LOG(INFO) << "State => " << info.state;
  LOG(INFO) << "Type => " << info.type;
  LOG(INFO) << "Encryption => " << info.encryption;
  LOG(INFO) << "Signal Strength => " << info.signal_strength;
  LOG(INFO) << "Requires Password => " << info.needs_passphrase;
}

// Dumps the contents of ServiceStatus to the log.
void DumpServices(const chromeos::ServiceStatus *status) {
  if (status == NULL)
    return;

  for (int i = 0; i < status->size; i++) {
    DumpService(status->services[i]);
  }
}

// Callback is an example of how to use the network monitoring functionality.
class Callback {
 public:
  // You can store whatever state is needed in the function object.
  explicit Callback(GMainLoop* loop) :
    count_(0),
    loop_(loop) {
  }

  // Note, you MUST copy the service status struct since it will be freed
  // the moment this function returns.
  //
  // DO NOT DO THIS
  // Struct my_status = status;
  //
  // DO THIS INSTEAD
  // Struct my_status = {};
  // my_status = MakeACopyOf(status);
  // ...
  static void Run(void* object, const chromeos::ServiceStatus& status) {
    LOG(INFO) << "\n\nCall back received";
    DumpServices(&status);
    Callback* self = static_cast<Callback*>(object);
    ++self->count_;
    if (self->count_ == 500)
      ::g_main_loop_quit(self->loop_);
  }
 private:
  int count_;
  GMainLoop* loop_;
};

// A simple example program demonstrating how to use the ChromeOS network API.
int main(int argc, const char** argv) {
  ::g_type_init();
  GMainLoop* loop = ::g_main_loop_new(NULL, false);
  // Construct a path for the shared library. This example uses a local path
  // but on chromeos the library is installed in:
  //  "/opt/google/chrome/chromeos/libcros.so"

  std::string app_path = argv[0];
  app_path.erase(app_path.begin() + app_path.find_last_of("/"), app_path.end());
  app_path += "/libcros.so";

  // Load the symbols for the Power API. LoadPower must be called before any
  // other functions.
  bool success = chromeos::LoadCros(app_path.c_str());
  DCHECK(success) << "LoadCros('" << app_path.c_str() << "') failed.";

  chromeos::ServiceStatus* status = chromeos::GetAvailableNetworks();
  DCHECK(status) << "Unable to scan for networks";
  DumpServices(status);
  chromeos::FreeServiceStatus(status);

  Callback callback(loop);
  chromeos::NetworkStatusConnection connection =
      chromeos::MonitorNetworkStatus(&Callback::Run, &callback);
  ::g_main_loop_run(loop);

  return 0;
}

