// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/utility.h>

#include <cstring>

namespace chromeos {

void* SecureMemset(void *v, int c, size_t n) {
  volatile unsigned char *p = static_cast<unsigned char *>(v);
  while (n--)
    *p++ = c;
  return v;
}

}  // namespace chromeos
