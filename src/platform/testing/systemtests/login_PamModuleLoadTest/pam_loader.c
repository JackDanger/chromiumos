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

  // Since dlsym can return NULL is the symbol is defined in the module as
  // equal to NULL, the right way to check for an error is symbol resolution
  // is to clear the last error (if any), call dlsym, and then check if dlerror
  // returns anything post-resolution.
  dlerror();
  dlsym(dlhandle, kPamFunction);
  if (dlerror())
    exit(2);

  return EXIT_SUCCESS;
}
