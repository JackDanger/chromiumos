#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# TODO: This is specific to eeepc. Figure out a way to generalize and
# add support for additional hotkeys.

case "$3" in
  00000013)  # Toggle sound
        amixer set Master toggle
        ;;
  00000014)  # Decrease volume
        amixer set Master 5%-
        ;;
  00000015)  # Increase volume
        amixer set Master 5%+
        ;;
esac
