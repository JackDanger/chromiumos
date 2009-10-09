#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This test case verifies that we created temp file systems, and that the
# each have writable space. In this case we want to ensure that available
# space is greater than some threshhold value.

MINSIZE=128000
status=0

tempfs=( tmp udev shmfs varrun varlock )
count=${#tempfs[@]}
index=0

while [ "${index}" -lt "${count}" ]; do
  available="$(df -t tmpfs | grep ${tempfs[$index]} | awk '{print $4}')"
  if [ -n "${available:+x}" ]; then
    if [ "${available}" -lt "${MINSIZE}" ]; then
      status=1
      echo "FAIL: $0, ${tempfs[$index]} is less than ${MINSIZE} bytes available"
    fi
  else
    status=1
    echo "FAIL: $0, ${tempfs[$index]} does not exist"
  fi
  index=$[$index+1]
done

exit "${status}"
