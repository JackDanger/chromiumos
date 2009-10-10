#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Called on startup as part of the init scripts. This calls forks off
# a task which will wait some time and then check for an update.

if [ "$1" = "start" ]
then
  {
    sleep 120  # seconds
    /opt/google/memento_updater/memento_updater.sh
  } &
fi
