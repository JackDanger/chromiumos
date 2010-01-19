#!/bin/bash
# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

CHLIB="$HOME/trunk/src/platform/cryptohome/lib"
source "$CHLIB/common"
source "$CHLIB/utils/declare_commands"
source "$CHLIB/cryptohome"

utils::declare_commands sha256sum

USERNAME="testuser@invalid.domain"
PASSWORDS="zero one two"

function usage {
  $echo "Usage: $0 [-q] <image-dir>"
  $echo
  $echo "Verifies that the cryptohome script is able to decrypt"
  $echo "the sample data created by init_cryptohome_data.sh."
  $echo
  $echo "Returns an exit code of 0 on success, nonzero otherwise."
  $echo
  $echo "         -q   Quiet mode"
  $echo " <image-dir>  Directory to store cryptohome data"
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
  info=$echo
else
  function no_echo {
    echo -n
  }

  info="no_echo"
fi

SYSTEM_SALT_FILE="$IMAGE_DIR/salt"

USERID=$(cat "$SYSTEM_SALT_FILE" <($echo -n $USERNAME) \
  | $openssl sha1)

$info "USERNAME: $USERNAME"
$info "USERID: $USERID"

RESULT=0
INDEX=0
for PASSWORD in $PASSWORDS; do
  HASHED_PASSWORD=$(cat <(echo -n $($xxd -p "$SYSTEM_SALT_FILE")) \
    <($echo -n "$PASSWORD") | $sha256sum | $head -c 32)

  $info "Checking master.$INDEX..."
  $info "PASSWORD: $PASSWORD"
  $info "HASHED_PASSWORD: $HASHED_PASSWORD"

  WRAPPER=$(cryptohome::password_to_wrapper "$HASHED_PASSWORD" \
    "$IMAGE_DIR/$USERID/master.$INDEX.salt")

  $info "WRAPPER: $WRAPPER"

  # uncomment if you want to see the computed salt, key, and iv
  # $openssl aes-256-ecb \
  #   -in "$IMAGE_DIR/$USERID/master.$INDEX" \
  #   -kfile <($echo -n "$WRAPPER") -md sha1 -d -P

  PLAINTEXT=$(cryptohome::unwrap_master_key "$HASHED_PASSWORD" "$USERID" \
    "$IMAGE_DIR/$USERID/master.$INDEX")

  EXIT=$?

  if [ $EXIT != 0 ]; then
    RESULT=$EXIT
  fi

  if [ $QUIET == 0 ]; then
    $info "MASTER_KEY:"
    $xxd <(echo -n "$PLAINTEXT")
  fi

  INDEX=$(($INDEX + 1))
done

if [ $RESULT != 0 ]; then
  $info "*** At least one decrypt failed!"
fi

exit $RESULT
