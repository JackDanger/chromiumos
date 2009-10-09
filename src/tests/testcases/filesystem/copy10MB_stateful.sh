#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This test will attempt to copy a 10 MB file to the stateful partition.
# and verify the bytes are the same.

TESTHOME="$(readlink -f "${0%/*}/..")"
source ${TESTHOME}/common/functions.sh

SIZE=10000
TEMPFILE="/tmp/testfile"
TARGETFILE="/mnt/stateful_partition/testfile"

CreateCopyFile "${SIZE}" "${TEMPFILE}" "${TARGETFILE}"
exit "$?"
