// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <base/logging.h>

namespace chromeos {
namespace dbus {

bool CallPtrArray(const Proxy& proxy,
                  const char* method,
                  glib::ScopedPtrArray<const char*>* result) {
  glib::ScopedError error;

  ::GType g_type_array = ::dbus_g_type_get_collection("GPtrArray",
                                                       DBUS_TYPE_G_OBJECT_PATH);


  if (!::dbus_g_proxy_call(proxy.gproxy(), method, &Resetter(&error).lvalue(),
                           G_TYPE_INVALID, g_type_array,
                           &Resetter(result).lvalue(), G_TYPE_INVALID)) {
    LOG(WARNING) << "CallPtrArray failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  return true;
}

BusConnection GetSystemBusConnection() {
  glib::ScopedError error;
  ::DBusGConnection* result = ::dbus_g_bus_get(DBUS_BUS_SYSTEM,
                                               &Resetter(&error).lvalue());
  CHECK(result);
  // Set to not exit when when system bus is disconnected.
  // This fixes the problem where when the dbus daemon is stopped, exit is
  // called which kills Chrome.
  ::dbus_connection_set_exit_on_disconnect(
      ::dbus_g_connection_get_connection(result), FALSE);
  return BusConnection(result);
}

BusConnection GetPrivateBusConnection(const char* address) {
  // Since dbus-glib does not have an API like dbus_g_connection_open_private(),
  // we have to implement our own.

  // We have to call _dbus_g_value_types_init() to register standard marshalers
  // just like as dbus_g_bus_get() and dbus_g_connection_open() do, but the
  // function is not exported. So we call GetPrivateBusConnection() which calls
  // dbus_g_bus_get() here instead. Note that if we don't call
  // _dbus_g_value_types_init(), we might get "WARNING **: No demarshaller
  // registered for type xxxxx" error and might not be able to handle incoming
  // signals nor method calls.
  GetSystemBusConnection();

  ::DBusGConnection* result = NULL;
  ::DBusConnection* raw_connection
        = ::dbus_connection_open_private(address, NULL);
  CHECK(raw_connection);

  ::dbus_connection_setup_with_g_main(raw_connection, NULL);
  // A reference count of |raw_connection| is transferred to |result|. You don't
  // have to (and should not) unref the |raw_connection|.
  result = ::dbus_connection_get_g_connection(raw_connection);
  CHECK(result);

  ::dbus_connection_set_exit_on_disconnect(
      ::dbus_g_connection_get_connection(result), FALSE);

  // TODO(yusukes): We should call dbus_connection_close() for private
  // connections.
  return BusConnection(result);
}

bool RetrieveProperties(const Proxy& proxy,
                        const char* interface,
                        glib::ScopedHashTable* result) {
  glib::ScopedError error;

  if (!::dbus_g_proxy_call(proxy.gproxy(), "GetAll", &Resetter(&error).lvalue(),
                           G_TYPE_STRING, interface, G_TYPE_INVALID,
                           ::dbus_g_type_get_map("GHashTable", G_TYPE_STRING,
                                                 G_TYPE_VALUE),
                           &Resetter(result).lvalue(), G_TYPE_INVALID)) {
    LOG(WARNING) << "RetrieveProperties failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }
  return true;
}

/* static */
Proxy::value_type Proxy::GetGProxy(const BusConnection& connection,
                                   const char* name,
                                   const char* path,
                                   const char* interface,
                                   bool connect_to_name_owner) {
  value_type result = NULL;
  if (connect_to_name_owner) {
    glib::ScopedError error;
    result = ::dbus_g_proxy_new_for_name_owner(connection.object_,
                                               name,
                                               path,
                                               interface,
                                               &Resetter(&error).lvalue());
    if (!result) {
      LOG(ERROR) << "Failed to construct proxy: "
                 << (error->message ? error->message : "Unknown Error")
                 << ": " << path;
    }
  } else {
    result = ::dbus_g_proxy_new_for_name(connection.object_,
                                         name,
                                         path,
                                         interface);
    if (!result) {
      LOG(ERROR) << "Failed to construct proxy: " << path;
    }
  }
  return result;
}

}  // namespace dbus
}  // namespace chromeos
