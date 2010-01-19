// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAM_OFFLINE_AUTHENTICATOR_H_
#define PAM_OFFLINE_AUTHENTICATOR_H_

#include "base/basictypes.h"
#include "pam_offline/credentials.h"
#include "pam_offline/utils.h"

namespace pam_offline {

// System salt and user dirs start here.
extern const std::string kDefaultShadowRoot;

class Authenticator {

 public:
  // Initializes the authenticator with the default shadow root of
  // "/home/.shadow/".
  Authenticator();

  // Initializes the authenticator with an alternative shadow root.  The
  // shadow_root should point to a directory with the system salt and
  // obfuscated user directories.
  //
  // Parameters
  //  shadow_root - A local file system path containing the system salt
  //    and obfuscated user directories.
  //
  explicit Authenticator(const std::string &shadow_root);

  ~Authenticator();

  // Loads the system salt, and anything else that might need to be done.
  // This *must* be called before other methods.
  //
  // Returns false if the initialization fails for some reason.  May also
  // spew LOG messages on failure.
  bool Init();

  // Returns the system salt
  Blob GetSystemSalt() const;

  // "Wraps" the hashed password using the same algorithm as
  // cryptohome::password_to_wrapper.  This encodes the hashed_password in a
  // master key specific salt, resulting in the passphrase for the master
  // key.
  //
  // Parameters
  //  master_salt_file - The local filesystem path to the salt file for the
  //   master password that you intend to decrypt.
  //  hashed_password  - The user's hashed password, as returned by
  //   Credentials::GetPasswordWeakHash.
  //  iters - The number of wrap iterations to perform.  Should be the same
  //   number that were used by the cryptohome script to create the passphrase.
  //
  std::string IteratedWrapHashedPassword(const std::string &master_salt_file,
                                         const std::string &hashed_password,
                                         const int iters) const;

  // Same as above, except with a default iters of 1.
  std::string WrapHashedPassword(const std::string &master_salt_file,
                                 const std::string &hashed_password) const;

  bool TestDecrypt(const std::string passphrase,
                   const Blob salt,
                   const Blob cipher_text) const;

  // Attempts to decrypt a single master key.
  //
  // Parameters
  //  master_key_file - The full local filesystem path to the master key.
  //  hashed_password - The hashed password (as returned by
  //    Credentials.GetPasswordWeakHash)
  //
  bool TestOneMasterKey(const std::string &master_key_file,
                        const std::string &hashed_password) const;

  // Enumerates all of the master keys (master.0, master.1, etc), looking
  // for that can be successfully decrypted with the given credentials.
  //
  // Parameters
  //  credentials - An object representing the user's credentials.
  //
  bool TestAllMasterKeys(const Credentials &credentials) const;

 private:
  std::string shadow_root_;
  Blob system_salt_;

  DISALLOW_COPY_AND_ASSIGN(Authenticator);
};

} // namespace pam_offline

#endif // PAM_OFFLINE_AUTHENTICATOR_H_
