#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# On lid close:
# - lock the screen
export HOME=/home/chronos
/usr/bin/xscreensaver-command -l

# - suspend the cryptohome device
#CRYPTOHOME=/dev/mapper/cryptohome
#/usr/bin/test -b $CRYPTOHOME && /sbin/dmsetup suspend $CRYPTOHOME

# - stop wireless if UP
wlan0isup=`/sbin/ifconfig wlan0 2>/dev/null | /bin/grep UP`
test "$wlan0isup" && /sbin/ifconfig wlan0 down

# - suspend to ram
echo -n mem > /sys/power/state

# On lid open:
# - restore wireless state
test "$wlan0isup" && /sbin/ifconfig wlan0 up

# On lid open:
# - has it such that you don't have to press a key to display lock screen
/usr/bin/xscreensaver-command -deactivate

# - resume cryptohome device
#/usr/bin/test -b $CRYPTOHOME && /sbin/dmsetup resume $CRYPTOHOME
