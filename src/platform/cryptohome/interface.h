// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CRYPTOHOME_INTERFACE_H_
#define CRYPTOHOME_INTERFACE_H_

#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-bindings.h>
#include <glib-object.h>
#include <stdlib.h>

#include <base/logging.h>
#include <chromeos/dbus/dbus.h>
#include <chromeos/glib/object.h>

#include "cryptohome/service.h"

// Helpers for using GObjects until we can get a C++ wrapper going.
namespace cryptohome {
namespace gobject {  // Namespace hiding the GObject type data.

struct Cryptohome {
  GObject parent_instance;
  Service *service;  // pointer to implementing service.
};
struct CryptohomeClass { GObjectClass parent_class; };

// cryptohome_get_type() is defined in interface.cc by the G_DEFINE_TYPE()
// macro.  This macro defines a number of other GLib class system specific
// functions and variables discussed in interface.cc.
GType cryptohome_get_type();  // defined by G_DEFINE_TYPE

// Interface function prototypes which wrap service.
gboolean cryptohome_is_mounted(Cryptohome *self,
                               gboolean *out_is_mounted,
                               GError **error);
gboolean cryptohome_mount(Cryptohome *self,
                          gchar *userid,
                          gchar *key,
                          gboolean *OUT_done,
                          GError **error);
gboolean cryptohome_unmount(Cryptohome *self,
                            gboolean *out_done,
                            GError **error);
}  // namespace gobject
}  // namespace cryptohome
#endif  // CRYPTOHOME_INTERFACE_H_

