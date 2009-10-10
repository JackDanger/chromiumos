#!/bin/sh

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A script to install from removable media to hard disk.

if [ "$USER" != "chronos" ]
then
  echo ""
  echo "Note: You must be the 'chronos' user to run this script."
  echo ""
  echo "Usage: install_to_hard_disk [destination_device]"
  echo ""
  echo "This will install the usb image to your machine's hard disk."
  echo "By default, it will attempt to install to '/dev/sda'."
  echo "First 'su chronos' and then run the script. It will ask"
  echo "for the root password before messing with your hard disk."
  exit 1
fi

DST=/dev/sda
if [ -n "$1" ]
then
  DST="$1"
fi

# First find the root device that we are installing from and verify it.
CMDLINE=`cat /proc/cmdline`
LABEL=`echo $CMDLINE | sed 's/.*root=LABEL=\([-\.[:alpha:][:digit:]]*\).*/\1/g'`
if [ "$LABEL" = "$CMDLINE" ]
then
  echo "Error: Unable to find root by label. Are you booted off USB?"
  exit 1
fi
echo "Using source USB device with label: $LABEL"
SRC_PARTITION=`readlink -f /dev/disk/by-label/$LABEL`
if [ ! -b "$SRC_PARTITION" ]
then
  echo "Error: Unable to find USB partition with label: $LABEL"
  exit 1
fi
SRC=`echo "$SRC_PARTITION" | sed 's/\(\/dev\/[[:alpha:]]*\)[[:digit:]]*/\1/g'`
if [ ! -b "$SRC" ]
then
  echo "Error: Unable to find USB device: $SRC"
  exit 1
fi
SRC_DEV=${SRC#/dev/}
REMOVABLE=`cat /sys/block/$SRC_DEV/removable`
if [ "$REMOVABLE" != "1" ]
then
  echo "Error: Source does not look like a removable device: $SRC_DEV"
  exit 1
fi

# Check out the dst device.
if [ ! -b "$DST" ]
then
  echo "Error: Unable to find destination device: $DST"
  exit 1
fi
DST_DEV=${DST#/dev/}
REMOVABLE=`cat /sys/block/$DST_DEV/removable`
if [ $? -ne 0 ]
then
  echo "Error: Invalid destiation device (must be whole device): $DST"
  exit 1
fi
if [ "$REMOVABLE" != "0" ]
then
  echo "Error: Attempt to install to a removeable device: $DST"
  exit 1
fi

# Ask for root password to be sure.
echo "This will install from '$SRC' to '$DST'. If you are sure this is "
echo "what you want then feel free to enter the root password to proceed."
sudo -K  # Force them to enter a password

# Verify sizes. Since we use sfdisk, this needs to be done after we start
# asking for the root password.
SIZE=`sudo sfdisk -l "$SRC" 2>/dev/null | grep "$SRC_PARTITION" | grep '*' | awk '{ print $6 }'`
SIZE=$((SIZE * 1024))
if [ $SIZE -eq 0 ]
then
  echo "Error: Unable to get system partition size."
  exit 1
fi
SIZE_NEEDED=$((SIZE * 4))  # Assume 2 system images, swap, and user space.
DST_SIZE=`cat /sys/block/$DST_DEV/size`
DST_SIZE=$((DST_SIZE * 512))
if [ $DST_SIZE -lt $SIZE_NEEDED ]
then
  echo "Error: Destination device is too small ($DST_SIZE vs $SIZE_NEEDED)"
  exit 1
fi

# Location to temporarily mount the new system image to fix things up.
ROOTFS_DIR="/tmp/chromeos_hd_install"

# From now on we die on error. We set up a failure handler and continue.
error_handler() {
  ! sudo umount "$ROOTFS_DIR"

  echo "Sorry, an error occurred after we messed with the hard disk."
  echo "The chromeos image was NOT installed successfully and there is "
  echo "a chance that you will not be able to boot off the netbook hard disk."
}
set -e
trap error_handler EXIT

# Copy the system image. We sync first to try to make the copy as safe as we
# can and copy the MBR plus the first partition in one go. We'll fix up the
# MBR and repartition below.
echo ""
echo "Copying the system image. This might take a while..."
BS=$((1024 * 1024 * 4))  # 4M block size
COUNT=$(((SIZE / BS) + 1))
sync
sudo dd if="$SRC" of="$DST" bs=$BS count=$COUNT
sync

# Set up the partition table. This is different from the USB partition table
# because the user paritition expands to fill the space on the disk.
# NOTE: We currently create an EFI partition rather than swap since the
# eeepc can take advantage of this to speed POST time.
NUM_SECTORS=$((SIZE / 512))
sudo sfdisk --force -uS "$DST" <<EOF
,$NUM_SECTORS,L,*,
,$NUM_SECTORS,L,-,
,$NUM_SECTORS,ef,-,
,,L,-,
;
EOF
sync

# Wait a bit and make sure that the partition device shows up.
DST_PARTITION="${DST}1"
sleep 5
if [ ! -b "$DST_PARTITION" ]
then
  echo "Error: Destination system partition was not created: $DST_PARTITION"
  exit 1
fi

# FSCK to make sure. We ignore errors here since e2fsck will report a
# non-zero result if it had to fix anything up.
! sudo e2fsck -y -f "$DST_PARTITION"

# Fix up the file system label. We skip the first char and prefix with 'H'
NEW_LABEL=`expr substr "$LABEL" 2 ${#LABEL}`
NEW_LABEL="H${LABEL}"
sudo tune2fs -L "$NEW_LABEL" "$DST_PARTITION"

# Mount root partition
mkdir -p "$ROOTFS_DIR"
sudo mount "$DST_PARTITION" "$ROOTFS_DIR"
# fix up the extlinux.conf

sudo sed -i '{ s/^DEFAULT .*/DEFAULT chromeos-hd/ }' \
  "$ROOTFS_DIR"/boot/extlinux.conf
sudo sed -i "{ s:HDROOT:$DST_PARTITION: }" \
  "$ROOTFS_DIR"/boot/extlinux.conf

# make a mountpoint for partition 4
sudo mkdir -p "$ROOTFS_DIR"/mnt/stateful_partition
sudo chmod 0755 "$ROOTFS_DIR"/mnt
sudo chmod 0755 "$ROOTFS_DIR"/mnt/stateful_partition

# fix up fstab to keep home dir on 4th partition
STATEFUL_PARTITION="${DST}4"
# delete old home partition info
sudo sed '/\/home\/chronos/d' "$ROOTFS_DIR"/etc/fstab
sudo mkfs.ext3 "$STATEFUL_PARTITION"
STATEFUL_PART_DIR=/tmp/chromeos-installer-home
sudo mkdir -p "$STATEFUL_PART_DIR"
sudo mount "$STATEFUL_PARTITION" "$STATEFUL_PART_DIR"

# set up home
sudo mkdir -p "$STATEFUL_PART_DIR"/home
sudo chown 0:0 "$STATEFUL_PART_DIR"/home
sudo chmod 0755 "$STATEFUL_PART_DIR"/home
sudo cp -Rp "$ROOTFS_DIR"/home/chronos "$STATEFUL_PART_DIR"/home

# set up var
sudo mkdir -p "$STATEFUL_PART_DIR"/var
sudo cp -Rp "$ROOTFS_DIR"/var/* "$STATEFUL_PART_DIR"/var/

# Default to Pacific timezone
sudo mkdir "$STATEFUL_PART_DIR"/etc
sudo rm "$ROOTFS_DIR"/etc/localtime
sudo ln -s /usr/share/zoneinfo/US/Pacific "$STATEFUL_PART_DIR"/etc/localtime
sudo ln -s /mnt/stateful_partition/etc/localtime "$ROOTFS_DIR"/etc/localtime

sudo umount "$STATEFUL_PART_DIR"

# write out a new fstab.
# keep in sync w/ src/scripts/mk_memento_images.sh
# TODO: Figure out if we can mount rootfs read-only
cat <<EOF | sudo dd of="$ROOTFS_DIR"/etc/fstab
/dev/root / rootfs ro 0 0
tmpfs /tmp tmpfs rw,nosuid,nodev 0 0
$STATEFUL_PARTITION /mnt/stateful_partition ext3 rw 0 1
/mnt/stateful_partition/home /home bind defaults,bind 0 0
/mnt/stateful_partition/var /var bind defaults,bind 0 0
EOF


sudo umount "$ROOTFS_DIR"

# Force data to disk before we declare done.
sync

echo "------------------------------------------------------------"
echo ""
echo "Installation to '$DST' complete."
echo "Please remove the USB device, cross your fingers, and reboot."

trap - EXIT
