#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This test will attempt to create and copy 100,000 files on the stateful
# file system and verify the number of files copied and the disk usage
# is the same.
TESTHOME="$(readlink -f "${0%/*}/..")"
source ${TESTHOME}/common/functions.sh

COUNT=100000
ROOTDIR="/mnt/stateful_partition"

MakeFiles "${COUNT}" "${ROOTDIR}"
exit "$?"
