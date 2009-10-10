#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

dpi=${dpi:-105}
display=${display:-:1}
resolution=${resolution:-1000x720}

xinit ./xinit.sh -- /usr/bin/Xephyr $display -ac -br \
  -dpi $dpi -screen $resolution -host-cursor
