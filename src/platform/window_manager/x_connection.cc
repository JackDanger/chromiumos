// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/x_connection.h"

#include <glog/logging.h>

namespace chromeos {

const int XConnection::kByteFormat = 8;
const int XConnection::kLongFormat = 32;


bool XConnection::RemapWindowIfMapped(XWindow xid) {
  if (!server_grabbed_) {
    if (!GrabServer())
      return false;
  }

  XWindowAttributes attr;
  if (!GetWindowAttributes(xid, &attr)) {
    if (server_grabbed_)
      CHECK(UngrabServer());
    return false;
  }

  if (attr.map_state != IsUnmapped) {
    if (!UnmapWindow(xid) || !MapWindow(xid)) {
      if (server_grabbed_)
        CHECK(UngrabServer());
      return false;
    }
  }

  if (server_grabbed_)
    CHECK(UngrabServer());
  return true;
}

bool XConnection::GetIntProperty(XWindow xid, XAtom xatom, int* value) {
  CHECK(value);
  vector<int> values;
  if (!GetIntArrayProperty(xid, xatom, &values)) {
    return false;
  }

  CHECK(!values.empty());  // guaranteed by GetIntArrayProperty()
  if (values.size() > 1) {
    LOG(WARNING) << "GetIntProperty() called for property " << xatom
                 << " with " << values.size() << " values; just returning "
                 << "the first";
  }
  *value = values[0];
  return true;
}

bool XConnection::GrabServer() {
  if (server_grabbed_)
    LOG(ERROR) << "Attempting to grab already-grabbed server";
  if (GrabServerImpl()) {
    server_grabbed_ = true;
    return true;
  }
  return false;
}

bool XConnection::UngrabServer() {
  if (!server_grabbed_)
    LOG(ERROR) << "Attempting to ungrab not-grabbed server";
  if (UngrabServerImpl()) {
    server_grabbed_ = false;
    return true;
  }
  return false;
}

bool XConnection::SetIntProperty(
    XWindow xid, XAtom xatom, XAtom type, int value) {
  vector<int> values(1, value);
  return SetIntArrayProperty(xid, xatom, type, values);
}

}  // namespace chromeos
