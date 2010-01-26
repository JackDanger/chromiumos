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

PKG_BASE=google-breakpad

# Die on any errors
set -e

# Make output dir
OUT_DIR="$FLAGS_build_root/x86/local_packages"
mkdir -p "${OUT_DIR}"

# Remove previous package from output dir
rm -f "$OUT_DIR"/libbreakpad-dev_*.deb

# Build the package
pushd "$TOP_SCRIPT_DIR/files"
rm -rf debian
ln -s ../debian debian
dpkg-buildpackage -b -tc -us -uc -j$NUM_JOBS
mv ../libbreakpad-dev_*.deb "$OUT_DIR"
rm ../${PKG_BASE}_*.changes
popd

# Install packages that are necessary for building later packages.
sudo -E dpkg -i "${OUT_DIR}/libbreakpad-dev"_*.deb
