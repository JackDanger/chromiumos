// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_MOUNT_H_
#define CHROMEOS_MOUNT_H_

#include <base/basictypes.h>

namespace chromeos { //NOLINT

struct DiskStatus {
  const char* path;
  const char* mountpath;
};

struct MountStatus {
  DiskStatus *disks;
  int size;
};

enum MountEventType {
  DISK_ADDED,
  DISK_REMOVED,
  DISK_CHANGED
};

// An internal listener to a d-bus signal. When notifications are received
// they are rebroadcasted in non-glib form.
class OpaqueMountStatusConnection;
typedef OpaqueMountStatusConnection* MountStatusConnection;

// NOTE: The instance of MountStatus that is received by the caller will be
// freed once your function returns. Copy this object if you intend to cache it.
//
// The expected callback signature that will be provided by the client who
// calls MonitorMountStatus.
typedef void(*MountMonitor)(void*, const MountStatus&, MountEventType evt);

// Processes a callback from a d-bus signal by finding the path of the
// DeviceKit Disks service that changed and sending the details along to the
// next handler in the chain as an instance of MountStatus.
extern MountStatusConnection (*MonitorMountStatus)(MountMonitor monitor, void*);

// Disconnects a listener from the mounting events.
extern void (*DisconnectMountStatus)(MountStatusConnection connection);

// Returns a list of all the available removeable devices that are found on
// the device.  If the device not mounted, it will be mounted, and an event
// will be sent when the mounting is complete.  The MountStatus returned by
// this function must be deleted by calling FreeMountStatus.
//
// Returns NULL on error.
extern MountStatus* (*RetrieveMountInformation)();

// Deletes a MountStatus type that was allocated in the ChromeOS .so. We need
// to do this to safely pass data over the .so boundary between our .so and
// Chrome.
extern void (*FreeMountStatus)(MountStatus* status);

}  // namespace chromeos

#endif  // CHROMEOS_MOUNT_H_
