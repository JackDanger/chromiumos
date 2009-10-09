#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A test case to ensure that the wireless device has an IP address.

status=0
WDEV="wlan0"
IFC="/sbin/ifconfig"

ipaddress="$($IFC $WDEV | grep "inet addr:" | awk '{print $2}' | cut -d':' -f2)"

if [ -z "${ipaddress}" ]; then
  echo "Error: ${WDEV} has no ip address!"
  echo "FAIL: $0"
  exit 1
else
  echo "Good: ${WDEV} has ip address ${ipaddress}"
fi

exit "${status}"
