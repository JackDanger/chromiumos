#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds the .deb package.

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
COMMON_SH="$(dirname "$0")/../../scripts/common.sh"
. "$COMMON_SH"

PKG_BASE="chromeos-synaptics"

# All packages are built in the chroot
assert_inside_chroot

# Command line options
DEFINE_string build_root "$DEFAULT_BUILD_ROOT" "Root of build output"

# Parse command line and update positional args
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Die on any errors
set -e

# Make output dir
OUT_DIR="$FLAGS_build_root/x86/local_packages"
mkdir -p "${OUT_DIR}"

# Remove previous package from output dir
rm -f "${OUT_DIR}/lib${PKG_BASE}"-dev_*.deb

# Rebuild the package
pushd "$TOP_SCRIPT_DIR"
rm -f ../lib${PKG_BASE}-dev_*.deb
dpkg-buildpackage -b -us -uc -j$NUM_JOBS
mv ../lib${PKG_BASE}-dev_*.deb "$OUT_DIR"
rm ../${PKG_BASE}_*.changes
popd

# Install packages that are necessary for building later packages.
sudo -E dpkg -i "${OUT_DIR}/lib${PKG_BASE}"-dev_*.deb
