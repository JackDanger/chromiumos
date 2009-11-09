// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GoogleUsernamePassword wraps a username/password pair that can be
// used to authenticate to Google.

#ifndef CHROMEOS_PAM_GOOGLE_USERNAME_PASSWORD_H_
#define CHROMEOS_PAM_GOOGLE_USERNAME_PASSWORD_H_

#include "pam_google/google_credentials.h"
#include <curl/curl.h>
#include <gtest/gtest.h>
#include <string.h>
#include "base/basictypes.h"

#include <string>

// Enable local account only if user has specifically requested it
#ifdef CHROMEOS_PAM_LOCALACCOUNT
#include "pam_google/pam_localaccount.h"
#endif

namespace chromeos_pam {
const char kCookiePersistence[] = "true";
const char kAccountType[] = "HOSTED_OR_GOOGLE";
const char kSource[] = "memento";

class OfflineCredentialStore;

class GoogleUsernamePassword : public GoogleCredentials {
 public:
  GoogleUsernamePassword(const char *username, const int username_length,
                         const char *password, const int password_length,
                         OfflineCredentialStore *store);
  ~GoogleUsernamePassword();

  /**
   * See comment in google_credentials.h
   */
  int Format(char *payload, int length);

  /**
   * See comment in google_credentials.h
   */
  void GetActiveUser(char *name_buffer, int length);

  /**
   * See comment in google_credentials.h
   */
  void GetActiveUserFull(char *name_buffer, int length);

  // returns true if username_ is the local account (if set up)
  bool IsLocalAccount();

  // returns true if username_ is acceptable
  bool IsAcceptable();

  // returns the weak hash from the default OfflineCredentialStore
  void GetWeakHash(char *hash_buffer, int length);

  // stores credentials in the default OfflineCredentialStore
  void StoreCredentials();

  // checks if the credentials match an offline login stored credential
  bool ValidForOfflineLogin();
 private:
  // ONLY FOR TESTING.  Allows the caller to tell us not to free the
  // memory we allocate for username_ and password_.
  GoogleUsernamePassword(const char *username, const int username_length,
                         const char *password, const int password_length,
                         OfflineCredentialStore *store,
                         bool dont_free_memory);

  int Urlencode(const char *data, char *buffer, int length);

  // I'm not using strings here because I want to be able to
  // explicitly zero any memory in which the user's password was
  // stored, and be certain that it's not been copied around by some
  // opaque implementation.  I'm avoiding scoped_ptrs for a similar
  // reason.
  //
  // username_ and password_ will be null-terminated.  username_ will be
  // a valid email address, as that is what consitutes a valid username
  // in Google Accounts.
  char *username_;
  char *password_;

  std::string salt_;
  std::string system_salt_;

  // handle to libcurl, which does urlencoding for us.
  CURL *curl_;

  // ONLY FOR TESTING.  We don't delete password_ in the destructor if
  // this is set, so that the caller can check to make sure we're
  // zeroing the memory we allocate.
  const bool dont_free_memory_;

  OfflineCredentialStore *store_;

  FRIEND_TEST(GoogleUsernamePasswordTest, MemoryZeroTest);
  FRIEND_TEST(GoogleUsernamePasswordTest, UrlencodeTest);
  FRIEND_TEST(GoogleUsernamePasswordTest, UrlencodeNoopTest);
  FRIEND_TEST(UsernamePasswordFetcherTest, FetchTest);
  DISALLOW_COPY_AND_ASSIGN(GoogleUsernamePassword);
};

}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_USERNAME_PASSWORD_H_
