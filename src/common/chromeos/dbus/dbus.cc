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


}  // namespace dbus
}  // namespace chromeos
