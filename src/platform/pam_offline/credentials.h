// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Credentials is the interface for objects that wrap up a set
// of credentials with which we can authenticate.  At the moment, the
// only implementation of this class is UsernamePassword.

#ifndef PAM_OFFLINE_CREDENTIALS_H_
#define PAM_OFFLINE_CREDENTIALS_H_

#include "pam_offline/utils.h"

namespace pam_offline {

class Credentials {
 public:
  Credentials();
  virtual ~Credentials();

  // Returns the full user name, including any '@' sign or domain name.
  //
  // Parameters
  //  name_buffer - Output buffer.
  //  length - Amount of space in name_buffer
  //
  virtual void GetFullUsername(char *name_buffer, int length) const = 0;

  // Returns the part of the username before the '@'
  //
  // Parameters
  //  name_buffer - Output buffer.
  //  length - Amount of space in name_buffer
  //
  virtual void GetPartialUsername(char *name_buffer, int length) const = 0;

#ifdef CHROMEOS_PAM_LOCALACCOUNT
  // returns true if we're willing to accept these credentials without
  // talking to Google.
  virtual bool IsLocalAccount() const = 0;
#endif

  // Returns the obfuscated username, used as the name of the directory
  // containing the user's stateful data (and maybe used for other reasons
  // at some point.)
  virtual std::string GetObfuscatedUsername(const Blob &system_salt) const = 0;

  // Returns a "weak hash" of the user's password.  Requires the system
  // salt to compute.
  //
  // This hashes using the same algorithm that pam/pam_google/pam_mount use to
  // get the user's plaintext password passed on to the login session.  The
  // two hashing algorithms must be kept in sync, as the hash is used to derive
  // a passphrase for the master key.
  //
  // Parameters
  //  system_salt - A blob containing the current system salt value.
  //
  // Returns
  //  A std::string containing the weak hash encoded as a hex sequence in ASCII.
  //
  virtual std::string GetPasswordWeakHash(const Blob &system_salt) const = 0;
};

}  // namespace pam_offline

#endif  // PAM_OFFLINE_CREDENTIALS_H_
