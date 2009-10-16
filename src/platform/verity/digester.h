// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __CHROMEOS_VERITY_DIGESTER_H
#define __CHROMEOS_VERITY_DIGESTER_H

#include <sys/types.h>

namespace chromeos {
namespace verity {

// Digester
// Abstract class providing cryptographic hash digest
// functionality to Verity.
class Digester {
 public:
  Digester();
  virtual ~Digester();
  virtual bool Initialize() = 0;
  virtual bool Check(const char *data,
                     size_t length,
                     const char *expected_digest) = 0;
  virtual bool Compute(const char *data,
                       size_t length,
                       char *hexdigest,
                       unsigned int available) = 0;
};

}  // namespace verity
}  // namespace chromeos

#endif  // __CHROMEOS_VERITY_DIGESTER_H
