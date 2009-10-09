#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This testcase will check our system logs for some key words that signify
# an issue has occured.

rc=0

declare -a keys=( fatal oops panic segfault )
declare -a logs=( kern.log syslog dmesg )

for log in ${logs[@]}
do
  for key in ${keys[@]}
    do
      if sudo grep -i $key /var/log/${log}; then
        echo $key found in $log
        rc=1
      fi
    done
done

if test "$rc" -eq 0; then
  echo "PASS: $0"
else
  echo "FAIL: $0"
fi

exit "${rc}"
