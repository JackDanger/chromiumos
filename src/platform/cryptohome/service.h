// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_SERVICE_H_
#define CRYPTOHOME_SERVICE_H_

#include <dbus/dbus-glib.h>
#include <glib-object.h>

#include <base/logging.h>
#include <chromeos/dbus/abstract_dbus_service.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/dbus/service_constants.h>
#include <chromeos/glib/object.h>

namespace cryptohome {
namespace gobject {
struct Cryptohome;
}  // namespace gobject

// Service
// Provides a wrapper for exporting CryptohomeInterface to
// D-Bus and entering the glib run loop.
//
// ::g_type_init() must be called before this class is used.
//
// TODO(wad) make an AbstractDbusService class which wraps a placeholder
//           GObject struct that references the service. Then subclass to
//           implement and push the abstract to common/chromeos/dbus/
class Service : public chromeos::dbus::AbstractDbusService {
 public:
  Service();
  virtual ~Service();

  // From chromeos::dbus::AbstractDbusService
  // Setup the wrapped GObject and the GMainLoop
  virtual bool Initialize();
  virtual bool Reset();

  // Used internally during registration to set the
  // proper service information.
  virtual const char *service_name() const
    { return kCryptohomeServiceName; }
  virtual const char *service_path() const
    { return kCryptohomeServicePath; }
  virtual const char *service_interface() const
    { return kCryptohomeInterface; }
  virtual GObject* service_object() const {
    return G_OBJECT(cryptohome_.get());
  }
  // Command-related accesors
  virtual const char *mount_command() const
    { return mount_command_; }
  virtual void set_mount_command(const char *cmd) { mount_command_ = cmd; }
  virtual const char *unmount_command() const
    { return unmount_command_; }
  virtual void set_unmount_command(const char *cmd) { unmount_command_ = cmd; }
  virtual const char *is_mounted_command() const
    { return is_mounted_command_; }
  virtual void set_is_mounted_command(const char *cmd)
    { is_mounted_command_ = cmd; }

  static const char *kDefaultMountCommand;
  static const char *kDefaultUnmountCommand;
  static const char *kDefaultIsMountedCommand;

  // Service implementation functions as wrapped in interface.cc
  // and defined in cryptohome.xml.
  virtual gboolean IsMounted(gboolean *OUT_is_mounted, GError **error);
  virtual gboolean Mount(gchar *user,
                         gchar *key,
                         gboolean *OUT_done,
                         GError **error);
  virtual gboolean Unmount(gboolean *OUT_done, GError **error);

 protected:
  virtual GMainLoop *main_loop() { return loop_; }

 private:
  GMainLoop *loop_;
  scoped_ptr<gobject::Cryptohome> cryptohome_;
  const char *mount_command_;
  const char *unmount_command_;
  const char *is_mounted_command_;
  DISALLOW_COPY_AND_ASSIGN(Service);
};

}  // cryptohome
#endif  // CRYPTOHOME_SERVICE_H_
