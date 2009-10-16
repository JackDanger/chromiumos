// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <glog/logging.h>
#include <openssl/evp.h>
#include <sys/types.h>

#include "openssl_digester.h"

namespace chromeos {
namespace verity {

bool OpenSSLDigester::Initialize() {
  OpenSSL_add_all_digests();
  impl_ = EVP_get_digestbyname(algorithm_);
  if (!impl_)
    return false;
  initialized_ = true;
  return true;
}

bool OpenSSLDigester::Check(const char *data,
                            size_t length,
                            const char *expected_digest) {
  if (!initialized_) {
    LOG(ERROR) << "Check called before Initialize()";
    return false;
  }
  char hexdigest[EVP_MAX_MD_SIZE * 2 + 1];
  if (!Compute(data, length, hexdigest, sizeof(hexdigest))) {
    LOG(ERROR) << "Unable to compute digest of given data.";
    return false;
  }
  // expected_digest may not be NUL terminated.
  if (!strncmp(expected_digest, hexdigest, strlen(hexdigest))) {
    DLOG(INFO) << "digest matched: " << hexdigest;
    return true;
  }
  DLOG(INFO) << "digest mismatched (" << hexdigest << ")";
  // expected_digest is not NUL terminated properly.
  // Expected " << expected_digest << " saw " << hexdigest;
  return false;
}

   // hexdigest must be long enough to contain the hash.
bool OpenSSLDigester::Compute(const char *data,
                              size_t length,
                              char *hexdigest,
                              unsigned int available) {
  if (!initialized_) {
    LOG(ERROR) << "Compute called before Initialize()";
    return false;
  }
  EVP_MD_CTX mdctx;
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_length;

  EVP_MD_CTX_init(&mdctx);
  EVP_DigestInit_ex(&mdctx, impl_, NULL);
  EVP_DigestUpdate(&mdctx, data, length);
  EVP_DigestFinal_ex(&mdctx, digest, &digest_length);
  EVP_MD_CTX_cleanup(&mdctx);

  if (available < (2 * digest_length)+1) {
    LOG(ERROR) << "hexdigest available space is too small for this digest.: "
               << (2 * digest_length) + 1;
    return false;
  }
  to_hex(hexdigest, digest, digest_length);
  return true;
}

};  // namespace verity
};  // namespace chromeos
