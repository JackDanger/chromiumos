// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for Service

#include "cryptohome/service.h"

#include <glib.h>
#include <gtest/gtest.h>

namespace cryptohome {


TEST(ServiceInterfaceTests, NopWrappers) {
  Service service;
  service.Initialize();
  service.set_mount_command("/bin/true");
  service.set_unmount_command("/bin/true");
  gboolean out = FALSE;
  GError *error = NULL;
  service.set_is_mounted_command("/bin/true");
  EXPECT_EQ(TRUE, service.IsMounted(&out, &error));
  EXPECT_EQ(TRUE, out);
  // Change to false so that we can exercise Mount
  // without it failing on a double mount.
  service.set_is_mounted_command("/bin/false");
  char user[] = "chromeos-user";
  char key[] = "274146c6e8886a843ddfea373e2dc71b";
  out = FALSE;
  EXPECT_EQ(TRUE, service.Mount(user, key, &out, &error));
  EXPECT_EQ(TRUE, out);
  // Check double mount detection
  service.set_is_mounted_command("/bin/true");
  out = FALSE;
  EXPECT_EQ(TRUE, service.Mount(user, key, &out, &error));
  EXPECT_EQ(FALSE, out);

  EXPECT_TRUE(service.Unmount(&out, &error));
  EXPECT_EQ(out, TRUE);
  // Check IsMounted tests for unmounting nothing.
  service.set_is_mounted_command("/bin/false");
  EXPECT_TRUE(service.Unmount(&out, &error));
  EXPECT_EQ(out, TRUE);
}

// TODO(wad) setup test fixture to create a temp dir
//           touch files on Mount/Unmount/IsMounted and
//           check for goodness.

}  // namespace cryptohome
