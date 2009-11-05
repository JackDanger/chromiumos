#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Shutdown is best-effort. We don't want to die on errors.
set +e

# For a given mountpoint, this will kill all processes with open files
# on that mountpoint so that it can be unmounted.
kill_with_open_files_on() {
  PIDS=$(lsof -t $@ | sort -n | uniq)
  for i in 1 2 3 4 5 6 7 8 9 10; do
    for pid in $PIDS ; do
      ! kill $pid
    done
    PIDS=$(lsof -t $@ | sort -n | uniq)
    if [ -z "$PIDS" ] ; then
      return
    fi
    sleep .1
  done
}

# Remount root in case a developer has remounted it rw for some reason.
mount -n -o remount,ro /

# TODO: swapoff as necessary.

# Kill any that may prevent us from unmounting the stateful partition
# or the crypto-home and then unmount. These should be all that we need
# to unmount for a clean shutdown.
kill_with_open_files_on /mnt/stateful_partition /home/chronos
/usr/sbin/umount.cryptohome
mount -n -o remount,ro /mnt/stateful_partition
umount -n /var/cache /var/log /home /mnt/stateful_partition

# Ensure that we always claim success.
exit 0
