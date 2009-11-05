#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a helper script for logging browser startup events.  Handles
# actually writing data to the disk.



echo "$1: $2; uptime is $3" >> "$4"
# sleep creates extra process entries in bootchart, and is unneeded anyhow.
rm -f $0
