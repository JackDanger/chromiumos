// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dlfcn.h>
#include <stdlib.h>

const char kPamModulePath[] = "/lib/security/pam_google.so";
const char kPamFunction[] = "pam_sm_authenticate";

int main(int argc, char** argv) {
  void *dlhandle;
  dlhandle = dlopen(kPamModulePath, RTLD_NOW);
  if (!dlhandle)
    exit(1);

  // We want to know that the symbol is non-NULL in the module, not just that
  // it is defined.
  if (!dlsym(dlhandle, kPamFunction))
    exit(2);

  return EXIT_SUCCESS;
}
