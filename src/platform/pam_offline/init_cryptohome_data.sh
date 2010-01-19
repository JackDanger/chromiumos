#!/bin/bash
# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#
# This uses the cryptohome script to initialize an "IMAGE_DIR".  This directory
# will contain the system salt, and three master keys for a user called
# testuser@invalid.domain.
#
# The three keys will have the passwords "zero", "one" and "two".  You can use
# the check_cryptohome_data.sh script to verify that cryptohome can
# successfully decrypt these keys.  The authenticator_unittest.cc testcases
# call this script to create their test data.
#

CH_LIB="$HOME/trunk/src/platform/cryptohome/lib"
source "$CH_LIB/common"
source "$CH_LIB/utils/declare_commands"
source "$CH_LIB/cryptohome"

utils::declare_commands sha256sum

USERNAME="testuser@invalid.domain"
PASSWORDS="zero one two"

function usage {
  $echo "Usage: $0 [-q] <image-dir>"
  $echo
  $echo "Initialize a directory of sample cryptohome data containing "
  $echo "system salt, and a single user named $USERNAME."
  $echo "The user will have three master keys, encrypted with the "
  $echo "passwords: $PASSWORDS."
  $echo
  $echo "         -q   Quiet mode"
  $echo " <image-dir>  Directory to store cryptohome data"
  $echo
  exit 1
}

QUIET=0
IMAGE_DIR=""

while [ ! -z "$1" ]; do
  if [ "$1" == "-q" ]; then
    QUIET=1; shift
  elif [ -z "$IMAGE_DIR" ]; then
    IMAGE_DIR="$1"; shift
  else
    # we only take two arguments, one of which is optional
    usage
  fi
done

if [[ -z "$IMAGE_DIR" || ${IMAGE_DIR:0:1} == "-" ]]; then
  usage
fi

if [ "$QUIET" == "0" ]; then
  info="echo"
else
  function no_echo {
    echo -n
  }

  info="no_echo"
fi

if [ -d "$IMAGE_DIR" ]; then
  $info "Image directory '$IMAGE_DIR' exists.  Remove it if you would like to"
  $info "re-initialize the test data."
  exit 0
fi

$info "Initializing sample crpytohome image root: $IMAGE_DIR"
$mkdir -p "$IMAGE_DIR"

$info "Creating system salt."
SYSTEM_SALT_FILE="$IMAGE_DIR/salt"
$head -c 16 /dev/urandom > $SYSTEM_SALT_FILE

$info "Creating user directory"

USERID=$($cat "$SYSTEM_SALT_FILE" <($echo -n $USERNAME) \
  | $openssl sha1)

$info "USERNAME: $USERNAME"
$info "USERID: $USERID"

$mkdir -p "$IMAGE_DIR/$USERID"

$info "Creating master keys..."
INDEX=0
for PASSWORD in $PASSWORDS; do
  HASHED_PASSWORD=$(cat <($echo -n $($xxd -p "$SYSTEM_SALT_FILE")) \
    <($echo -n "$PASSWORD") | $sha256sum | $head -c 32)

  $info "PASSWORD: $PASSWORD"
  $info "HASHED_PASSWORD: $HASHED_PASSWORD"

  MASTER_KEY=$(cryptohome::create_master_key "$HASHED_PASSWORD" "$USERID" \
    "$IMAGE_DIR/$USERID/master.$INDEX")

  EXIT=$?
  if [ $EXIT != 0 ]; then
    exit $EXIT
  fi

  $info "MASTER_KEY: $MASTER_KEY"

  INDEX=$(($INDEX + 1))
done
