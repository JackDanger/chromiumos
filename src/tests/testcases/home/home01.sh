#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A simple test to ensure we can create and save a data file in the
# home directory.

status=0
TESTFILE=~/testhome.txt
i=1
text[1]="This is a simple text file to test the home directory"
text[2]="This file should be created and saved in $TESTFILE"

echo ${text[1]} > $TESTFILE
echo ${text[2]} >> $TESTFILE

cat "${TESTFILE}" | while read line
do
  if [[ "${line}" = "${text[i]}" ]]
  then
    echo "Lines are equal"
  else
    echo "Lines are not equal"
    status=1
  fi
  i=$[$i+1]
done

exit "${status}"
