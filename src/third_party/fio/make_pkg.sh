#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds the .deb package.  This cannot use make_pkg_common because it's in
# the wrong directory relative to the sources.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"

# Make the package
PKG_BASE="fio"

# Command line options
DEFINE_string build_root "$DEFAULT_BUILD_ROOT" "Root of build output"

# Parse command line and update positional args
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

assert_inside_chroot

# Die on any errors
set -e

# Link in debian
cd "${TOP_SCRIPT_DIR}"
ln -sf ../debian files/

# Make output dir
OUT_DIR="${FLAGS_build_root}/x86/local_packages"
mkdir -p "${OUT_DIR}"

# Remove previous package from output dir
rm -f "${OUT_DIR}/${PKG_BASE}_*.deb"

# Build the package
pushd "$TOP_SCRIPT_DIR/files"
rm -f ../"${PKG_BASE}_*.deb"
dpkg-buildpackage -b -tc -us -uc
pwd
mv ../"${PKG_BASE}"_*.deb "${OUT_DIR}"
rm ../"${PKG_BASE}"_*.changes
popd

