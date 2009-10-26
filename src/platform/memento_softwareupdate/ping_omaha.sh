#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

source `dirname "$0"`/memento_updater_logging.sh

# Example Omaha ping and response (converted to 80 char width w/ backslashes):

# <?xml version="1.0" encoding="UTF-8"?>
# <o:gupdate xmlns:o="http://www.google.com/update2/request" \
# version="Keystone-1.0.5.0" protocol="2.0" \
# machineid="{177255303f3cc519182a103069489327}" ismachine="0" \
# userid="{706F576A-ACF9-4611-B608-E5528EAC106A}">
#     <o:os version="MacOSX" platform="mac" sp="10.5.6_i486"></o:os>
#     <o:app appid="com.google.GoogleAppEngineLauncher" version="1.2.2.380" \
# lang="en-us" brand="GGLG">
#         <o:ping active="0"></o:ping>
#         <o:updatecheck></o:updatecheck>
#     </o:app>
# </o:gupdate>

# Response (converted to 80 char width w/ backslashes):

# <?xml version="1.0" encoding="UTF-8"?><gupdate \
# xmlns="http://www.google.com/update2/response" protocol="2.0"><app \
# appid="com.google.GoogleAppEngineLauncher" status="ok"><ping \
# status="ok"/><updatecheck status="noupdate"/></app></gupdate>

# If you change version= above to "0.0.0.0", you get (again, 80 chars w/ \s):

# <?xml version="1.0" encoding="UTF-8"?><gupdate \
# xmlns="http://www.google.com/update2/response" protocol="2.0"><app \
# appid="com.google.GoogleAppEngineLauncher" status="ok"><ping \
# status="ok"/><updatecheck DisplayVersion="1.2.2.0" \
# MoreInfo="http://appenginesdk.appspot.com" Prompt="true" \
# codebase="http://googleappengine.googlecode.com/files/GoogleAppEngine\
# Launcher-1.2.2.dmg" hash="vv8ifTj79KivBMTsCDsgKPpsmOo=" needsadmin="false" \
# size="4018650" status="ok"/></app></gupdate>

# Parameters of the update request:
OS=Memento
PLATFORM=memento
APP_ID={87efface-864d-49a5-9bb3-4b050a7c227a}
APP_VERSION="$1"
OS_VERSION=${APP_VERSION}_$(uname -m)
LANG=en-us
BRAND=GGLG

OMAHA_ID_FILE=/mnt/stateful_partition/etc/omaha_id
if [ ! -f "$OMAHA_ID_FILE" ]
then
  # omaha file isn't a regular file
  if [ -e "$OMAHA_ID_FILE" ]
  then
    # but the omaha file does exist. delete it
    rm -rf "$OMAHA_ID_FILE"
  fi
  # Generate Omaha ID:
  dd if=/dev/urandom bs=16 count=1 status=noxfer | xxd -c 32 -g 1 -u | \
      cut -d ' ' -f 2-17 | awk \
      '{print "{" $1 $2 $3 $4 "-" $5 $6 "-" $7 $8 "-" $9 $10 "-" \
      $11 $12 $13 $14 $15 $16 "}"; }' > "$OMAHA_ID_FILE"
  chmod 0444 "$OMAHA_ID_FILE"
fi

MACHINE_ID=$(cat "$OMAHA_ID_FILE")
if [ "x" = "x$MACHINE_ID" ]
then
  log missing Omaha ID and unable to generate one
  exit 1
fi
USER_ID=$MACHINE_ID
AU_VERSION=MementoSoftwareUpdate-0.1.0.0
APP_TRACK=$(grep ^CHROMEOS_RELEASE_TRACK /mnt/stateful_partition/etc/lsb-release | \
            cut -d = -f 2-)
if [ "x" = "x$APP_TRACK" ]
then
  # look in the main file
  APP_TRACK=$(grep ^CHROMEOS_RELEASE_TRACK /etc/lsb-release | cut -d = -f 2-)
fi

AUSERVER_URL=$(grep ^CHROMEOS_AUSERVER /etc/lsb-release | cut -d = -f 2-) 

if [ "x" = "x$AUSERVER_URL" ]
then
  AUSERVER_URL="https://tools.google.com/service/update2"
  log using default update server
fi

# for testing. Uncomment and use these to reproduce the examples above
# OS=MacOSX
# PLATFORM=mac
# OS_VERSION=10.5.6_i486
# APP_ID=com.google.GoogleAppEngineLauncher
# #APP_VERSION=0.0.0.0
# APP_VERSION=1.2.2.380
# LANG=en-us
# BRAND=GGLG
# MACHINE_ID={177255303f3cc519182a103069489327}
# USER_ID={706F576A-ACF9-4611-B608-E5528EAC106A}
# AU_VERSION=Keystone-1.0.5.0

# post file must be a regular file for wget:
POST_FILE=/tmp/memento_au_post_file
cat > "/tmp/memento_au_post_file" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<o:gupdate xmlns:o="http://www.google.com/update2/request" \
version="$AU_VERSION" protocol="2.0" machineid="$MACHINE_ID" \
ismachine="0" userid="$USER_ID">
    <o:os version="$OS" platform="$PLATFORM" sp="$OS_VERSION"></o:os>
    <o:app appid="$APP_ID" version="$APP_VERSION" lang="$LANG" brand="$BRAND" \
track="$APP_TRACK">
        <o:ping active="0"></o:ping>
        <o:updatecheck></o:updatecheck>
    </o:app>
</o:gupdate>
EOF

log sending this request to omaha at $AUSERVER_URL
cat "$POST_FILE" >> "$MEMENTO_AU_LOG"

RESPONSE=$(wget -q --header='Content-Type: text/xml' \
           --post-file="$POST_FILE" -O - $AUSERVER_URL)

rm -f "$POST_FILE"

log got response:
log "$RESPONSE"

# parse response
CODEBASE=$(expr match "$RESPONSE" '.* codebase="\([^"]*\)"')
HASH=$(expr match "$RESPONSE" '.* hash="\([^"]*\)"')
SIZE=$(expr match "$RESPONSE" '.* size="\([^"]*\)"')

if [ -z "$CODEBASE" ]
then
  log No update
  exit 0
fi

# We need to make sure that we download updates via HTTPS, but we can skip
# this check for developer builds.
DEVSERVER_URL=$(grep ^CHROMEOS_DEVSERVER /etc/lsb-release | cut -d = -f 2-) 
if [ "x" = "x$DEVSERVER_URL" ]
then
  HTTPS_CODEBASE=$(expr match "$CODEBASE" '\(https://.*\)')
  if [ -z "$HTTPS_CODEBASE" ]
  then
    log No https url
    exit 0
  fi
fi


echo URL=$CODEBASE
echo HASH=$HASH
echo SIZE=$SIZE
