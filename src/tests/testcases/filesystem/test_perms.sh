#!/bin/sh

# This script can be used to test some filesystem properties of our images.
# How to use: start ssh on the target machine and note the IP address. Then:
# $ scp test_filesystem.sh chronos@<IP ADDRESS>:/tmp && \
#   ssh -t chronos@<IP ADDRESS> /bin/sh /tmp/test_filesystem.sh

set -e

# Root-owned directories
ROOT_DIRS="/ /bin /boot /dev /etc /home /lib /media /mnt 
           /mnt/stateful_partition /opt /proc
           /sbin /sys /tmp /usr /usr/bin /usr/lib /usr/local 
           /usr/local/bin /usr/local/etc /usr/local/lib /usr/local/sbin
           /usr/local/share /usr/sbin /usr/share /var /var/cache
           /var/lib /var/lock /var/log /var/run /var/tmp /tmp"

# Read-only filesystem directories
RO_FS_DIRS="/ /bin /boot /etc /lib /media /mnt /opt
            /sbin /usr /usr/bin /usr/lib /usr/local 
            /usr/local/bin /usr/local/etc /usr/local/lib /usr/local/sbin
            /usr/local/share /usr/sbin /usr/share /var
            /var/lib /var/local"

# subset of ROOT_DIRS that are writable by root
ROOT_RW_DIRS="/var/cache /var/log"

TMP_DIRS="/tmp /var/lock /var/tmp"

ERRORS_FOUND=0

found_error() {
  ERRORS_FOUND=$((ERRORS_FOUND + 1))
}

# Ensure we can't write into read-only filesystem
for i in $RO_FS_DIRS; do
  echo testing $i
  # writing a file should fail
  TEST_FILE="$i/test.file"
  ! OUT=$(sudo touch "$TEST_FILE" 2>&1)
  if [ "$OUT" != "touch: cannot touch \`$TEST_FILE': Read-only file system" ]
  then
    found_error
    echo "Write incorrectly succeeded: $OUT"
    ! sudo rm -f "$TEST_FILE"
  fi
done

# Ensure UID=GID=0 for root-owned directories
for i in $ROOT_DIRS; do
  echo testing $i
  if [ "$(stat -c %u $i)" != "0" ]; then
    found_error
    echo bad UID "$(stat -c %u $i)"
  fi
  if [ "$(stat -c %g $i)" != "0" ]; then
    found_error
    echo bad GID "$(stat -c %g $i)"
  fi
done

# Ensure only root can write into these directories
for i in $ROOT_RW_DIRS; do
  echo testing $i
  # writing a file as root should succeed
  TEST_FILE="$i/test.file"
  ! OUT=$(sudo touch "$TEST_FILE" 2>&1)
  if [ "x$OUT" != "x" ]
  then
    found_error
    echo "Write incorrectly failed: $OUT"
  fi
  ! sudo rm -f "$TEST_FILE"

  # writing a file as non-root should fail
  TEST_FILE="$i/test.file"
  ! OUT=$(touch "$TEST_FILE" 2>&1)
  if [ "$OUT" != "touch: cannot touch \`$TEST_FILE': Permission denied" ]
  then
    found_error
    echo "Write incorrectly succeeded: $OUT"
    ! rm -f "$TEST_FILE"
  fi
done

# Ensure anyone can write into temp directories
for i in $TMP_DIRS; do
  echo testing $i
  # writing a file as non-root should succeed
  TEST_FILE="$i/test.file"
  ! OUT=$(touch "$TEST_FILE" 2>&1)
  if [ "x$OUT" != "x" ]
  then
    found_error
    echo "Write incorrectly failed: $OUT"
  fi
  ! rm -f "$TEST_FILE"
done

echo Found "$ERRORS_FOUND" errors
if [ "$ERRORS_FOUND" -gt "0" ]; then
  exit 255
fi

