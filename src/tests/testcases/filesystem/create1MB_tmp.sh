#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This test will attempt to create a 1MB file on the temp file system
# and verify the size if correct.
TESTHOME="$(readlink -f "${0%/*}/..")"
source ${TESTHOME}/common/functions.sh

SIZE=1000
TEMPFILE="/tmp/testfile"

CreateFile "${SIZE}" "${TEMPFILE}"
exit "$?"
