// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_network.h"  // NOLINT

#include <glog/logging.h>
#include <algorithm>
#include <cstring>

#include "marshal.h"  // NOLINT
#include "chromeos/dbus/dbus.h"  // NOLINT
#include "chromeos/glib/object.h"  // NOLINT

// TODO(rtc): Unittest this code as soon as the tree is refactored.
namespace chromeos {  // NOLINT

namespace { // NOLINT

// Connman D-Bus service identifiers.
const char* kConnmanManagerInterface = "org.moblin.connman.Manager";
const char* kConnmanServiceInterface = "org.moblin.connman.Service";
const char* kConnmanServiceName = "org.moblin.connman";

// Connman function names.
const char* kGetPropertiesFunction = "GetProperties";
const char* kConnectServiceFunction = "ConnectService";

// Connman property names.
const char* kEncryptionProperty = "Security";
const char* kPassphraseRequiredProperty = "PassphraseRequired";
const char* kServicesProperty = "Services";
const char* kSignalStrengthProperty = "Strength";
const char* kSsidProperty = "Name";
const char* kStateProperty = "State";
const char* kTypeProperty = "Type";
const char* kUnknownString = "UNKNOWN";

// Connman type options.
const char* kTypeEthernet = "ethernet";
const char* kTypeWifi = "wifi";
const char* kTypeWimax = "wimax";
const char* kTypeBluetooth = "bluetooth";
const char* kTypeCellular = "cellular";

// Connman state options.
const char* kStateIdle = "idle";
const char* kStateCarrier = "carrier";
const char* kStateAssociation = "association";
const char* kStateConfiguration = "configuration";
const char* kStateReady = "ready";
const char* kStateDisconnect = "disconnect";
const char* kStateFailure = "failure";

// Connman encryption options.
const char* kWpaEnabled = "wpa";
const char* kWepEnabled = "wep";
const char* kRsnEnabled = "rsn";

// Invokes the given method on the proxy and stores the result
// in the ScopedHashTable. The hash table will map strings to glib values.
bool GetProperties(const dbus::Proxy& proxy, glib::ScopedHashTable* result) {
  glib::ScopedError error;
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           kGetPropertiesFunction,
                           &Resetter(&error).lvalue(),
                           G_TYPE_INVALID,
                           ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           &Resetter(result).lvalue(), G_TYPE_INVALID)) {
    LOG(WARNING) << "GetProperties failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

// TODO(rtc): Forked from chromeos_power.cc. This needs to be moved to
// a common location. I'll do this once we have the tree setup the way we
// want it.
char* NewStringCopy(const char* x) {
  char* result = static_cast<char*>(::operator new(std::strlen(x) + 1));
  std::strcpy(result, x);  // NOLINT
  return result;
}

ConnectionType ParseType(const std::string& type) {
  if (type == kTypeEthernet)
    return TYPE_ETHERNET;
  if (type == kTypeWifi)
    return TYPE_WIFI;
  if (type == kTypeWimax)
    return TYPE_WIMAX;
  if (type == kTypeBluetooth)
    return TYPE_BLUETOOTH;
  if (type == kTypeCellular)
    return TYPE_CELLULAR;
  return TYPE_UNKNOWN;
}

ConnectionState ParseState(const std::string& state) {
  if (state == kStateIdle)
    return STATE_IDLE;
  if (state == kStateCarrier)
    return STATE_CARRIER;
  if (state == kStateAssociation)
    return STATE_ASSOCIATION;
  if (state == kStateConfiguration)
    return STATE_CONFIGURATION;
  if (state == kStateReady)
    return STATE_READY;
  if (state == kStateDisconnect)
    return STATE_DISCONNECT;
  if (state == kStateFailure)
    return STATE_FAILURE;
  return STATE_UNKNOWN;
}

EncryptionType ParseEncryptionType(const std::string& encryption) {
  if (encryption == kRsnEnabled)
    return RSN;
  if (encryption == kWpaEnabled)
    return WPA;
  if (encryption == kWepEnabled)
    return WEP;
  return NONE;
}

// Populates an instance of a ServiceInfo with the properties
// from a Connman service.
void ParseServiceProperties(const glib::ScopedHashTable& properties,
                            ServiceInfo* info) {
  const char* default_string = kUnknownString;
  properties.Retrieve(kTypeProperty, &default_string);
  info->type = ParseType(default_string);

  default_string = kUnknownString;
  properties.Retrieve(kSsidProperty, &default_string);
  info->ssid = NewStringCopy(default_string);

  default_string = kUnknownString;
  properties.Retrieve(kStateProperty, &default_string);
  info->state = ParseState(default_string);

  default_string = kUnknownString;
  properties.Retrieve(kEncryptionProperty, &default_string);
  info->encryption = ParseEncryptionType(default_string);

  bool default_bool = false;
  properties.Retrieve(kPassphraseRequiredProperty, &default_bool);
  info->needs_passphrase = default_bool;

  uint8 default_uint8 = 0;
  properties.Retrieve(kSignalStrengthProperty, &default_uint8);
  info->signal_strength = default_uint8;
}

// Returns a ServiceInfo object populated with data from a
// given DBus object path.
//
// returns true on success.
bool ParseServiceInfo(const char* path, ServiceInfo *info) {
  dbus::Proxy service_proxy(dbus::GetSystemBusConnection(),
                            kConnmanServiceName,
                            path,
                            kConnmanServiceInterface);
  glib::ScopedHashTable service_properties;
  if (!GetProperties(service_proxy, &service_properties))
    return false;
  ParseServiceProperties(service_properties, info);
  return true;
}

// Creates a new ServiceStatus instance populated with the contents from
// a vector of ServiceInfo.
ServiceStatus* CopyFromVector(const std::vector<ServiceInfo>& services) {
  ServiceStatus* result = new ServiceStatus();
  if (services.size() == 0) {
    result->services = NULL;
  } else {
    result->services = new ServiceInfo[services.size()];
  }
  result->size = services.size();
  std::copy(services.begin(), services.end(), result->services);
  return result;
}

// Deletes all of the heap allocated members of a given ServiceInfo instance.
void DeleteServiceInfoProperties(ServiceInfo info) {
  delete info.ssid;
}

ServiceStatus* GetServiceStatus(const GPtrArray* array) {
  std::vector<ServiceInfo> buffer;
  // TODO(rtc/seanparent): Think about using std::transform instead of this
  // loop. For now I think the loop will be generally more readable than
  // std::transform().
  const char* path = NULL;
  for (int i = 0; i < array->len; i++) {
    path = static_cast<const char*>(g_ptr_array_index(array, i));
    ServiceInfo info = {};
    if (!ParseServiceInfo(path, &info))
      continue;
    buffer.push_back(info);
  }
  return CopyFromVector(buffer);
}

}  // namespace

extern "C"
void ChromeOSFreeServiceStatus(ServiceStatus* status) {
  if (status == NULL)
    return;
  std::for_each(status->services,
                status->services + status->size,
                &DeleteServiceInfoProperties);
  delete [] status->services;
  delete status;
}

class OpaqueNetworkStatusConnection {
 public:
  typedef dbus::MonitorConnection<void(const char*, const glib::Value*)>*
      ConnectionType;

  OpaqueNetworkStatusConnection(const dbus::Proxy& proxy,
                                const NetworkMonitor& monitor,
                                void* object)
     : proxy_(proxy),
       monitor_(monitor),
       object_(object),
       connection_(NULL) {
  }

  static void Run(void* object,
                  const char* property,
                  const glib::Value* value) {
    OpaqueNetworkStatusConnection* self =
        static_cast<OpaqueNetworkStatusConnection*>(object);
    if (strcmp("Services", property) != 0) {
      return;
    }

    ::GPtrArray *services =
        static_cast< ::GPtrArray *>(::g_value_get_boxed(value));
    if (services->len == 0) {
      LOG(INFO) << "Signal sent without path.";
      return;
    }
    ServiceStatus* status = GetServiceStatus(services);
    self->monitor_(self->object_, *status);
    ChromeOSFreeServiceStatus(status);
  }

  ConnectionType& connection() {
    return connection_;
  }

 private:
  dbus::Proxy proxy_;
  NetworkMonitor monitor_;
  void* object_;
  ConnectionType connection_;
};

extern "C"
NetworkStatusConnection ChromeOSMonitorNetworkStatus(NetworkMonitor monitor,
                                                     void* object) {
  // TODO(rtc): Figure out where the best place to init the marshaler is, also
  // it may need to be freed.
  dbus_g_object_register_marshaller(marshal_VOID__STRING_BOXED,
                                    G_TYPE_NONE,
                                    G_TYPE_STRING,
                                    G_TYPE_VALUE,
                                    G_TYPE_INVALID);
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy proxy(bus, kConnmanServiceName, "/", kConnmanManagerInterface);
  NetworkStatusConnection result =
      new OpaqueNetworkStatusConnection(proxy, monitor, object);
  result->connection() = dbus::Monitor(
      proxy, "PropertyChanged", &OpaqueNetworkStatusConnection::Run, result);
  return result;
}

extern "C"
bool ChromeOSConnectToWifiNetwork(const char* ssid,
                                  const char* passphrase,
                                  const char* encryption) {
  if (ssid == NULL)
    return false;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable scoped_properties =
      glib::ScopedHashTable(
          ::g_hash_table_new_full(::g_str_hash,
                                  ::g_str_equal,
                                  ::g_free,
                                  NULL));

  glib::Value value_mode("managed");
  glib::Value value_type("wifi");
  glib::Value value_ssid(ssid);
  glib::Value value_passphrase(passphrase == NULL ? "" : passphrase);
  glib::Value value_security(encryption == NULL ? "rsn" : encryption);

  ::GHashTable* properties = scoped_properties.get();
  ::g_hash_table_insert(properties, ::g_strdup("Mode"), &value_mode);
  ::g_hash_table_insert(properties, ::g_strdup("Type"), &value_type);
  ::g_hash_table_insert(properties, ::g_strdup("SSID"), &value_ssid);
  ::g_hash_table_insert(properties, ::g_strdup("Passphrase"),
      &value_passphrase);
  ::g_hash_table_insert(properties, ::g_strdup("Security"), &value_security);

  glib::ScopedError error;
  ::DBusGProxy obj;
  if (!::dbus_g_proxy_call(manager_proxy.gproxy(),
                           kConnectServiceFunction,
                           &Resetter(&error).lvalue(),
                           ::dbus_g_type_get_map("GHashTable",
                                                 G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           properties,
                           G_TYPE_INVALID,
                           DBUS_TYPE_G_PROXY,
                           &obj,
                           G_TYPE_INVALID)) {
    LOG(WARNING) << "ConnectService failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

extern "C"
ServiceStatus* ChromeOSGetAvailableNetworks() {
  typedef glib::ScopedPtrArray<const char*> ScopedPtrArray;
  typedef ScopedPtrArray::iterator iterator;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy manager_proxy(bus,
                            kConnmanServiceName,
                            "/",
                            kConnmanManagerInterface);

  glib::ScopedHashTable properties;
  if (!GetProperties(manager_proxy, &properties)) {
    return NULL;
  }

  GHashTable* table = properties.get();
  gpointer ptr = g_hash_table_lookup(table, kServicesProperty);
  if (ptr == NULL)
    return NULL;

  std::vector<ServiceInfo> buffer;
  // TODO(seanparent): See if there is a cleaner way to implement this.
  GPtrArray* service_value =
      static_cast<GPtrArray*>(g_value_get_boxed(static_cast<GValue*>(ptr)));
  return GetServiceStatus(service_value);
}

}  // namespace chromeos

