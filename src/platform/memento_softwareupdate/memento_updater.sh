#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This is the autoupdater for Memento. When called it consults Omaha to see
# if there's an update available. If so, it downloads it to the other
# partition on the Memento USB stick, then alters the MBR and partitions
# as needed so the next reboot will boot into the newly installed partition.
# Care is taken to ensure that when this exits the USB stick is configured
# to boot into the same partition as before or into the new partition,
# however there may be a small time window when this is not the case. Such a
# window should be about 1 second or less, and we tolerate that since this
# is for testing and not a real autoupdate solution for the long run.

source `dirname "$0"`/memento_updater_logging.sh

# make sure we're root
if [ "root" != $(whoami) ]
then
  echo run this script as root
  exit 1
fi

# check that this script doesn't run concurrently
PID_FILE=/tmp/memento_updater_lock
if [[ -f "$PID_FILE" && ! -d /proc/`cat $PID_FILE` ]]
then
  # process holding lock file is dead. clean up lockfile
  rm -rf "$PID_FILE"
fi

# make sure we're not booted from USB
HAS_INITRD=$(grep ' initrd=' /proc/cmdline | wc -l)
if [ "$HAS_INITRD" = "1" ]
then
  log not updating because we booted from USB
  exit 1
fi

# make sure update hasn't already completed
UPDATED_COMPLETED_FILE="/tmp/memento_autoupdate_completed"
if [ -f "$UPDATED_COMPLETED_FILE" ]
then
  exit 0
fi

if ( set -o noclobber; echo "$$" > "$PID_FILE") 2> /dev/null; 
then
  true
else
  log "Failed to acquire lockfile: $PID_FILE." 
  log "Held by $(cat $PID_FILE)"
  exit 1
fi
# remove lockfile when we exit
trap 'rm -f "$PID_FILE"; log Memento AutoUpdate terminating; exit $?' \
    INT TERM EXIT

log Memento AutoUpdate starting

# Get local version
APP_VERSION=$(grep ^CHROMEOS_RELEASE_VERSION \
              /mnt/stateful_partition/etc/lsb-release | \
              cut -d = -f 2-)
if [ "x" = "x$APP_VERSION" ]
then
  # look in the main file
  APP_VERSION=$(grep ^CHROMEOS_RELEASE_VERSION \
                /etc/lsb-release | cut -d = -f 2-)
fi

# See if we're forcing an update from a specific URL
if [ "x" = "x$1" ]
then
  # abort if autoupdates have been disabled, but only when an update image
  # isn't forced
  UPDATES_DISABLED_FILE="/var/local/disable_software_update"
  if [ -f "$UPDATES_DISABLED_FILE" ]
  then
    log Updates disabled. Aborting.
    exit 0
  fi

  # check w/ omaha to see if there's an update
  OMAHA_CHECK_OUTPUT=$(`dirname "$0"`/ping_omaha.sh $APP_VERSION)
  IMG_URL=$(echo "$OMAHA_CHECK_OUTPUT" | grep '^URL=' | cut -d = -f 2-)
  CHECKSUM=$(echo "$OMAHA_CHECK_OUTPUT" | grep '^HASH=' | cut -d = -f 2-)
else
  log User forced an update from: "$1" checksum: "$2"
  IMG_URL="$1"
  CHECKSUM="$2"
fi

if [[ -z "$IMG_URL" || -z "$CHECKSUM" ]]
then
  log no update
  exit 0
fi
# TODO(adlr): make sure we have enough space for the download. we are
# already correct if we don't have space, but it would be nice to fail
# fast.
log Update Found: $IMG_URL checksum: $CHECKSUM

# figure out which device i'm on, and which to download to.
ROOT_ARG=$(cat /proc/cmdline | tr ' ' '\n' | grep ^root= | cut -d = -f 2-)
LOCAL_DEV="$ROOT_ARG"
# do some device sanity checks
if expr match "$ROOT_ARG" '^UUID=' > /dev/null
then
  LOCAL_DEV=$(readlink -f /dev/disk/by-uuid/${ROOT_ARG#UUID=})
elif expr match "$ROOT_ARG" '^LABEL=' > /dev/null
then
  LOCAL_DEV=$(readlink -f /dev/disk/by-label/${ROOT_ARG#LABEL=})
fi

# we install onto the other dev. so if we end in 1, other ends in 2, and
# vice versa
INSTALL_DEV=$(echo $LOCAL_DEV | tr '1234' '2143')

ROOT_DEV=$(echo $LOCAL_DEV | tr -d '1234')  # strip trailing number

# do some device sanity checks
if ! expr match "$LOCAL_DEV" '^/dev/[a-z][a-z]*[1234]$' > /dev/null
then
  log "didnt find good local device. local: $LOCAL_DEV install: $INSTALL_DEV"
  exit 1
fi
if ! expr match "$INSTALL_DEV" '^/dev/[a-z][a-z]*[1234]$' > /dev/null
then
  log "didnt find good install device. local: $LOCAL_DEV install: $INSTALL_DEV"
  exit 1
fi
if [ "$LOCAL_DEV" == "$INSTALL_DEV" ]
then
  log local and installation device are the same: "$LOCAL_DEV"
  exit 1
fi

log Booted from "$LOCAL_DEV" and installing onto "$INSTALL_DEV"

# make sure installation device is unmounted
if [ "$INSTALL_DEV" == ""$(grep "^$INSTALL_DEV " /proc/mounts | \
                           cut -d ' ' -f 1 | uniq) ]
then
  # drive is mounted. must unmount
  log unmounting "$INSTALL_DEV"
  umount "$INSTALL_DEV"
  # check if it's still mounted for some strange reason
  if [ "$INSTALL_DEV" == ""$(grep "^$INSTALL_DEV " /proc/mounts | \
                             cut -d ' ' -f 1 | uniq) ]
  then
    log unable to unmount "$INSTALL_DEV", which is where i need to write to
    exit 1
  fi
fi

# download file to the device
log downloading image. this may take a while

# wget - fetch file, send to stdout
# tee - save a copy off to device, also send to stdout
# openssl - calculate the sha1 hash of stdin, send checksum to stdout
# tr - convert trailing newline to a space
# pipestatus - append return codes for all prior commands. should all be 0

CHECKSUM_FILE="/tmp/memento_autoupdate_checksum"
RETURNED_CODES=$(wget -O - --load-cookies <(echo "$COOKIES") \
    "$IMG_URL" 2>> "$MEMENTO_AU_LOG" | \
    tee >(openssl sha1 -binary | openssl base64 > "$CHECKSUM_FILE") | \
    gzip -d > "$INSTALL_DEV" ; echo ${PIPESTATUS[*]})

EXPECTED_CODES="0 0 0"
CALCULATED_CS=$(cat "$CHECKSUM_FILE")
rm -f "$CHECKSUM_FILE"

if [[ ("$CALCULATED_CS" == "$CHECKSUM")  && \
      ("$RETURNED_CODES" == "$EXPECTED_CODES") ]]
then
  # wonderful
  log download success
else
  # either checksum mismatch or ran out of space.
  log checksum mismatch or other error \
      calculated checksum: "$CALCULATED_CS" reference checksum: "$CHECKSUM" \
      return codes: "$RETURNED_CODES" expected codes: "$EXPECTED_CODES"
  # zero-out installation partition
  dd if=/dev/zero of=$INSTALL_DEV bs=4096 count=1
  exit 1
fi

# Return 0 if $1 > $2.
# $1 and $2 are in "a.b.c.d" format where a, b, c, and d are base 10.
function version_number_greater_than {
  # Replace periods with spaces and strip off leading 0s (lest numbers be
  # interpreted as octal).
  REPLACED_A=$(echo "$1" | sed -r 's/(^|\.)0*/ /g')
  REPLACED_B=$(echo "$2" | sed -r 's/(^|\.)0*/ /g')
  EXPANDED_A=$(printf '%020d%020d%020d%020d' $REPLACED_A)
  EXPANDED_B=$(printf '%020d%020d%020d%020d' $REPLACED_B)
  # This is a string compare:
  [[ "$EXPANDED_A" > "$EXPANDED_B" ]]
}

# it's best not to interrupt the script from this point on out, since it
# should really be doing these things atomically. hopefully this part will
# run rather quickly.

# tell the new image to make itself "ready"
log running postinst on the downloaded image
MOUNTPOINT=/tmp/newpart
mkdir -p "$MOUNTPOINT"
mount "$INSTALL_DEV" "$MOUNTPOINT"

# Check version of new software
NEW_VERSION=$(grep ^GOOGLE_RELEASE "$MOUNTPOINT"/etc/lsb-release | \
              cut -d = -f 2-)
if [ "x$NEW_VERSION" = "x" ]
then
  log "Can't find new version number. aborting update"
  umount "$MOUNTPOINT"
  rmdir "$MOUNTPOINT"
  exit 1
else
  # See if it's newer than us
  if version_number_greater_than "$APP_VERSION" "$NEW_VERSION"
  then
    log "Can't upgrade to older version: " "$NEW_VERSION"
    umount "$MOUNTPOINT"
    rmdir "$MOUNTPOINT"
    exit 1
  fi
fi

"$MOUNTPOINT"/postinst "$INSTALL_DEV" 2>&1 | cat >> "$MEMENTO_AU_LOG"; \
  [ "${PIPESTATUS[*]}" = "0 0" ]
POSTINST_RETURN_CODE=$?
umount "$MOUNTPOINT"
rmdir "$MOUNTPOINT"

# $1 is return code, $2 is command
function abort_update_if_cmd_failed_long {
  if [ "$1" -ne "0" ]
  then
    log "$2 failed with error code  $1 . aborting update"
    exit 1
  fi
}

function abort_update_if_cmd_failed {
  abort_update_if_cmd_failed_long "$?" "!!"
}

# if it failed, don't update MBR but just to be safe, zero out a page of
# install device
abort_update_if_cmd_failed_long "$POSTINST_RETURN_CODE" "$MOUNTPOINT"/postinst

# postinstall on new partition succeeded.
# fix up MBR and make our own partition not something casper will find

# update MBR to make the other partition bootable
# the slash-magic converts '/' -> '\/' so it's valid in a regex
log updating MBR of usb device

# flush linux caches; seems to be necessary
sync
echo 3 > /proc/sys/vm/drop_caches

LAYOUT_FILE=/tmp/new_partition_layout
sfdisk -d $ROOT_DEV | sed -e "s/${INSTALL_DEV//\//\\/} .*[^e]$/&, bootable/" \
                          -e "s/\(${LOCAL_DEV//\//\\/} .*\), bootable/\1/" \
  > "$LAYOUT_FILE"
sfdisk --force $ROOT_DEV < "$LAYOUT_FILE" 2>&1 | cat >> "$MEMENTO_AU_LOG"
abort_update_if_cmd_failed

# mark update as complete so we don't try to update again
touch "$UPDATED_COMPLETED_FILE"

# tell user to reboot
log Autoupdate applied. You should now reboot
