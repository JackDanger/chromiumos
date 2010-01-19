// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_offline/username_password.h"

#include "openssl/sha.h"
#include "base/logging.h"

#include "chromeos/utility.h"

namespace pam_offline {

using std::string;

UsernamePassword::UsernamePassword(const char *username,
                                   const int username_length,
                                   const char *password,
                                   const int password_length)
    : dont_free_memory_(false) {

  Init(username, username_length, password, password_length);
}

// ONLY FOR TESTING
UsernamePassword::UsernamePassword(const char *username,
                                   const int username_length,
                                   const char *password,
                                   const int password_length,
                                   bool dont_free_memory)
    : dont_free_memory_(dont_free_memory) {
  Init(username, username_length, password, password_length);
}

UsernamePassword::~UsernamePassword() {
  chromeos::SecureMemset(username_, 0, strlen(username_));
  chromeos::SecureMemset(password_, 0, strlen(password_));
  delete [] username_;
  if (!dont_free_memory_) {
    delete [] password_;
  }
}

void UsernamePassword::Init(const char *username,
                            const int username_length,
                            const char *password,
                            const int password_length) {
  username_ = new char[username_length+1];
  password_ = new char[password_length+1];
  strncpy(username_, username, username_length);
  strncpy(password_, password, password_length);
  username_[username_length] = password_[password_length] = 0;
}

void UsernamePassword::GetFullUsername(char *name_buffer, int length) const {
  strncpy(name_buffer, username_, length);
}

void UsernamePassword::GetPartialUsername(char *name_buffer, int length) const {
  char *at_ptr = strrchr(username_, '@');
  *at_ptr = '\0';
  strncpy(name_buffer, username_, length);
  *at_ptr = '@';
}

string UsernamePassword::GetObfuscatedUsername(const Blob &system_salt) const {
  CHECK(username_);

  SHA_CTX ctx;
  unsigned char md_value[SHA_DIGEST_LENGTH];

  SHA1_Init(&ctx);
  SHA1_Update(&ctx, &system_salt[0], system_salt.size());
  SHA1_Update(&ctx, username_, strlen(username_));
  SHA1_Final(md_value, &ctx);

  Blob md_blob(md_value,
               md_value + (SHA_DIGEST_LENGTH * sizeof(unsigned char)));

  return AsciiEncode(md_blob);
}

// This hashes using the same algorithm that pam/pam_google/pam_mount use to
// get the user's plaintext password safely passed on to the login session.
// That means we compute a sha256sum of the ASCII encoded system salt plus the
// plaintext password, ASCII encode the result, and take the first 32 bytes.
// To say that in bash...
//
//    $(cat <(echo -n $(xxd -p "$SYSTEM_SALT_FILE"))
//      <(echo -n "$PASSWORD") | sha256sum | head -c 32)
//
string UsernamePassword::GetPasswordWeakHash(const Blob &system_salt) const {
  CHECK(password_);

  SHA256_CTX sha_ctx;
  unsigned char md_value[SHA256_DIGEST_LENGTH];

  string system_salt_ascii(AsciiEncode(system_salt));

  SHA256_Init(&sha_ctx);
  SHA256_Update(&sha_ctx, system_salt_ascii.c_str(),
                system_salt_ascii.length());
  SHA256_Update(&sha_ctx, password_, strlen(password_));
  SHA256_Final(md_value, &sha_ctx);

  return AsciiEncode(Blob(md_value, md_value + SHA256_DIGEST_LENGTH / 2));
}

}  // namespace pam_offline
