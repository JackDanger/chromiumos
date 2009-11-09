#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

XAUTH_FILE="/var/run/chromelogin.auth"
SERVER_READY=

user1_handler () {
  echo "received SIGUSR1!"
  SERVER_READY=y
}

trap user1_handler USR1
MCOOKIE=$(head -c 8 /dev/random | openssl md5)
/usr/bin/xauth -q -f ${XAUTH_FILE} add :0 . ${MCOOKIE}

/etc/init.d/xstart.sh ${XAUTH_FILE} $$&

while [ -z ${SERVER_READY} ]; do
  sleep .1
done

# TODO: move this to a more appropriate place, once we actually start
# doing login for real in this code pathway.
/sbin/initctl emit login-prompt-ready &
su chronos -c "/etc/init.d/start_login.sh ${MCOOKIE}"

