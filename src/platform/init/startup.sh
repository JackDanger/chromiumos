#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


HAS_INITRAMFS=0
if [ -d /dev/.initramfs ]
then
  # The initrd will have mounted /sys, /proc, and /dev for us.
  HAS_INITRAMFS=1
else
  mount -n -t sysfs -onodev,noexec,nosuid sysfs /sys
  mount -n -t proc -onodev,noexec,nosuid proc /proc
  mount -n -t tmpfs -omode=0755 udev /dev
fi

# Moblin trick: Disable blinking cursor. Without this a splash screen
# will show a distinct cursor shape even when the cursor is set to none.
echo 0 > /sys/devices/virtual/graphics/fbcon/cursor_blink

# Since we defer udev until later in the boot process, we pre-populate /dev
# with the set of devices needed for X to run.
cp -a -f /lib/udev/devices/* /dev
mknod -m 0600 /dev/initctl p

# Splash screen!
if [ -x /usr/bin/ply-image ]
then
  /usr/bin/ply-image /usr/share/chromeos-assets/images/login_splash.png &
fi

mount -n -t tmpfs tmp /tmp
mount -n -t tmpfs -onosuid,nodev shmfs /dev/shm
mount -n -t devpts -onoexec,nosuid,gid=5,mode=0620 devpts /dev/pts

# Mount our stateful partition
ROOT_DEV=$(sed 's/.*root=\([^ ]*\).*/\1/g' /proc/cmdline)
if [ "${ROOT_DEV#*=}" = "$ROOT_DEV" ]
then
  # We get here if $ROOT doesn't have an = in it.

  # Old installations have system partitions on partitions 1 and 2. They
  # have the stateful partition on partition 4. New installations have
  # partitions 3 and 4 as system partitions and partition 1 as the stateful
  # partition.
  STATE_DEV=$(echo "$ROOT_DEV" | tr 1234 4411)
else
  # $ROOT has an = in it, so we assume it's LABEL= or UUID=. Follow that
  # convention when specifying the stateful partition.
  STATE_DEV="/dev/disk/by-label/C-STATE"
fi
mount -n -t ext3 "$STATE_DEV" /mnt/stateful_partition

# Make sure stateful partition has some basic directories
mkdir -p -m 0755 /mnt/stateful_partition/var/cache
mkdir -p -m 0755 /mnt/stateful_partition/var/log
mkdir -p -m 0755 /mnt/stateful_partition/home
mkdir -p -m 0755 /mnt/stateful_partition/etc
chmod 0755 /mnt/stateful_partition/var

# Default to Pacific timezone if we don't have one set
! ln -s /usr/share/zoneinfo/US/Pacific /mnt/stateful_partition/etc/localtime \
    > /dev/null 2>&1

# Mount some /var directories and /home
mount -n --bind /mnt/stateful_partition/var/cache /var/cache
mount -n --bind /mnt/stateful_partition/var/log /var/log
mount -n -t tmpfs -omode=1777,nodev,noexec,nosuid vartmp /var/tmp
mount -n --bind /mnt/stateful_partition/home /home

mount -n -t tmpfs -omode=0755,nosuid varrun /var/run
touch /var/run/.ramfs  # TODO: Is this needed?
mount -n -t tmpfs -omode=1777,nodev,noexec,nosuid varlock /var/lock
touch /var/lock/.ramfs # TODO: Is this needed?
mount -n -t tmpfs media /media

# Bootchart
if [ \( $HAS_INITRAMFS -eq 0 \) -a \( -d /lib/bootchart \) ]
then
  BC_LOGS=/var/run/bootchart
  BC_HZ=25
  mkdir -p "$BC_LOGS"
  start-stop-daemon --background --start --quiet \
    --exec /lib/bootchart/collector -- $BC_HZ "$BC_LOGS"
fi

# Some things freak out if no hostname is set.
hostname localhost

# create salt for user data dir crypto
mkdir -p /home/.shadow
SALT=/home/.shadow/salt
(test -f "$SALT" || head -c 16 /dev/urandom > "$SALT") &
