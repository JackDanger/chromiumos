#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is a helper script for logging browser startup events.  Chromium
# can toss a "message in a bottle" out into the stream of startup events
# by forking and execing this script with appropriate arguments, and a
# timestamped entry in the desired log file will be generated.

# Usage: bottle.sh <message>

UPTIME=$(cat /proc/uptime)
TIME=$(date --rfc-3339=ns)
LOGFILE="$HOME/chrome_startup.log"
touch $LOGFILE
chown chronos $LOGFILE
chmod 0666 $LOGFILE
# In order to get the right name to appear in bootchart, we symlink
# to log.sh with the tag that the caller provided.
ln -s /opt/google/chrome/log.sh "/tmp/$1"
exec "/tmp/$1" "$TIME" "$1" "$UPTIME" "$LOGFILE"

