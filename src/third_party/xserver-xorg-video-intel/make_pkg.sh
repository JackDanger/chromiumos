#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds the .deb package.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"

# Make the package
PKG_BASE="xserver-xorg-video-intel"

# Command line options
DEFINE_string build_root "$DEFAULT_BUILD_ROOT" "Root of build output"

# Parse command line and update positional args
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Die on any errors
set -e

# Make output dir
OUT_DIR="$FLAGS_build_root/x86/local_packages"
mkdir -p "$OUT_DIR"

# Remove previous package from output dir
rm -f "$OUT_DIR"/${PKG_BASE}_*.deb

# Set up the debian build directory.
PKG_BUILD_DIR="${FLAGS_build_root}/${PKG_BASE}"
mkdir -p "$PKG_BUILD_DIR"
rm -rf "${PKG_BUILD_DIR}/build"
dpkg-source -x "$TOP_SCRIPT_DIR"/src/*.dsc "${PKG_BUILD_DIR}/build"

# Apply our patches.
CHROMEOS_PATCHES=`ls "${TOP_SCRIPT_DIR}"/*.patch`
for i in ${CHROMEOS_PATCHES}
do
  patch -d "$PKG_BUILD_DIR"/build -p1 < "$i"
done

# Build the package. We up the version number before building so that the
# ChromeOS version will get chosen and won't conflict with the repository.
pushd "$PKG_BUILD_DIR"/build
dch -i "ChromeOS Patches"
dpkg-buildpackage -j$NUM_JOBS -b -tc -us -uc
mv ../${PKG_BASE}_*.deb "$OUT_DIR"
rm -f ../${PKG_BASE}_*.changes
popd
