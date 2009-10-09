#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A test case to ensure that the sshd daemon is not running.

status=0
DAEMON="sshd"

ps -eaf | grep "${DAEMON}" | grep -v grep
found=$?
if [ "${found}" -eq 0 ]; then
  echo "Error: ${DAEMON} found in memory!"
  echo "FAIL: $0"
  status=1
else
  echo "PASS: $0"
fi

exit "${status}"
