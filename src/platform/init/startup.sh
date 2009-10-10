#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


HAS_INITRAMFS=0
if [ -d /dev/.initramfs ]
then
  HAS_INITRAMFS=1
  ! umount -l /dev
  ! umount -l /sysfs
  ! umount -l /tmp
  mount -n -o remount,rw /
fi

# Mount /proc, /sys, /tmp
mount -n -t proc -onodev,noexec,nosuid proc /proc
mount -n -t sysfs -onodev,noexec,nosuid sysfs /sys
mount -n -t tmpfs tmp /tmp

# Moblin trick: Disable blinking cursor. Without this a splash screen
# will show a distinct cursor shape even when the cursor is set to none.
echo 0 > /sys/devices/virtual/graphics/fbcon/cursor_blink

# Mount /dev and pre-populate with the set of devices needed
# for X to run.
mount -n -t tmpfs -omode=0755 udev /dev
cp -a -f /lib/udev/devices/* /dev
mknod -m 0600 /dev/initctl p
mount -n -t tmpfs -onosuid,nodev shmfs /dev/shm
mount -n -t devpts -onoexec,nosuid,gid=5,mode=0620 devpts /dev/pts

# Splash screen!
if [ -x /usr/bin/ply-image ]
then
  /usr/bin/ply-image /usr/share/chromeos-assets/images/login_splash.png &
fi

# Mount our stateful partition
if [ $HAS_INITRAMFS -eq 0 ]
then
  mount -n -t ext3 /dev/sda4 /mnt/stateful_partition
  mount -n --bind /mnt/stateful_partition/var /var
  mount -n --bind /mnt/stateful_partition/home /home
fi

# Run and lock directories.
mount -n -t tmpfs -omode=0755,nosuid varrun /var/run
touch /var/run/.ramfs  # TODO: Is this needed?
mount -n -t tmpfs -omode=1777,nodev,noexec,nosuid varlock /var/lock
touch /var/lock/.ramfs # TODO: Is this needed?

# Bootchart
if [ \( $HAS_INITRAMFS -eq 0 \) -a \( -d /lib/bootchart \) ]
then
  BC_LOGS=/var/run/bootchart
  BC_HZ=25
  mkdir -p "$BC_LOGS"
  start-stop-daemon --background --start --quiet \
    --exec /lib/bootchart/collector -- $BC_HZ "$BC_LOGS"
fi

# Prepare for X
X_SOCKET_DIR=/tmp/.X11-unix
X_ICE_DIR=/tmp/.ICE-unix
hostname localhost  # Some things freak out if no hostname is set.
mkdir -p "$X_SOCKET_DIR" "$X_ICE_DIR"
chown root:root "$X_SOCKET_DIR" "$X_ICE_DIR"
chmod 1777 "$X_SOCKET_DIR" "$X_ICE_DIR"

# create salt for user data dir crypto
mkdir -p /home/.shadow
SALT=/home/.shadow/salt
(test -f "$SALT" || head -c 16 /dev/urandom > "$SALT") &

# Login Manager
start-stop-daemon --start --quiet --pidfile /var/run/slim.lock \
  --name slim --startas /usr/bin/slim -- -d

