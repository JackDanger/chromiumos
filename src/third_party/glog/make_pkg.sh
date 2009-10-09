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

PKG_BASE=libgoogle-glog0

# Die on any errors
set -e

# Make output dir
OUT_DIR="$FLAGS_build_root/x86/local_packages"
mkdir -p "$OUT_DIR"

# Remove previous package from output dir
rm -f "$OUT_DIR"/${PKG_BASE}_*.deb

# Build the package
pushd "$TOP_SCRIPT_DIR/files"
./configure --prefix=/usr

# Tell glog to build binary-only deb to avoid interactive prompt
sed -i 's/debuild -uc -us/debuild -b -uc -us/' packages/deb.sh

make -j$NUM_JOBS deb
sudo -E dpkg -i --force-all packages/debian-*/sid/*.deb
cp packages/debian-*/sid/${PKG_BASE}_*.deb "$OUT_DIR"
popd
