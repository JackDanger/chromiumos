#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

LOOP=
if [ "$1" = '--loop' ]; then LOOP=1; fi

CHROME_DIR="/opt/google/chrome"
CHROME="$CHROME_DIR/chrome"
BOTTLE="$CHROME_DIR/bottle.sh"
COOKIE_PIPE="/tmp/cookie_pipe"
DISABLE_CHROME_RESTART="/tmp/disable_chrome_restart"
SEND_METRICS="/etc/send_metrics"
USER_DATA_DIR="${HOME}/${CHROMEOS_USER}/.config/google-chrome"

"$BOTTLE" "Booting..." &

# The first time a user runs chrome, we use some initial arguments
# to set up their environment. From then on, chrome session restore
# will take care of opening the tabs they want.
FIRST_RUN_ARGS=""
CLIENTSSL_AUTH_ALLOW_ARGS=""
# Determine user's domain.
# TODO: reliably determine the correct domain for all users.
DOMAIN="${CHROMEOS_USER#*@}"

if [ ! -d "$USER_DATA_DIR" ]; then
  mkdir -p "$USER_DATA_DIR"
  if [ -f "$SEND_METRICS" ]; then
    # Automatically opt-in to Chrome OS stats collecting.  This does
    # not have to be a cryptographically random string, but we do need
    # a 32 byte, printable string.
    head -c 8 /dev/random | openssl md5 > \
        "${USER_DATA_DIR}/Consent To Send Stats"
  fi

  # Give appropriate stuff to the users based on their domain.
  if [ $DOMAIN = "google.com" ]; then
    FIRST_RUN_ARGS="--pinned-tab-count=3 \
                    http://welcome-cros.appspot.com \
                    https://mail.google.com/a/google.com \
                    https://calendar.google.com/a/google.com \
                    chrome://newtab"
  elif [ $DOMAIN = "gmail.com" ]; then
    FIRST_RUN_ARGS="--pinned-tab-count=2 \
                    https://mail.google.com/mail \
                    https://calendar.google.com \
                    chrome://newtab"
  else
    # TODO: handle other domains.
    FIRST_RUN_ARGS="--pinned-tab-count=2 \
                    https://mail.google.com/mail \
                    https://calendar.google.com \
                    chrome://newtab"
  fi
fi

# Allow Chromium to automatically send available SSL client certificates 
# to a server. Until a client certificate UI is available in Chromium,
# this is automatically enabled for internal users.
if [ $DOMAIN = "google.com" ]; then
    CLIENTSSL_AUTH_ALLOW_ARGS="--auto-ssl-client-auth"
fi

# We want to pass user login credentials to chrome through the cookie
# pipe, but only for the first invocation of chrome since boot.
COOKIE_PIPE_ARG=""
if [ -p "$COOKIE_PIPE" ]; then
  # TODO: If we quote $COOKIE_PIPE, then chrome ignores the argument. Why?
  COOKIE_PIPE_ARG="--cookie-pipe=$COOKIE_PIPE"
fi

while true; do
  "$BOTTLE" "Starting Chrome" &
  "$CHROME" --enable-plugins \
            --enable-gview \
            --no-first-run $COOKIE_PIPE_ARG  \
            --user-data-dir="$USER_DATA_DIR" \
            $FIRST_RUN_ARGS \
            $CLIENTSSL_AUTH_ALLOW_ARGS
            
  # After the first launch skip the cookie pipe and first run args.
  rm -f "$COOKIE_PIPE"
  COOKIE_PIPE_ARG=""
  FIRST_RUN_ARGS=""

  # Only loop if --loop is supplied and auto-restart isn't disabled.
  if test -f "$DISABLE_CHROME_RESTART" -o -z "$LOOP"; then
    break
  fi
done
