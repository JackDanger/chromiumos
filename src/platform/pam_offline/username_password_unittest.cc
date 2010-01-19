// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePassword.

#include <string.h>  // For memset(), memcpy()
#include <gtest/gtest.h>

#include <string>

#include "pam_offline/username_password.h"
#include "pam_offline/utils.h"

namespace pam_offline {
const char kFakeUser[] = "fakeuser";
const char kFakePass[] = "fakepass";

const char kFakeSystemSalt[] = "012345678901234567890";

class UsernamePasswordTest : public ::testing::Test { };

TEST(UsernamePasswordTest, MemoryZeroTest) {
  int zerolen = strlen(kFakePass)+1;
  char zeros[sizeof(kFakePass)];
  memset(zeros, 0, zerolen);
  UsernamePassword *up = new UsernamePassword(
      kFakeUser, strlen(kFakeUser), kFakePass, strlen(kFakePass), true);
  char *password = up->password_;
  delete up;
  EXPECT_EQ(0, memcmp(zeros, password, zerolen));
  delete password;
}

TEST(UsernamePasswordTest, GetPartialUsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePassword up(username, strlen(username),
                      kFakePass, strlen(kFakePass));
  char partial_username[80];
  up.GetPartialUsername(partial_username, sizeof(partial_username));
  EXPECT_EQ(0, strcmp(kFakeUser, partial_username));
}

TEST(UsernamePasswordTest, GetFullUsernameTest) {
  char username[80];
  snprintf(username, sizeof(username), "%s%s", kFakeUser, "@gmail.com");
  UsernamePassword up(username, strlen(username),
                      kFakePass, strlen(kFakePass));
  char full_username[80];
  up.GetFullUsername(full_username, sizeof(full_username));
  EXPECT_EQ(0, strcmp(username, full_username));
}

TEST(UsernamePasswordTest, GetObfuscatedUsernameTest) {
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      kFakePass, strlen(kFakePass));

  Blob fake_salt(AsciiDecode(kFakeSystemSalt));

  EXPECT_EQ("8a8b96d525830c10a92fdef2394136bd9b0d7217",
            up.GetObfuscatedUsername(fake_salt));
}

TEST(UsernamePasswordTest, GetPasswordWeakHashTest) {
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      kFakePass, strlen(kFakePass));

  Blob fake_salt(AsciiDecode(kFakeSystemSalt));

  EXPECT_EQ("176c1e698b521373d77ce655d2e56a1d",
            up.GetPasswordWeakHash(fake_salt));
}

}  // namespace pam_offline
