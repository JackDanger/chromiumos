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
PKG_BASE="libclutter-1.0"

# Command line options
DEFINE_string build_root "$DEFAULT_BUILD_ROOT" "Root of build output"

# Parse command line and update positional args
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Die on any errors
set -e

# Make output dir
# TODO(kwaters): target architecture
OUT_DIR="${FLAGS_build_root}/x86/local_packages"
mkdir -p "${OUT_DIR}"

# Remove previous package from output dir
rm -f "${OUT_DIR}/${PKG_BASE}-*_*.deb"

# apply patches
rm -fr "${TOP_SCRIPT_DIR}/files/debian"
pushd "${TOP_SCRIPT_DIR}/files"
gunzip -c "${TOP_SCRIPT_DIR}/clutter-1.0_1.0.4-0ubuntu1.diff.gz" | patch -p1
patch -p1 <"${TOP_SCRIPT_DIR}/debian_rules.patch"
popd

chmod +x "${TOP_SCRIPT_DIR}/files/debian/rules"

# Build the package
pushd "$TOP_SCRIPT_DIR/files"
dpkg-buildpackage -b -tc -us -uc
mv ../"${PKG_BASE}"-*_*.deb "${OUT_DIR}"
rm ../clutter-1.0_*.changes
popd

