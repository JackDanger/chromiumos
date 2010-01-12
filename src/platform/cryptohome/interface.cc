// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "cryptohome/interface.h"

namespace cryptohome {
namespace gobject {

// Register with the glib type system.
// This macro automatically defines a number of functions and variables
// which are required to make cryptohome functional as a GObject:
// - cryptohome_parent_class
// - cryptohome_get_type()
// - dbus_glib_cryptohome_object_info
// It also ensures that the structs are setup so that the initialization
// functions are called in the appropriate way by g_object_new().
G_DEFINE_TYPE(Cryptohome, cryptohome, G_TYPE_OBJECT);

GObject *cryptohome_constructor(GType gtype,
                                guint n_properties,
                                GObjectConstructParam *properties) {
  GObject *obj;
  GObjectClass *parent_class;
  // Instantiate using the parent class, then extend for local properties.
  parent_class = G_OBJECT_CLASS(cryptohome_parent_class);
  obj = parent_class->constructor(gtype, n_properties, properties);

  Cryptohome *cryptohome = reinterpret_cast<Cryptohome *>(obj);
  cryptohome->service = NULL;

  // We don't have any thing we care to expose to the glib class system.
  return obj;
}

void cryptohome_class_init(CryptohomeClass *real_class) {
  // Called once to configure the "class" structure.
  GObjectClass *gobject_class = G_OBJECT_CLASS(real_class);
  gobject_class->constructor = cryptohome_constructor;
}

void cryptohome_init(Cryptohome *self) { }

// TODO(wad) add error messaging
#define CRYPTOHOME_WRAP_METHOD(_NAME, args...) \
  if (!self->service) { \
    return FALSE; \
  } \
  return self->service->_NAME(args, error);

gboolean cryptohome_is_mounted(Cryptohome *self, gboolean *out_is_mounted, GError **error) {
  CRYPTOHOME_WRAP_METHOD(IsMounted, out_is_mounted);
}
gboolean cryptohome_mount(Cryptohome *self,
                          gchar *userid,
                          gchar *key,
                          gboolean *OUT_done,
                          GError **error) {
  CRYPTOHOME_WRAP_METHOD(Mount, userid, key, OUT_done);
}
gboolean cryptohome_unmount(Cryptohome *self,
                            gboolean *out_done,
                            GError **error) {
  CRYPTOHOME_WRAP_METHOD(Unmount, out_done);
}
#undef CRYPTOHOME_WRAP_METHOD

}  // namespace gobject
}  // namespace cryptohome
