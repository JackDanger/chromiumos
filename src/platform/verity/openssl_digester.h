// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __CHROMEOS_VERITY_OPENSSL_DIGESTER_H
#define __CHROMEOS_VERITY_OPENSSL_DIGESTER_H

#include <openssl/evp.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/types.h>
#include <glog/logging.h>

#include "digester.h"

namespace chromeos {
namespace verity {

class OpenSSLDigester : public Digester {
 public:
   OpenSSLDigester(const char *digest_alg) :
     initialized_(false), algorithm_(digest_alg) { }
   ~OpenSSLDigester() { }

   // Sets up the digester context
   bool Initialize();

   // Computes the digest for the given data and compares it to
   // the expected digest.
   bool Check(const char *data, size_t length, const char *expected_digest);

   // Computes the digest of the given data and returns it as hex in hexdigest
   // if there is enough space.
   bool Compute(const char *data,
                size_t length,
                char *hexdigest,
                unsigned int available);
 private:
  // A shabby function to convert an arbitrarily long digest to hex.
  // This needs to be replace FOR SPEED.
  // Note: hexdigest must be 2*digest_length+i long.
  void to_hex(char *hexdigest,
              const unsigned char *digest,
              unsigned int digest_length) {
    for (unsigned int i = 0; i < digest_length ; i++) {
      sprintf(hexdigest+(2*i), "%02x", static_cast<int>(digest[i]));
    }
  }

  bool initialized_;
  const char *algorithm_;
  const EVP_MD *impl_;
};

};  // namespace verity
};  // namespace chromeos

#endif  // __CHROMEOS_VERITY_OPENSSL_DIGESTER_H
