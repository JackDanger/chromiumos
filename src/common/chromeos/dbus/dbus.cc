// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/dbus.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib-bindings.h>
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
  CHECK(result) << (error->message ? error->message : "Unknown Error");
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

bool RegisterExclusiveService(const BusConnection& connection,
                              const char* interface_name,
                              const char* service_name,
                              const char* service_path,
                              GObject* object) {
  CHECK(object);
  CHECK(interface_name);
  CHECK(service_name);
  // Create a proxy to DBus itself so that we can request to become a
  // service name owner and then register an object at the related service path.
  Proxy proxy = chromeos::dbus::Proxy(connection,
                                      DBUS_SERVICE_DBUS,
                                      DBUS_PATH_DBUS,
                                      DBUS_INTERFACE_DBUS);
  // Exclusivity is determined by replacing any existing
  // service, not queuing, and ensuring we are the primary
  // owner after the name is ours.
  guint flags = DBUS_NAME_FLAG_DO_NOT_QUEUE|DBUS_NAME_FLAG_REPLACE_EXISTING;
  glib::ScopedError err;
  guint result = 0;
  // TODO(wad) determine if we are moving away from using generated functions
  if (!org_freedesktop_DBus_request_name(proxy.gproxy(),
                                         service_name,
                                         0,
                                         &result,
                                         &Resetter(&err).lvalue())) {
    LOG(ERROR) << "Unabled to request service name: "
               << (err->message ? err->message : "Unknown Error.");
    return false;
  }

  // Handle the error codes, releasing the name if exclusivity conditions
  // are not met.
  bool needs_release = false;
  if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
    LOG(ERROR) << "Failed to become the primary owner. Releasing . . .";
    needs_release = true;
  }
  if (result == DBUS_REQUEST_NAME_REPLY_EXISTS) {
    LOG(ERROR) << "Service name exists: " << service_name;
    return false;
  } else if (result == DBUS_REQUEST_NAME_REPLY_IN_QUEUE) {
    LOG(ERROR) << "Service name request enqueued despite our flags. Releasing";
    needs_release = true;
  }
  LOG_IF(WARNING, result == DBUS_REQUEST_NAME_REPLY_ALREADY_OWNER)
    << "Service name already owned by this process";
  if (needs_release) {
    if (!org_freedesktop_DBus_release_name(
           proxy.gproxy(),
           service_name,
           &result,
           &Resetter(&err).lvalue())) {
      LOG(ERROR) << "Unabled to release service name: "
                 << (err->message ? err->message : "Unknown Error.");
    }
    DLOG(INFO) << "ReleaseName returned code " << result;
    return false;
  }

  // Determine a path from the service name and register the object.
  dbus_g_connection_register_g_object(connection.g_connection(), 
                                      service_path,
                                      object);
  return true;
}



}  // namespace dbus
}  // namespace chromeos
