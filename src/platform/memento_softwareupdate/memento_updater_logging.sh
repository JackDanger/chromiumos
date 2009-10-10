#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# common logging function for all memento updater scripts

export MEMENTO_AU_LOG=/var/log/softwareupdate.log

function log {
  echo $(/bin/date) "$*" >> "$MEMENTO_AU_LOG"
}
export log
