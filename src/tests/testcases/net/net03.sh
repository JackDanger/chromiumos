#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A test case to ensure we can ping a remote host on the internet.

status=0
REMOTEHOST="www.google.com"

ping -c 5 "${REMOTEHOST}"
status=$?
if [ "${status}" -ne 0 ]; then
  echo "Error: could not ping ${REMOTEHOST}"
  echo "FAIL: $0"
else
  echo "PASS: $0"
fi

exit "${status}"
