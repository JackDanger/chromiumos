// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/x_connection.h"

#include "base/logging.h"

namespace window_manager {

using std::string;
using std::vector;

const int XConnection::kByteFormat = 8;
const int XConnection::kLongFormat = 32;


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

bool XConnection::GetAtom(const string& name, XAtom* atom_out) {
  vector<string> names;
  names.push_back(name);
  vector<XAtom> atoms;
  if (!GetAtoms(names, &atoms))
    return false;

  CHECK(atoms.size() == 1);
  *atom_out = atoms[0];
  return true;
}

bool XConnection::SetIntProperty(
    XWindow xid, XAtom xatom, XAtom type, int value) {
  vector<int> values(1, value);
  return SetIntArrayProperty(xid, xatom, type, values);
}

}  // namespace window_manager
