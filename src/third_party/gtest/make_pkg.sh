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

# Make output dir
OUT_DIR="$FLAGS_build_root/x86/local_packages"
mkdir -p "${OUT_DIR}"

# Build the package
pushd "$TOP_SCRIPT_DIR/files"
rm -rf debian
ln -s ../debian debian
rm -f ../libgtest0_*.deb ../libgtest-dev_*.deb
dpkg-buildpackage -b -tc -us -uc -j$NUM_JOBS
mv ../libgtest0_*.deb ../libgtest-dev_*.deb "$OUT_DIR"
rm ../gtest_*.changes
popd

# Install packages that are necessary for building later packages.
sudo -E dpkg -i "${OUT_DIR}/libgtest0"_*.deb "${OUT_DIR}/libgtest-dev"_*.deb
