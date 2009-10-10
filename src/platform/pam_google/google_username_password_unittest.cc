// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for GoogleUsernamePassword.

#include "pam_google/offline_credential_store.h"
#include "pam_google/google_username_password.h"
#include <curl/curl.h>
#include <gtest/gtest.h>
#include <string.h>

namespace chromeos_pam {
const char kFakeUser[] = "fakeuser";
const char kFakePass[] = "fakepass";

class GoogleUsernamePasswordTest : public ::testing::Test { };

TEST(GoogleUsernamePasswordTest, MemoryZeroTest) {
  int zerolen = strlen(kFakePass)+1;
  char *zeros = new char[zerolen];
  memset(zeros, 0, zerolen);
  GoogleUsernamePassword *up = new GoogleUsernamePassword(
      kFakeUser, strlen(kFakeUser), kFakePass, strlen(kFakePass),
      NULL, true);
  char *password = up->password_;
  delete up;
  EXPECT_EQ(0, memcmp(zeros, password, zerolen));
  free(zeros);
  delete password;
}

TEST(GoogleUsernamePasswordTest, UrlencodeNoopTest) {
  GoogleUsernamePassword up(kFakeUser, strlen(kFakeUser),
                            kFakePass, strlen(kFakePass), NULL);
  const char test_string[] = "JustPlainAscii";
  int buflen = 3*strlen(test_string) + 1;
  char *buffer = new char[buflen];
  int bytes_written = up.Urlencode(test_string, buffer, buflen);
  EXPECT_EQ(strlen(test_string), bytes_written);
  EXPECT_EQ(0, strncmp(test_string, buffer, bytes_written));
  delete [] buffer;
}

TEST(GoogleUsernamePasswordTest, UrlencodeTest) {
  GoogleUsernamePassword up(kFakeUser, strlen(kFakeUser),
                            kFakePass, strlen(kFakePass), NULL);
  const char test_string[] = "Needs URL //3|\\|C@d1n6:";
  CURL *curl = curl_easy_init();
  char *urlencoded = curl_easy_escape(curl, test_string, 0);
  int buflen = 3*strlen(test_string) + 1;
  char *buffer = new char[buflen];
  int bytes_written = up.Urlencode(test_string, buffer, buflen);
  EXPECT_EQ(strlen(urlencoded), bytes_written);
  EXPECT_EQ(0, strncmp(urlencoded, buffer, bytes_written));
  curl_free(urlencoded);
  curl_easy_cleanup(curl);
  delete [] buffer;
}

TEST(GoogleUsernamePasswordTest, GetActiveUserTest) {
  char username[80];
  sprintf(username, "%s%s", kFakeUser, "@gmail.com");
  GoogleUsernamePassword up(username, strlen(username),
                          kFakePass, strlen(kFakePass), NULL);
  char active_username[80];
  up.GetActiveUser(active_username, sizeof(active_username));
  EXPECT_EQ(0, strcmp(kFakeUser, active_username));
}

TEST(GoogleUsernamePasswordTest, IsAcceptableTest) {
  char username[80];
  sprintf(username, "%s%s", "foo", "@gmail.com");
  GoogleUsernamePassword up(username, strlen(username),
                            kFakePass, strlen(kFakePass), NULL);
  EXPECT_TRUE(up.IsAcceptable());
  sprintf(username, "%s%s", "foo2", "@gmail.com");
  GoogleUsernamePassword up2(username, strlen(username),
                             kFakePass, strlen(kFakePass), NULL);
  EXPECT_TRUE(up2.IsAcceptable());
}

#ifdef CHROMEOS_PAM_LOCALACCOUNT

TEST(GoogleUsernamePasswordTest, IsLocalAccountTest) {
  GoogleUsernamePassword up(kLocalAccount, strlen(kLocalAccount),
                            kFakePass, strlen(kFakePass), NULL);
  EXPECT_TRUE(up.IsLocalAccount());
}

TEST(GoogleUsernamePasswordTest, LocalAccountIsNotAcceptableTest) {
  GoogleUsernamePassword up(kLocalAccount, strlen(kLocalAccount),
                            kFakePass, strlen(kFakePass), NULL);
  EXPECT_TRUE(up.IsLocalAccount());
  EXPECT_FALSE(up.IsAcceptable());
}

#endif // CHROMEOS_PAM_LOCALACCOUNT

TEST(GoogleUsernamePasswordTest, IsAcceptableFailTest) {
  GoogleUsernamePassword up(kFakeUser, strlen(kFakeUser),
                            kFakePass, strlen(kFakePass), NULL);
  EXPECT_FALSE(up.IsAcceptable());
}

TEST(GoogleUsernamePasswordTest, FormatTest) {
  GoogleUsernamePassword up(kFakeUser, strlen(kFakeUser),
                            kFakePass, strlen(kFakePass), NULL);
  char email[80];
  char password[80];
  char account[80];

  sprintf(email, "Email=%s&", kFakeUser);
  sprintf(password, "Passwd=%s&", kFakePass);
  sprintf(account, "accountType=%s&", kAccountType);

  char buffer[256];
  up.Format(buffer, sizeof(buffer));
  EXPECT_TRUE(NULL != strstr(buffer, email));
  EXPECT_TRUE(NULL != strstr(buffer, password));
  EXPECT_TRUE(NULL != strstr(buffer, account));
}

}  // namespace chromeos_pam
