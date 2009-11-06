#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script gets executed by run_in_xephyr.sh.

# We lose the changes made by these commands if we don't wait until after
# the window manager is already running, since the X server resets its
# state when the last client disconnects.
(sleep 1 && if [ -e $HOME/.Xresources ]; then xrdb -load $HOME/.Xresources; fi)
(sleep 1 && xmodmap -e 'add mod4 = Super_L Super_R')

# It looks like there might be a race condition if we start the terminal at
# the same time as the window manager -- I'm occasionally seeing the WM not
# notice the window.  Sleep for a second so the WM has a chance to start
# first.
(sleep 1 && ./mock_chrome &)
(sleep 2 && xterm &)

# Uncomment to dump all communication between the WM and X server to a file.
#XTRACE="xtrace -n -o /tmp/wm_xtrace.log"

exec $XTRACE ./wm --logtostderr \
  --wm_background_image=../assets/images/background_1024x768.png
