#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This is the runner that will run functional tests for ChromeOS.
# It is a simple script that will parse chromeos.suite, which should be in the
# format of one test case per line.

if [ "$#" -ne 1 ]; then
  echo -e "\nUsage: $0 <suitefile>"
  exit 1
else
  suitefile=$1
fi

datetime="$(date +%Y%m%d%H%M%S)"
LOGFILE="${datetime}.log"
OUTFILE="${datetime}.output"

source /etc/lsb-release

model="$(dmidecode -t 1 | grep Name: | cut -d':' -f2)"
serial="$(dmidecode -t 1 | grep 'Serial Number:' | cut -d':' -f2)"
vendor="$(dmidecode -t 1 | grep Manufacturer: | cut -d':' -f2)"
kernel="$(uname -r)"
arch="$(uname -m)"
cputype="$(dmidecode -t 4 | grep Version: | cut -d':' -f2)"
cpunum="$(grep -i 'physical id' /proc/cpuinfo | sort -u | wc -l)"
bios="$(dmidecode -t 0 | grep Version | cut -d':' -f2)"
biosdate="$(dmidecode -t 0 | grep 'Date:' | cut -d':' -f2)"
mem="$(grep MemTotal /proc/meminfo | cut -d':' -f2)"
video="$(lspci | grep VGA | cut -d':' -f3)"
nic="$(lspci | grep Network | cut -d':' -f3)"

if [ -n "${nic}:-x" ]; then
  nic="$(lspci | grep Ethernet | cut -d':' -f3)"
fi

echo "Functional Test Run on ${datetime}" > "${LOGFILE}"
echo "==========================================================" >> "${LOGFILE}"
echo "Distribution:          ${GOOGLE_ID}" >> "${LOGFILE}"
echo "Google Release:        ${GOOGLE_RELEASE}" >> "${LOGFILE}"
echo "Codename:              ${GOOGLE_CODENAME}" >> "${LOGFILE}"
echo "Developer Track:       ${GOOGLE_TRACK}" >> "${LOGFILE}"
echo "System Model:          ${model}" >> "${LOGFILE}"
echo "Kernel Version:        ${kernel}" >> "${LOGFILE}"
echo "Architecture:          ${arch}" >> "${LOGFILE}"
echo "Processor Model:       ${cputype}" >> "${LOGFILE}"
echo "Number of Processors:  ${cpunum}" >> "${LOGFILE}"
echo "System Memory:         ${mem}" >> "${LOGFILE}"
echo "Bios Date:             ${biosdate}" >> "${LOGFILE}"
echo "Bios Version:          ${bios}" >> "${LOGFILE}"
echo "Graphics Adapter:      ${video}" >> "${LOGFILE}"
echo "Network Card:          ${nic}" >> "${LOGFILE}"
echo "-----------------------------------------" >> "${LOGFILE}"

echo "Output of Functional Test Run on ${datetime}" > "${OUTFILE}"
echo "==========================================================" >> "${OUTFILE}"

echo "${suitefile}"
# The suitefile should contain one test case per line. We'll honor comments
# only if they start at the beginning of the line.
cat "${suitefile}" | while read testcase; do
  echo "${testcase}" | grep ^#
  if [ $? -ne 0 ]; then
    if [ -z "${testcase}" ]; then
      echo "Skipping blank line"
    else
      echo "Executing ${testcase}"
      "${testcase}" >> "${OUTFILE}" 2>&1
      rc=$?
      if [ $rc -ne 0 ]
      then
        echo "FAIL: ${testcase}" >> "${LOGFILE}"
        echo "Return Code: ${rc}" >> "${LOGFILE}"
      else
        echo "PASS ${testcase}" >> "${LOGFILE}"
      fi
    fi
  fi
done

datetime=$(date +%Y%m%d%H%M%S)
echo "==========================================================" >> "${LOGFILE}"
echo "==========================================================" >> "${OUTFILE}"
echo "Finished test run on $datetime" >> "${LOGFILE}"
echo "Finished test run on $datetime" >> "${OUTFILE}"
