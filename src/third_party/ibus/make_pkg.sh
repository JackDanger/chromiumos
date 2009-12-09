#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds the .deb package.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"

# Command line options
DEFINE_string build_root "$DEFAULT_BUILD_ROOT" "Root of build output"

# Parse command line and update positional args
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Die on any errors
set -e

# Remove previous package from output dir
PKG_BASE="ibus"
# TODO(yusukes): support ARM.
OUT_DIR="$FLAGS_build_root/x86/local_packages"
mkdir -p "${OUT_DIR}"
rm -f "${OUT_DIR}/"*"${PKG_BASE}"*_*.deb

# Set up the debian build directory
PKG_BUILD_DIR="${FLAGS_build_root}/${PKG_BASE}"
mkdir -p "$PKG_BUILD_DIR"
rm -rf "${PKG_BUILD_DIR}/build"
cp -rp "${TOP_SCRIPT_DIR}/files" "${PKG_BUILD_DIR}/build"
pushd "${PKG_BUILD_DIR}/build"
# Apply patches
touch ./ChangeLog
chmod u+x ./debian/rules
./autogen.sh

# Build the package
dpkg-buildpackage -b -tc -us -uc
mv ../*"${PKG_BASE}"*_*.deb "${OUT_DIR}"
# The .changes file contain checksums of the .deb packages, etc. Since "dpkg -i"
# command doesn't use the file, we can delete it.
rm -f ../"${PKG_BASE}"_*.changes
popd

# Install packages that are necessary for building ibus-chewing and ibus-anthy packages.
sudo -E dpkg -i "${OUT_DIR}/"*"${PKG_BASE}"*_*.deb
