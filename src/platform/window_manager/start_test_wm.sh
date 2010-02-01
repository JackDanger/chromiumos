#!/bin/sh
# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script launches the window manager on the local X server
# running on display :1.0 on a regular Ubuntu machine.  You need to
# start the other X server from a pty that isn't in an xterm with the
# command "xinit -- :1".  You have to build wm from within the chroot
# environment using scons on the command line (not using the building
# scripts).

# This script is only for use during development to get some rapid
# feedback without having to install on a device.

# By default, this script runs WITHOUT clutter, at least for now.

script_dir=$PWD/$(dirname $0)

export LD_LIBRARY_PATH=/lib32:/usr/lib32:$script_dir/../../../chroot/usr/lib:$LD_LIBRARY_PATH
export DISPLAY=:1.0

echo "Switch to the X Server running on display $DISPLAY NOW!"
echo "4..."
sleep 1
echo "3..."
sleep 1
echo "2..."
sleep 1
echo "1..."
sleep 1
echo "Starting wm!"

$script_dir/wm --logtostderr --wm_background_image ../assets/images/background_1024x600.png --use_tidy "$@"
