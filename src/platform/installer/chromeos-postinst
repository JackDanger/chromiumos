#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is called after an AutoUpdate or USB install.
set -e

# update /boot/extlinux.conf
INSTALL_ROOT=`dirname "$0"`
INSTALL_DEV="$1"

# set default label to chromeos-hd
sed -i 's/^DEFAULT .*/DEFAULT chromeos-hd/' "$INSTALL_ROOT"/boot/extlinux.conf
sed -i "{ s:HDROOT:$INSTALL_DEV: }" "$INSTALL_ROOT"/boot/extlinux.conf

# NOTE: The stateful partition will not be mounted when this is
# called at USB-key install time.
