#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called on startup as part of the init scripts. This calls forks off
# a task which will wait some time and then check for an update.

# Exit immediately if it's a dev machine
DEVKIT_URL=$(grep ^CHROMEOS_DEVSERVER /etc/lsb-release | cut -d = -f 2-) 
if [ -n "$DEVKIT_URL" ]; then
  exit 0
fi

if [ -z "$1" ]; then
  SLEEP_TIME=1800  # seconds; 30 min
else
  SLEEP_TIME="$1"
fi

# Boot after 2 min, then try every 30 min
sleep 120  # seconds
# Once at startup, if log file is too big, move it aside
LOG_FILE=/var/log/softwareupdate.log
if [ $(stat -c%s "$LOG_FILE") -gt "5242880" ]; then  # 5MB
  mv "$LOG_FILE" "$LOG_FILE".0
fi
while [ true ]; do
  /opt/google/memento_updater/memento_updater.sh
  sleep "$SLEEP_TIME"
done
