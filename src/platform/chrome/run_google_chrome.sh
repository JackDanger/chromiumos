#!/bin/sh

LOOP=
if [ "$1" = '--loop' ]; then LOOP=1; fi

CHROME="/opt/google/chrome/chrome"
COOKIE_PIPE="/tmp/cookie_pipe"
DISABLE_CHROME_RESTART="/tmp/disable_chrome_restart"
USER_DATA_DIR="${HOME}/${CHROMEOS_USER}/.config/google-chrome"

# The first time a user runs chrome, we use some initial arguments
# to set up their environment. From then on, chrome session restore
# will take care of opening the tabs they want.
FIRST_RUN_ARGS=""
if [ ! -d "$USER_DATA_DIR" ]; then
  mkdir -p "$USER_DATA_DIR"
  # Automatically opt-in to Chrome OS stats collecting.
  # TODO: remove after dogfood?
  touch "${USER_DATA_DIR}/Consent To Send Stats"

  # determine the logged in user's domain and give them the appropriate stuff.
  # TODO: reliably determine the correct domain for all users.
  DOMAIN="${CHROMEOS_USER#*@}"
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

# We want to pass user login credentials to chrome through the cookie
# pipe, but only for the first invocation of chrome since boot.
COOKIE_PIPE_ARG=""
if [ -p "$COOKIE_PIPE" ]; then
  # TODO: If we quote $COOKIE_PIPE, then chrome ignores the argument. Why?
  COOKIE_PIPE_ARG="--cookie-pipe=$COOKIE_PIPE"
fi

while true; do
  "$CHROME" --enable-plugins \
            --no-first-run $COOKIE_PIPE_ARG  \
            --user-data-dir="$USER_DATA_DIR" \
            $FIRST_RUN_ARGS

  # After the first launch skip the cookie pipe and first run args.
  rm -f "$COOKIE_PIPE"
  COOKIE_PIPE_ARG=""
  FIRST_RUN_ARGS=""

  # Only loop if --loop is supplied and auto-restart isn't disabled.
  if test -f "$DISABLE_CHROME_RESTART" -o -z "$LOOP"; then
    break
  fi
done
