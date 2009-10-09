#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A test case to ensure video resolution is set to expected values.

status=0
DEFAULTRES="1024x600"
TOKEN="Screen 0"

currentres="$(xrandr | grep "${TOKEN}" | cut -d',' -f2 | awk '{print $2$3$4}')"

if [ "${DEFAULTRES}" != "${currentres}" ]; then
  status=1
  echo "FAIL: $0, curent resolution is: ${currentres}"
fi

exit "${status}"
