// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <glib-object.h>
#include <glog/logging.h>

#include <iostream>  // NOLINT

#include "chromeos_cros_api.h"  // NOLINT
#include "chromeos_power.h"
#include "monitor_utils.h" //NOLINT

// \file This is a simple console application which will monitor the power
// status to std::cout and disconnect after it has reported the status
// 20 times.

void PrintPowerStatus(const chromeos::PowerStatus& x) {
  using std::cout;
  using std::endl;

  cout << "             line_power_on: " << x.line_power_on << endl;
  cout << "            battery_energy: " << x.battery_energy << endl;
  cout << "       battery_energy_rate: " << x.battery_energy_rate << endl;
  cout << "           battery_voltage: " << x.battery_voltage << endl;
  cout << "     battery_time_to_empty: " << x.battery_time_to_empty << endl;
  cout << "      battery_time_to_full: " << x.battery_time_to_full << endl;
  cout << "        battery_percentage: " << x.battery_percentage << endl;
  cout << "        battery_is_present: " << x.battery_is_present << endl;
  cout << "             battery_state: " << x.battery_state << endl;
  cout << "--------------------------------------------------" << endl;
}

// Callback is an example object which can be passed to MonitorPowerStatus.

class Callback {
 public:
  // You can store whatever state is needed in the function object.
  explicit Callback(GMainLoop* loop) :
    count_(0),
    loop_(loop) {
  }

  static void Run(void* object, const chromeos::PowerStatus& x) {
    Callback* self = static_cast<Callback*>(object);

    PrintPowerStatus(x);

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

  // Display information about the power system
  chromeos::PowerInformation info = {};
  bool success;
  success = chromeos::RetrievePowerInformation(&info);
  DCHECK(success) << "RetrievePowerInformation failed.";

  PrintPowerStatus(info.power_status);

  using std::cout;
  using std::endl;

  cout << "      battery_energy_empty: " << info.battery_energy_empty << endl;
  cout << "       battery_energy_full: " << info.battery_energy_full << endl;
  cout << "battery_energy_full_design: " << info.battery_energy_full_design
      << endl;
  cout << "   battery_is_rechargeable: " << info.battery_is_rechargeable
      << endl;
  cout << "          battery_capacity: " << info.battery_capacity << endl;
  cout << "        battery_technology: " << info.battery_technology << endl;
  cout << "            battery_vendor: " << info.battery_vendor << endl;
  cout << "             battery_model: " << info.battery_model << endl;
  cout << "            battery_serial: " << info.battery_serial << endl;
  cout << "         line_power_vendor: " << info.line_power_vendor << endl;
  cout << "          line_power_model: " << info.line_power_model << endl;
  cout << "         line_power_serial: " << info.line_power_serial << endl;
  cout << "--------------------------------------------------" << endl << endl;

  // Connect the callback to monitor the power status. The monitor function will
  // be called once immediately on connection, and then any time the status
  // changes. (Currently only called when the battery status changes.) The
  // callback must have a lifetime at least until after the call to
  // DisconnectPowerStatus.

  Callback callback(loop);

  chromeos::PowerStatusConnection connection =
      chromeos::MonitorPowerStatus(&Callback::Run, &callback);

  ::g_main_loop_run(loop);

  // When we're done, we disconnect the power status.
  chromeos::DisconnectPowerStatus(connection);

  return 0;
}
