// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// UsernamePassword wraps a username/password pair that can be used to
// authenticate a user.

#ifndef PAM_OFFLINE_USERNAME_PASSWORD_H_
#define PAM_OFFLINE_USERNAME_PASSWORD_H_

#include "pam_offline/credentials.h"

#include <string.h>

#include "base/basictypes.h"
#include "gtest/gtest.h"

// Enable local account only if user has specifically requested it
#ifdef CHROMEOS_PAM_LOCALACCOUNT
#include "pam_offline/pam_localaccount.h"
#endif

namespace pam_offline {

class UsernamePassword : public Credentials {
 public:
  UsernamePassword(const char *username, const int username_length,
                   const char *password, const int password_length);
  ~UsernamePassword();

  // From credentials.h...
  void GetFullUsername(char *name_buffer, int length) const;
  void GetPartialUsername(char *name_buffer, int length) const;
  std::string GetObfuscatedUsername(const Blob &system_salt) const;
  std::string GetPasswordWeakHash(const Blob &system_salt) const;

#ifdef CHROMEOS_PAM_LOCALACCOUNT
  // returns true if username_ is the local account (if set up)
  bool IsLocalAccount() const;
#endif

 private:
  // ONLY FOR TESTING.  Allows the caller to tell us not to free the
  // memory we allocate for username_ and password_.
  UsernamePassword(const char *username, const int username_length,
                   const char *password, const int password_length,
                   bool dont_free_memory);

  void Init(const char *username, const int username_length,
            const char *password, const int password_length);

  // NOTE(cmasone): I'm not using strings here because I want to be able
  // to explicitly zero any memory in which the user's password was
  // stored, and be certain that it's not been copied around by some
  // opaque implementation.  I'm avoiding scoped_ptrs for a similar
  // reason.
  //
  // username_ and password_ will be null-terminated.  username_ will be
  // whatever token was required to fully identify the user to their
  // authentication service.  For the typical Google account based login,
  // this will be a full email address.
  char *username_;
  char *password_;

  // ONLY FOR TESTING.  We don't delete password_ in the destructor if
  // this is set, so that the caller can check to make sure we're
  // zeroing the memory we allocate.
  const bool dont_free_memory_;

  FRIEND_TEST(UsernamePasswordTest, MemoryZeroTest);
  FRIEND_TEST(UsernamePasswordFetcherTest, FetchTest);

  DISALLOW_COPY_AND_ASSIGN(UsernamePassword);
};

}  // namespace pam_offline

#endif  // PAM_OFFLINE_USERNAME_PASSWORD_H_
