// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos_mount.h" // NOLINT

#include <glog/logging.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "util.h" // NOLINT
#include "chromeos/dbus/dbus.h"
#include "chromeos/glib/object.h"

namespace chromeos { // NOLINT

const char* kDeviceKitDisksInterface =
    "org.freedesktop.DeviceKit.Disks";
const char* kDeviceKitDeviceInterface =
    "org.freedesktop.DeviceKit.Disks.Device";
const char* kDeviceKitPropertiesInterface =
    "org.freedesktop.DBus.Properties";

extern "C"
MountStatus* ChromeOSRetrieveMountInformation();

namespace {  // NOLINT

// Creates a new MountStatus instance populated with the contents from
// a vector of DiskStatus.
MountStatus* CopyFromVector(const std::vector<DiskStatus>& services) {
  MountStatus* result = new MountStatus();
  if (services.size() == 0) {
    result->disks = NULL;
  } else {
    result->disks = new DiskStatus[services.size()];
  }
  result->size = services.size();
  std::copy(services.begin(), services.end(), result->disks);
  return result;
}

bool DeviceIsRemoveable(const dbus::BusConnection& bus, dbus::Proxy& proxy) {
  bool ispartition = false;
  if (!dbus::RetrieveProperty(proxy,
                              kDeviceKitDeviceInterface,
                              "device-is-partition",
                              &ispartition)) {
    DLOG(WARNING) << "unable to determine if device is a partition, bailing";
    return false;
  }
  if (ispartition) {
    glib::Value obj;
    if (!dbus::RetrieveProperty(proxy,
                                kDeviceKitDeviceInterface,
                                "partition-slave",
                                &obj)) {
      return false;
    }

    const char* parent = static_cast<const char*>(::g_value_get_boxed(&obj));
    dbus::Proxy parentproxy(bus,
                            kDeviceKitDisksInterface,
                            parent,
                            kDeviceKitPropertiesInterface);
    bool removeable = false;
    if (!dbus::RetrieveProperty(parentproxy,
                                kDeviceKitDeviceInterface,
                                "device-is-removable",
                                &removeable)) {
      // Since we should always be able to get this property, if we can't,
      // there is some problem, so we should return null.
      DLOG(WARNING) << "unable to determine if device is removeable";
      return NULL;
    }
    return removeable;
  }
}

bool DeviceIsMounted(const dbus::BusConnection& bus,
                     const dbus::Proxy& proxy,
                     std::string& path,
                     bool* ismounted) {
  bool mounted = false;
  if (!dbus::RetrieveProperty(proxy,
                              kDeviceKitDeviceInterface,
                              "device-is-mounted",
                              &mounted)) {
    DLOG(WARNING) << "unable to determine if device is mounted, bailing";
    return false;
  }
  *ismounted = mounted;
  if (mounted) {
    glib::Value value;
    if (!dbus::RetrieveProperty(proxy,
                                kDeviceKitDeviceInterface,
                                "device-mount-paths",
                                &value)) {
      return false;
    }

    char** paths = static_cast<char**>(g_value_get_boxed(&value));
    // TODO(dhg): This is an array for a reason, try to use it.
    if (paths[0])
      path = paths[0];
  }
  return true;
}

bool MountRemoveableDevice(const dbus::BusConnection& bus, const char* path) {
  dbus::Proxy proxy(bus,
                    kDeviceKitDisksInterface,
                    path,
                    kDeviceKitDeviceInterface);
  glib::ScopedError error;
  char* val;
  const char** options = {NULL};
  if (!::dbus_g_proxy_call(proxy.gproxy(),
                           "FilesystemMount",
                           &Resetter(&error).lvalue(),
                           G_TYPE_STRING, "",
                           G_TYPE_STRV, options,
                           G_TYPE_INVALID,
                           G_TYPE_STRING,
                           &val, G_TYPE_INVALID)) {
    LOG(WARNING) << "Filesystem Mount failed: "
        << (error->message ? error->message : "Unknown Error.");
    return false;
  }

  g_free(val);
  return true;
}

}  // namespace


void DeleteDiskStatusProperties(DiskStatus status) {
  delete status.path;
  delete status.mountpath;
}

extern "C"
void ChromeOSFreeMountStatus(MountStatus* status) {
  if (status == NULL)
    return;
  std::for_each(status->disks,
                status->disks + status->size,
                &DeleteDiskStatusProperties);
  delete [] status->disks;
  delete status;
}

class OpaqueMountStatusConnection {
 public:
  typedef dbus::MonitorConnection<void (const char*)>* ConnectionType;

  OpaqueMountStatusConnection(const MountMonitor& monitor,
                              const dbus::Proxy& mount,
                              void* object)
     : monitor_(monitor),
       object_(object),
       mount_(mount),
       addconnection_(NULL),
       removeconnection_(NULL) {
  }

  void FireEvent(MountEventType evt) {
    MountStatus* info;
    if ((info = ChromeOSRetrieveMountInformation()) != NULL)
      monitor_(object_, *info, evt);
    ChromeOSFreeMountStatus(info);
  }

  static void Added(void* object, const char* device) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    DLOG(INFO) << "device added:" << device;
    self->FireEvent(DISK_ADDED);
  }

  static void Removed(void* object, const char* device) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    DLOG(INFO) << "device removed:" << device;
    self->FireEvent(DISK_REMOVED);
  }

  static void Changed(void* object, const char* device) {
    MountStatusConnection self = static_cast<MountStatusConnection>(object);
    DLOG(INFO) << "device changed" << device;
    self->FireEvent(DISK_CHANGED);
  }

  ConnectionType& addedconnection() {
    return addconnection_;
  }

  ConnectionType& removedconnection() {
    return removeconnection_;
  }

  ConnectionType& changedconnection() {
    return changedconnection_;
  }

 private:
  MountMonitor monitor_;
  void* object_;
  dbus::Proxy mount_;
  ConnectionType addconnection_;
  ConnectionType removeconnection_;
  ConnectionType changedconnection_;
};

extern "C"
MountStatusConnection ChromeOSMonitorMountStatus(MountMonitor monitor,
                                                 void* object) {
  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy mount(bus,
                    kDeviceKitDisksInterface,
                    "/org/freedesktop/DeviceKit/Disks",
                    kDeviceKitDisksInterface);
  MountStatusConnection result =
     new OpaqueMountStatusConnection(monitor, mount, object);
  ::dbus_g_proxy_add_signal(mount.gproxy(),
                            "DeviceAdded",
                            DBUS_TYPE_G_OBJECT_PATH,
                             G_TYPE_INVALID);
  ::dbus_g_proxy_add_signal(mount.gproxy(),
                            "DeviceRemoved",
                            DBUS_TYPE_G_OBJECT_PATH,
                            G_TYPE_INVALID);
  ::dbus_g_proxy_add_signal(mount.gproxy(),
                            "DeviceChanged",
                            DBUS_TYPE_G_OBJECT_PATH,
                            G_TYPE_INVALID);

  result->addedconnection() = dbus::Monitor(mount, "DeviceAdded",
      &OpaqueMountStatusConnection::Added, result);
  result->removedconnection() = dbus::Monitor(mount, "DeviceRemoved",
      &OpaqueMountStatusConnection::Removed, result);
  result->changedconnection() = dbus::Monitor(mount, "DeviceChanged",
      &OpaqueMountStatusConnection::Changed, result);

  return result;
}

extern "C"
void ChromeOSDisconnectMountStatus(MountStatusConnection connection) {
  dbus::Disconnect(connection->addedconnection());
  dbus::Disconnect(connection->removedconnection());
  dbus::Disconnect(connection->changedconnection());
  delete connection;
}

extern "C"
MountStatus* ChromeOSRetrieveMountInformation() {
  typedef glib::ScopedPtrArray<const char*> ScopedPtrArray;
  typedef ScopedPtrArray::iterator iterator;

  ScopedPtrArray devices;

  dbus::BusConnection bus = dbus::GetSystemBusConnection();
  dbus::Proxy mount(bus,
                    kDeviceKitDisksInterface,
                    "/org/freedesktop/DeviceKit/Disks",
                    kDeviceKitDisksInterface);
  if (!dbus::CallPtrArray(mount, "EnumerateDevices", &devices)) {
    DLOG(WARNING) << "Could not enumerate disk devices.";
    return NULL;
  }
  std::vector<DiskStatus> buffer;
  for (iterator currentpath = devices.begin();
       currentpath < devices.end();
       ++currentpath) {
    dbus::Proxy proxy(bus,
                      kDeviceKitDisksInterface,
                      *currentpath,
                      kDeviceKitPropertiesInterface);
    if (DeviceIsRemoveable(bus, proxy)) {
      DiskStatus info = {};
      bool ismounted = false;
      std::string path;
      info.path = NewStringCopy(*currentpath);
      if (DeviceIsMounted(bus, proxy, path, &ismounted)) {
        if (ismounted) {
          info.mountpath = NewStringCopy(path.c_str());
        } else {
          MountRemoveableDevice(bus, *currentpath);
        }
      }
      buffer.push_back(info);
    }
  }
  return CopyFromVector(buffer);
}

}  // namespace chromeos
