#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Some common functions used in many of the test cases.


function CreateCopyFile {
  # This function will create a file of $size bytes, and then copy this
  # file to a target directory. It will then verify that the file created
  # matches the file that was copied.
  local ARGNUM=3
  local BLOCK=1024

  if [ $# -ne $ARGNUM ]; then
    echo "Usage: $0 size tempfile targetfile"
    echo "size = size of tempfile to create"
    echo "tempfile = pathname of tempfile to create"
    echo "targetfile = pathname of place to copy temp file to"

    exit 1
  else
    size=$1
    tempfile=$2
    target=$3
  fi

  status=0

  dd if=/dev/zero of="${tempfile}" bs="${BLOCK}" count="${size}"

  # Get the original file size and md5sum value.
  sizeorig="$(stat -c%s ${tempfile})"
  md5orig="$(md5sum ${tempfile} | awk '{print $1}')"
  # Copy the file to our destination location.
  cp "${tempfile}" "${target}"

  # Get the target file size and md5sum value.
  sizetarget="$(stat -c%s ${target})"
  md5target="$(md5sum ${target} | awk '{print $1}')"

  if [ "${sizeorig}" != "${sizetarget}" ]; then
    status=1
    echo "FAIL: $0"
    echo "file copied: ${sizetarget}"
    echo "original file: ${sizeorig}"
  fi

  if [ "${md5orig}" != "${md5target}" ]; then
    status=1
    echo "FAIL: $0"
    echo "md5sums of original and copied files don't match!"
  fi

  rm -f "${tempfile}"
  rm -f "${target}"

  exit "${status}"
}

function CreateFile {
  # This function will create a file using dd, and verify that the size 
  # created is what we expected using the stat command.

  local ARGNUM=2
  local BLOCK=1024

  if [ $# -ne $ARGNUM ]; then
    echo "Usage: $0 size tempfile"
    echo "size = size of tempfile to create"
    echo "tempfile = pathname of file to create"
    exit 1
  else
    size=$1
    tempfile=$2
  fi

  status=0

  fsize="$((size * BLOCK))"

  dd if=/dev/zero of="${tempfile}" bs="${BLOCK}" count="${size}"

  bytes="$(stat -c%s ${tempfile})"

  if [ "${bytes}" != "${fsize}" ]; then
    status=1
    echo "FAIL: $0"
    echo "File Size: ${bytes}"
    echo "Expected Size: ${fsize}"
  else
    echo "PASS: $0"
  fi

  rm -f "${tempfile}"

  exit "${status}"
}

function MakeFiles {
  # This function will make $count number of files, and verify that the
  # number of files created and copied matches the number we expected,
  # and that the space they use is the same.

  local ARGNUM=2

  if [ $# -ne $ARGNUM ]; then
    echo "Usage: $0 count directory"
    echo "count = number of files to create"
    echo "directory = directory to create files in"
    exit 1
  else
    count=$1
    rootdir=$2
  fi

  status=0

  sourcedir="${rootdir}/sourcedir"
  targetdir="${rootdir}/targetdir"

  mkdir -p "${sourcedir}"
  if [ $? -ne 0 ]; then
    echo "Error creating ${sourcedir}"
    exit 1
  fi

  mkdir -p "${targetdir}"
  if [ $? -ne 0 ]; then
    echo "Error creating ${targetdir}"
    exit 1
  fi

  i=0
  while [ $i -lt $count ]; do
    echo "A line of text for our testfile ${i}" > "${sourcedir}/file${i}"
    cp "${sourcedir}/file${i}" "${targetdir}"
    i=$[$i+1]
  done

  sourcenum=$(ls -l "${sourcedir}" | wc -l)
  source_usage=$(du -sk "${sourcedir}" | awk '{print $1}')
  targetnum=$(ls -l "${targetdir}" | wc -l)
  target_usage=$(du -sk "${targetdir}" | awk '{print $1}')

  if [ "${sourcenum}" -ne "${targetnum}" ]; then
    echo "Error: ${sourcedir} has ${sourcenum} files"
    echo "${targetdir} has ${targetnum} files"
    status=1
  else
    echo "Number of files in each directory matches"
  fi

  if [ "${source_usage}" -ne "${target_usage}" ]; then
    echo "Error: file usage of ${sourcedir} is ${source_usage}"
    echo "file usage of ${targetdir} is ${target_usage}"
    status=1
  else
    echo "Each directory uses the same amount of space"
  fi

  echo "Cleaning up..."
  rm -rf "${sourcedir}"
  rm -rf "${targetdir}"

  exit "${status}"
}
