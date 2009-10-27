// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <cstring>

char* NewStringCopy(const char* x) {
  char* result = static_cast<char*>(std::malloc(std::strlen(x) + 1));
  std::strcpy(result, x);  // NOLINT
  return result;
}
