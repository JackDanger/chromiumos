// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_H_
#define CHROMEOS_NETWORK_H_

#include <base/basictypes.h>

namespace chromeos { // NOLINT

// connection types (see connman/include/service.h)
enum ConnectionType {
  TYPE_UNKNOWN    = 0x00000,
  TYPE_ETHERNET   = 0x00001,
  TYPE_WIFI       = 0x00010,
  TYPE_WIMAX      = 0x00100,
  TYPE_BLUETOOTH  = 0x01000,
  TYPE_CELLULAR   = 0x10000,
};

// connection states (see connman/include/service.h)
enum ConnectionState {
  STATE_UNKNOWN,
  STATE_IDLE,
  STATE_CARRIER,
  STATE_ASSOCIATION,
  STATE_CONFIGURATION,
  STATE_READY,
  STATE_DISCONNECT,
  STATE_FAILURE,
};

enum EncryptionType {
  NONE,
  WEP,
  WPA,
  RSN
};

struct ServiceInfo {
  const char* ssid;
  ConnectionType type;
  ConnectionState state;
  int64 signal_strength;
  bool needs_passphrase;
  EncryptionType encryption;
};

struct ServiceStatus {
  ServiceInfo *services;
  int size;
};

// Connects to a given ssid.
//
// Set passphrase to NULL if the network doesn't require authentication.
//
// Set encryption to NULL if the network doesn't require authentication
// otherwise we will use 'rsn' as the default.
//
// returns false on failure and true on success.
//
// Note, a successful call to this function only indicates that the
// connection process has started. You will have to query the connection state
// to determine if the connection was established successfully.
extern bool (*ConnectToWifiNetwork)(const char* ssid,
                                    const char* passphrase,
                                    const char* encryption);

// Returns a list of all of the available services that a user can connect to.
// The ServiceStatus instance that is returned by this function MUST be
// deleted with by calling FreeServiceStatus.
//
// Returns NULL on error.
extern ServiceStatus* (*GetAvailableNetworks)();

// Deletes a ServiceStatus type that was allocated in the ChromeOS dll. We need
// to do this to safely pass data over the dll boundary between our .so and
// Chrome.
extern void (*FreeServiceStatus)(ServiceStatus* status);

// An internal listener to a d-bus signal. When notifications are received
// they are rebroadcasted in non-glib form.
class OpaqueNetworkStatusConnection;
typedef OpaqueNetworkStatusConnection* NetworkStatusConnection;

// NOTE: The instance of ServiceStatus that is received by the caller will be
// freed once your function returns. Copy this object if you intend to cache it.
//
// The expected callback signature that will be provided by the client who
// calls MonitorNetworkStatus.
typedef void(*NetworkMonitor)(void* object, const ServiceStatus& status);

// Processes a callback from a d-bus signal by finding the path of the
// Connman service that changed and sending the details along to the next
// handler in the chain as an instance of ServiceInfo.
extern NetworkStatusConnection (*MonitorNetworkStatus)(NetworkMonitor, void*);

// Disconnects a NetworkStatusConnection.
extern void (*DisconnectNetworkStatus)(NetworkStatusConnection connection);

// Returns the enabled network devices as a bitwise or value of ConnectionTypes.
//
// Returns 0 on error.
extern int (*GetEnabledNetworkDevices)();

// Enable or disable the specific network device for connection.
//
// Returns false on failure and true on success.
extern bool (*EnableNetworkDevice)(ConnectionType type, bool enable);

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_H_

