#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

trap '' USR1 TTOU TTIN
exec /usr/bin/X11/X -nolisten tcp vt01 -auth $1
