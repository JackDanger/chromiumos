// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file\.

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const int kBadArgCount = 1;
const int kInvalidPath = 2;
const int kExecFailed = 3;

int main(int argc, char** argv) {
  char* valid_paths[] = {
    "/opt/google/memento_updater/memento_updater.sh",
    "/opt/google/memento_updater/ping_omaha.sh",
    NULL
  };

  const int kMaxArgs = 3;

  if ((argc > kMaxArgs) || (argc < 2)) {
    return kBadArgCount;
  }

  int valid = 0;
  for (int i = 0; valid_paths[i]; i++) {
    if (!strcmp(argv[1], valid_paths[i])) {
      valid = 1;
    }
  }
  if (valid == 0)
    return kInvalidPath;

  char* child_argv[3];
  child_argv[0] = strdup(argv[1]);  // leak
  child_argv[1] = (argc == 3) ? strdup(argv[2]) : NULL;  // leak
  child_argv[2] = NULL;
  
  char* envp[2];
  envp[0] = strdup("PATH=/bin:/sbin:/usr/bin:/usr/sbin");
  envp[1] = NULL;

  setuid(0);
  execve(child_argv[0], child_argv, envp);

  // Error if we get to here.
  return kExecFailed;
}