#!/bin/bash -e

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Builds tests.

SCRIPT_DIR=`dirname "$0"`
SCRIPT_DIR=`readlink -f "$SCRIPT_DIR"`

BUILD_ROOT=${BUILD_ROOT:-${SCRIPT_DIR}/../../../src/build}
OUT_DIR=${BUILD_ROOT}/x86/tests
mkdir -p $OUT_DIR

pushd $SCRIPT_DIR

TESTS="key_bindings_test layout_manager_test shadow_test util_test
  window_manager_test window_test"

scons $TESTS
cp $TESTS $OUT_DIR

popd
