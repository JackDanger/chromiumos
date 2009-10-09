#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This test case ensures that the / filesystem is mounted read only.

status=0

cat /etc/mtab | grep "\/dev\/root" | grep "ro,"
if [ $? -ne 0 ]; then
  status=1
fi

exit "${status}"
