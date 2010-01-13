// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/abstract_dbus_service.h>
#include <base/logging.h>

namespace chromeos {
namespace dbus {

bool AbstractDbusService::Register(const chromeos::dbus::BusConnection &conn) {
  return RegisterExclusiveService(conn,
                                  service_interface(),
                                  service_name(),
                                  service_path(),
                                  service_object());
}

bool AbstractDbusService::Run() {
  if (!main_loop()) {
    LOG(ERROR) << "No run loop. Call Initialize before use.";
    return false;
  }
  ::g_main_loop_run(main_loop());
  DLOG(INFO) << "Run() completed";
  return true;
}

bool AbstractDbusService::Shutdown() {
  ::g_main_loop_quit(main_loop());
  return true;
}

}  // namespace dbus
}  // namespace chromeos
