// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Example client which exercises the DBus Cryptohome interfaces.
#include <iostream>

#include <base/basictypes.h>
#include <base/command_line.h>
#include <base/logging.h>
#include <base/string_util.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>

#include "cryptohome/bindings/client.h"

namespace switches {
  static const char kActionSwitch[] = "action";
  static const char *kActions[] = { "mount", "unmount", "is_mounted", NULL };
  enum ActionEnum { ACTION_MOUNT, ACTION_UNMOUNT, ACTION_MOUNTED };
}  // namespace switches

int main(int argc, char **argv) {
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);

  CommandLine *cl = CommandLine::ForCurrentProcess();
  std::string action = cl->GetSwitchValueASCII(switches::kActionSwitch);
  g_type_init();
  chromeos::dbus::BusConnection bus = chromeos::dbus::GetSystemBusConnection();
  chromeos::dbus::Proxy proxy(bus,
                              cryptohome::kCryptohomeServiceName,
                              cryptohome::kCryptohomeServicePath,
                              cryptohome::kCryptohomeInterface);
  DCHECK(proxy.gproxy()) << "Failed to acquire proxy";

  if (!strcmp(switches::kActions[switches::ACTION_MOUNT], action.c_str())) {
    gboolean done = false;
    static const gchar *kUser = "chromeos-user";
    static const gchar *kKey = "274146c6e8886a843ddfea373e2dc71b";
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    if (!org_chromium_CryptohomeInterface_mount(proxy.gproxy(),
                                                kUser,
                                                kKey,
                                                &done,
                                                errptr)) {
      LOG(FATAL) << "Mount call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "Mount did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_UNMOUNT],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    gboolean done = false;
    if (!org_chromium_CryptohomeInterface_unmount(proxy.gproxy(),
                                                  &done,
                                                  errptr)) {
      LOG(FATAL) << "Unmount call failed: " << error->message;
    }
    LOG_IF(ERROR, !done) << "Unmount did not complete?";
    LOG_IF(INFO, done) << "Call completed";
  } else if (!strcmp(switches::kActions[switches::ACTION_MOUNTED],
                     action.c_str())) {
    chromeos::glib::ScopedError error;
    GError **errptr = &chromeos::Resetter(&error).lvalue();
    gboolean done = false;
    if (!org_chromium_CryptohomeInterface_is_mounted(proxy.gproxy(),
                                                     &done,
                                                     errptr)) {
      LOG(FATAL) << "IsMounted call failed: " << error->message;
    }
    std::cout << done << std::endl;

  } else {
    LOG(FATAL) << "Unknown action or no action given (mount,unmount)";
  }
  return 0;
}
