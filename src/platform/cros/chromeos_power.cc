// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_power.h"

#include <glog/logging.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"

namespace chromeos {

namespace {  // NOLINT

// REVISIT (seanparent) : StrLess and StrEqualTo are general function objects
// for dealing with NTBSs. An equivalent to StrLess exists in ASL -
// I'll either roll these into a chromos/cstring.h or add ASL to chromeos build.

char* NewStringCopy(const char* x) {
  char* result = static_cast<char*>(std::malloc(std::strlen(x) + 1));
  std::strcpy(result, x);  // NOLINT
  return result;
}

bool RetrieveBatteryStatus(const glib::ScopedHashTable& table,
                           PowerStatus* status) {
  bool success = true;

  success &= table.Retrieve("energy", &status->battery_energy);
  success &= table.Retrieve("energy-rate", &status->battery_energy_rate);
  success &= table.Retrieve("voltage", &status->battery_voltage);
  success &= table.Retrieve("time-to-empty", &status->battery_time_to_empty);
  success &= table.Retrieve("time-to-full", &status->battery_time_to_full);
  success &= table.Retrieve("percentage", &status->battery_percentage);
  success &= table.Retrieve("is-present", &status->battery_is_present);

  ::uint32 state = 0;
  success &= table.Retrieve("state", &state);
  status->battery_state = BatteryState(state);

  return success;
}

// If the battery proxy is empty, then clear the battery status, otherwise
// retrieve the battery status from the proxy.

bool RetrieveBatteryStatus(const dbus::Proxy& battery,
                           PowerStatus* status) {
  if (!battery) {
    // Clear the battery status but don't overwrite the line_power status.
    const PowerStatus zero_battery = { status->line_power_on };
    *status = zero_battery;
    return true;
  }

  glib::ScopedHashTable table;

  if (!dbus::RetrieveProperties(battery,
                                "org.freedesktop.DeviceKit.Power.Device",
                                &table))
    return false;

  return RetrieveBatteryStatus(table, status);
}

bool RetrieveLinePowerStatus(const dbus::Proxy& line_power,
                             PowerStatus* status) {
  if (!line_power) {
    status->line_power_on = true;
    return true;
  }
  return dbus::RetrieveProperty(line_power,
                                "org.freedesktop.DeviceKit.Power.Device",
                                "online",
                                &status->line_power_on);
}

// Will return the battery and line_power proxies if available, otherwise they
// are left unchanged. An error code is not returned because the devices may
// not be present (such as within a virtual machine for QE).

bool RetrievePowerDeviceProxies(const dbus::BusConnection& bus,
                                const dbus::Proxy& power,
                                dbus::Proxy* battery,
                                dbus::Proxy* line_power) {
  typedef glib::ScopedPtrArray<const char*> ScopedPtrArray;
  typedef ScopedPtrArray::iterator iterator;

  ScopedPtrArray devices;

  if (!dbus::CallPtrArray(power, "EnumerateDevices", &devices)) {
    DLOG(WARNING) << "Could not enumerate power devices.";
    return false;
  }

  // Iterate the devices and pull out the first battery and line-power.

  const char* battery_name = NULL;
  const char* line_power_name = NULL;

  // REVISIT (seanparent) : There is some kind of algorithm here which
  // splits a sequence to a set of outputs where each output is associated
  // with a predicate. A from of a multi-out copy_if
  //
  // copy_if(range, pred1, out1, pred2, out2, ...)

  for (iterator f = devices.begin(), l = devices.end(); f != l; ++f) {
    dbus::Proxy proxy(bus,
                      "org.freedesktop.DeviceKit.Power",
                      *f,
                      "org.freedesktop.DBus.Properties");
    ::uint32 type;
    if (!dbus::RetrieveProperty(proxy,
                                "org.freedesktop.DeviceKit.Power.Device",
                                "type",
                                &type))
      return NULL;

    if (!battery_name && type == 2)
      battery_name = *f;
    else if (!line_power_name && type == 1)
      line_power_name = *f;
  }

  DLOG_IF(WARNING, !battery_name) << "Battery is missing!";
  DLOG_IF(WARNING, !line_power_name) << "Line power is missing!";

  if (battery_name)
    *battery = dbus::Proxy(bus,
                           "org.freedesktop.DeviceKit.Power",
                           battery_name,
                           "org.freedesktop.DBus.Properties");

  if (line_power_name)
    *line_power = dbus::Proxy(bus,
                              "org.freedesktop.DeviceKit.Power",
                              line_power_name,
                              "org.freedesktop.DBus.Properties");

  return true;
}

}  // namespace

class OpaquePowerStatusConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*)>* ConnectionType;

  OpaquePowerStatusConnection(const PowerStatus& status,
                              const dbus::Proxy& battery,
                              const dbus::Proxy& line_power,
                              const PowerMonitor& monitor,
                              void* object)
     : status_(status),
       battery_(battery),
       line_power_(line_power),
       monitor_(monitor),
       object_(object),
       connection_(NULL) {
  }

  static void Run(void* object, const char* device) {
    PowerStatusConnection self = static_cast<PowerStatusConnection>(object);

    if (std::strcmp(device, self->battery_.path()) == 0)
      RetrieveBatteryStatus(self->battery_, &self->status_);
    else if (std::strcmp(device, self->line_power_.path()) == 0)
      RetrieveLinePowerStatus(self->line_power_, &self->status_);
    else
      return;

    self->monitor_(self->object_, self->status_);
  }

  ConnectionType& connection() {
    return connection_;
  }

 private:
  PowerStatus status_;
  dbus::Proxy battery_;
  dbus::Proxy line_power_;
  PowerMonitor monitor_;
  void* object_;
  ConnectionType connection_;
};

extern "C"
PowerStatusConnection ChromeOSMonitorPowerStatus(PowerMonitor monitor,
                                                 void* object) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy power(bus,
                    "org.freedesktop.DeviceKit.Power",
                    "/org/freedesktop/DeviceKit/Power",
                    "org.freedesktop.DeviceKit.Power");

  dbus::Proxy battery;
  dbus::Proxy line_power;

  if (!RetrievePowerDeviceProxies(bus, power, &battery, &line_power))
    return NULL;

  PowerStatus status = { };

  if (!RetrieveBatteryStatus(battery, &status))
    return NULL;

  if (!RetrieveLinePowerStatus(line_power, &status))
    return NULL;

  monitor(object, status);

  PowerStatusConnection result = new OpaquePowerStatusConnection(status,
      battery, line_power, monitor, object);

  result->connection() = dbus::Monitor(power, "DeviceChanged",
                                       &OpaquePowerStatusConnection::Run,
                                       result);

  return result;
}

extern "C"
void ChromeOSDisconnectPowerStatus(PowerStatusConnection connection) {
  dbus::Disconnect(connection->connection());
  delete connection;
}

extern "C"
bool ChromeOSRetrievePowerInformation(PowerInformation* info) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy power(bus,
                    "org.freedesktop.DeviceKit.Power",
                    "/org/freedesktop/DeviceKit/Power",
                    "org.freedesktop.DeviceKit.Power");

  dbus::Proxy battery;
  dbus::Proxy line_power;

  if (!RetrievePowerDeviceProxies(bus, power, &battery, &line_power))
    return false;

  glib::ScopedHashTable battery_table;
  glib::ScopedHashTable line_power_table;

  if (!dbus::RetrieveProperties(battery,
                                "org.freedesktop.DeviceKit.Power.Device",
                                &battery_table))
    return false;

  if (!dbus::RetrieveProperties(line_power,
                                "org.freedesktop.DeviceKit.Power.Device",
                                &line_power_table))
    return false;

  // NOTE (seanparent) : If this code needs to be made thread safe in the
  // future then the info_g should be moved to thread local storage.

  static bool init = false;
  static PowerInformation info_g = {};

  bool success = true;

  if (!init) {
    success &= battery_table.Retrieve("energy-empty",
                                      &info_g.battery_energy_empty);
    success &= battery_table.Retrieve("energy-full",
                                      &info_g.battery_energy_full);
    success &= battery_table.Retrieve("energy-full-design",
                                      &info_g.battery_energy_full_design);
    success &= battery_table.Retrieve("is-rechargeable",
                                      &info_g.battery_is_rechargeable);

    ::uint32 technology = 0;
    success &= battery_table.Retrieve("technology", &technology);
    info_g.battery_technology = BatteryTechnology(technology);

    // We malloc space for the strings and simply leak them.
    const char* tmp = "";

    success &= battery_table.Retrieve("vendor", &tmp);
    info_g.battery_vendor = NewStringCopy(tmp);

    success &= battery_table.Retrieve("model", &tmp);
    info_g.battery_model = NewStringCopy(tmp);

    success &= battery_table.Retrieve("serial", &tmp);
    info_g.battery_serial = NewStringCopy(tmp);

    success &= line_power_table.Retrieve("vendor", &tmp);
    info_g.line_power_vendor = NewStringCopy(tmp);

    success &= line_power_table.Retrieve("model", &tmp);
    info_g.line_power_model = NewStringCopy(tmp);

    success &= line_power_table.Retrieve("serial", &tmp);
    info_g.line_power_serial = NewStringCopy(tmp);

    init = success;
  }

  *info = info_g;

  success &= RetrieveBatteryStatus(battery_table,
                                   &info->power_status);
  success &= line_power_table.Retrieve("online",
                                       &info->power_status.line_power_on);

  return success;
}

}  // namespace chromeos
