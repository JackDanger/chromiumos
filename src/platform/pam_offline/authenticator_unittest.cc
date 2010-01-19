// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UsernamePassword.

#include "pam_offline/authenticator.h"

#include <string.h>  // For memset(), memcpy()
#include <stdlib.h>

#include "gtest/gtest.h"
#include "pam_offline/authenticator.h"
#include "pam_offline/username_password.h"
#include "pam_offline/utils.h"

namespace pam_offline {

using std::string;

const char kImageDir[] = "test_image_dir";
const char kFakeUser[] = "testuser@invalid.domain";

class AuthenticatorTest : public ::testing::Test { };

TEST(AuthenticatorTest, BadInitTest) {
  // create an authenticator that points to an invalid shadow root
  // and make sure it complains
  Authenticator authn("/dev/null");
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      "zero", 4);

  EXPECT_EQ(false, authn.Init());
  EXPECT_EQ(false, authn.TestAllMasterKeys(up));
}

TEST(AuthenticatorTest, GoodDecryptTest0) {
  Authenticator authn(kImageDir);
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      "zero", 4);

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(true, authn.TestAllMasterKeys(up));
}

TEST(AuthenticatorTest, GoodDecryptTest1) {
  Authenticator authn(kImageDir);
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      "one", 3);

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(true, authn.TestAllMasterKeys(up));
}

TEST(AuthenticatorTest, GoodDecryptTest2) {
  Authenticator authn(kImageDir);
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      "two", 3);

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(true, authn.TestAllMasterKeys(up));
}

TEST(AuthenticatorTest, BadDecryptTest) {
  Authenticator authn(kImageDir);
  UsernamePassword up(kFakeUser, strlen(kFakeUser),
                      "bogus", 5);

  EXPECT_EQ(true, authn.Init());
  EXPECT_EQ(false, authn.TestAllMasterKeys(up));
}

} // namespace pam_offline
