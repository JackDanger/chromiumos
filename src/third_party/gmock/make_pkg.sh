#!/bin/bash
# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Builds the gmock deb packages and installs them.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"

# Link debian/ into the build directory.
set -e  # make sure we don't delete anything we're using on error
pushd $(dirname "$0")/files
rm -rf debian
ln -s ../debian debian
set +e
popd

# Build the package
TOP_SCRIPT_DIR="$(dirname "$0")/files"  # override top level from common
make_pkg_common "*gmock" "$@"  # *gmock covers the various output prefixes
mv libgmock-dev*.deb "$OUT_DIR"  # it still misses the -dev pkg

# Install for use by other packages in the chroot
sudo -E dpkg -i "${OUT_DIR}/libgmock"_*.deb
sudo -E dpkg -i "${OUT_DIR}/libgmock-dev"_*.deb
