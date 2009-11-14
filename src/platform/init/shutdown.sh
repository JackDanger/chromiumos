#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Shutdown is best-effort. We don't want to die on errors.
set +e

# For a given mountpoint, this will kill all processes with open files
# on that mountpoint so that it can be unmounted. It starts off by sending
# a TERM and if the process hasn't exited quickly enough it will send KILL.
#
# Since a typical shutdown should have no processes with open files on a
# partition that we care about at this point, we log the set of processes
# to /var/log/shutdown_force_kill_processes
kill_with_open_files_on() {
  PIDS=$(lsof -t $@ | sort -n | uniq)
  if [ -z "$PIDS" ] ; then
    return  # The typical case; no open files at this point.
  fi

  # PIDS should have been empty. Since it is not, we log for future inspection.
  lsof $@ > /var/log/shutdown_force_kill_processes

  # First try a gentle kill -TERM
  for i in 1 2 3 4 5 6 7 8 9 10; do
    for pid in $PIDS ; do
      ! kill -TERM $pid
    done
    PIDS=$(lsof -t $@ | sort -n | uniq)
    if [ -z "$PIDS" ] ; then
      return
    fi
    sleep .1
  done

  # Now kill -KILL as necessary
  PIDS=$(lsof -t $@ | sort -n | uniq)
  for i in 1 2 3 4 5 6 7 8 9 10; do
    for pid in $PIDS ; do
      ! kill -KILL $pid
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

# Unmount our stateful mount points. If any of these fail we do a best-effort
# attempt to touch a /var/log file for future inspection.
/usr/sbin/umount.cryptohome
if [ $? -ne 0 ] ; then
  mount > /var/log/shutdown_cryptohome_umount_failure
fi
umount -n /var/cache /var/log /home /mnt/stateful_partition
if [ $? -ne 0 ] ; then
  mount > /mnt/stateful_partition/var/log/shutdown_stateful_umount_failure
  mount > /var/log/shutdown_stateful_umount_failure
fi

# Just in case something didn't unmount properly above.
sync

# Ensure that we always claim success.
exit 0
