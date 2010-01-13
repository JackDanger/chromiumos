// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cryptohome/interface.h"
#include "cryptohome/service.h"

#include <stdio.h>
#include <stdlib.h>

#include <chromeos/dbus/dbus.h>

// Forcibly namespace the dbus-bindings generated server bindings instead of
// modifying the files afterward.
namespace cryptohome {  // NOLINT
namespace gobject {  // NOLINT
#include "cryptohome/bindings/server.h"
}  // namespace gobject
}  // namespace cryptohome

namespace cryptohome {

const char *Service::kDefaultMountCommand = "/usr/sbin/mount.cryptohome";
const char *Service::kDefaultUnmountCommand = "/usr/sbin/umount.cryptohome";
const char *Service::kDefaultIsMountedCommand =
  "/bin/mount | /bin/grep -q /dev/mapper/cryptohome";

Service::Service() : loop_(NULL),
                     cryptohome_(NULL),
                     mount_command_(kDefaultMountCommand),
                     unmount_command_(kDefaultUnmountCommand),
                     is_mounted_command_(kDefaultIsMountedCommand) { }
Service::~Service() { }

bool Service::Initialize() {
  // Install the type-info for the service with dbus.
  dbus_g_object_type_install_info(gobject::cryptohome_get_type(),
                                  &gobject::dbus_glib_cryptohome_object_info);
  return Reset();
}

bool Service::Reset() {
  cryptohome_.reset(NULL);  // Make sure the destructor is run first.
  cryptohome_.reset(reinterpret_cast<gobject::Cryptohome*>(
                     g_object_new(gobject::cryptohome_get_type(), NULL)));
  // Allow references to this instance.
  cryptohome_->service = this;

  if (loop_) {
    ::g_main_loop_unref(loop_);
  }
  loop_ = g_main_loop_new(NULL, false);
  if (!loop_) {
    LOG(ERROR) << "Failed to create main loop";
    return false;
  }
  return true;
}

gboolean Service::IsMounted(gboolean *OUT_is_mounted, GError **error) {
  int status = system(is_mounted_command());
  *OUT_is_mounted = !status;
  return TRUE;
}

gboolean Service::Mount(gchar *userid,
                        gchar *key, 
                        gboolean *OUT_done,
                        GError **error) {
  // Never double mount.
  // This will mean we don't mount over existing mounts,
  // but at present, we reboot on user change so it is ok.
  // Later, this can be more intelligent.
  if (IsMounted(OUT_done, error) && *OUT_done) {
    *OUT_done = FALSE;
    return TRUE;
  }
  // Note, by doing this we allow any chronos caller to
  // send a variable for use in the scripts. Bad idea.
  // Thankfully, this will go away with the scripts.
  setenv("CHROMEOS_USER", userid, 1);
  FILE *mount = popen(mount_command(), "w");
  if (!mount) {
    // TODO(wad) *error = 
    return FALSE;
  }
  fprintf(mount, "%s", key);
  int status = pclose(mount);
  if (status != 0) {
    *OUT_done = FALSE;
  } else {
    *OUT_done = TRUE;
  }
  return TRUE;
}

gboolean Service::Unmount(gboolean *OUT_done, GError **error) {
  // Check for a mount first.
  if (IsMounted(OUT_done, error) && *OUT_done) {
    *OUT_done = FALSE;
    return TRUE;
  }

  int status = system(unmount_command());
  *OUT_done = !status;
  return TRUE;
}

}  // namespace cryptohome
