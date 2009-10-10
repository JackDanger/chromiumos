// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>

#include "chromeos_cros_api.h" // NOLINT
#include "chromeos_network.h"  // NOLINT
#include "chromeos_power.h"  // NOLINT

namespace chromeos {  // NOLINT

typedef bool (*CrosVersionCheckType)(chromeos::CrosAPIVersion);
typedef PowerStatusConnection (*MonitorPowerStatusType)(PowerMonitor, void*);
typedef void (*DisconnectPowerStatusType)(PowerStatusConnection);
typedef bool (*RetrievePowerInformationType)(PowerInformation* information);
typedef bool (*ConnectToWifiNetworkType)(const char*,
                                         const char*,
                                         const char*);
typedef ServiceStatus* (*GetAvailableNetworksType)();
typedef void (*FreeServiceStatusType)(ServiceStatus*);
typedef NetworkStatusConnection
    (*MonitorNetworkStatusType)(NetworkMonitor, void*);

CrosVersionCheckType CrosVersionCheck = 0;

MonitorPowerStatusType MonitorPowerStatus = 0;
DisconnectPowerStatusType DisconnectPowerStatus = 0;
RetrievePowerInformationType RetrievePowerInformation = 0;

ConnectToWifiNetworkType ConnectToWifiNetwork = 0;
GetAvailableNetworksType GetAvailableNetworks = 0;
FreeServiceStatusType FreeServiceStatus = 0;
MonitorNetworkStatusType MonitorNetworkStatus = 0;

char const * const kCrosDefaultPath = "/opt/google/chrome/chromeos/libcros.so";

// TODO(rtc/davemoore/seanparent): Give the caller some mechanism to help them
// understand why this call failed.
bool LoadCros(const char* path_to_libcros) {
  if (!path_to_libcros)
    return false;

  void* handle = ::dlopen(path_to_libcros, RTLD_NOW);
  if (handle == NULL)
    return false;
  
  CrosVersionCheck =
      CrosVersionCheckType(::dlsym(handle, "ChromeOSCrosVersionCheck"));

  if (!CrosVersionCheck)
    return false;

  if (!CrosVersionCheck(chromeos::kCrosAPIVersion))
    return false;

  MonitorPowerStatus = MonitorPowerStatusType(
      ::dlsym(handle, "ChromeOSMonitorPowerStatus"));

  DisconnectPowerStatus = DisconnectPowerStatusType(
      ::dlsym(handle, "ChromeOSDisconnectPowerStatus"));

  RetrievePowerInformation = RetrievePowerInformationType(
      ::dlsym(handle, "ChromeOSRetrievePowerInformation"));

  ConnectToWifiNetwork = ConnectToWifiNetworkType(
      ::dlsym(handle, "ChromeOSConnectToWifiNetwork"));

  GetAvailableNetworks = GetAvailableNetworksType(
      ::dlsym(handle, "ChromeOSGetAvailableNetworks"));

  FreeServiceStatus = FreeServiceStatusType(
      ::dlsym(handle, "ChromeOSFreeServiceStatus"));

  MonitorNetworkStatus =
      MonitorNetworkStatusType(::dlsym(handle, "ChromeOSMonitorNetworkStatus"));

  return MonitorPowerStatus
      && DisconnectPowerStatus
      && RetrievePowerInformation
      && ConnectToWifiNetwork
      && GetAvailableNetworks
      && FreeServiceStatus
      && MonitorNetworkStatus;
}

}  // namespace chromeos

