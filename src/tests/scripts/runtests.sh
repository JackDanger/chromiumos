#!/bin/bash
#
# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# A script to drive our tests on ChromeOS.
# This script will drive:
#   - functional acceptance tests
#   - Linux Test Project Test Harness
#   - UnixBench 5.1.2

# Linux Test Project
# We'll run the LTP with our own test definition file called chromeos.tests

datetime="$(date +%Y%m%d%H%M%S)"
hostip=$(/sbin/ifconfig wlan0 | awk '/inet addr/ {split ($2,A,":"); print A[2]}')
logfile="${datetime}-${hostip}"
ltplog="${logfile}.ltp"
ltpout="${logfile}.out"
ltphtml="${logfile}.html"

startdir="$(pwd)"

# Collect some inventory information. This is needed by the parser that parses
# through the logs and reports the results to a dashboard.
source /etc/lsb-release
model="$(dmidecode -t 1 | grep Product | cut -d':' -f2)"
serial="$(dmidecode -t 1 | grep Serial | cut -d':' -f2)"
vendor="$(dmidecode -t 1 | grep Manufacturer | cut -d':' -f2)"
kernel="$(uname -r)"
arch="$(uname -m)"
cputype="$(dmidecode -t 4 | grep Version | cut -d':' -f2)"
cpunum="$(grep -i 'physical id' /proc/cpuinfo | sort -u | wc -l)"
bios="$(dmidecode -t 0 | grep Version | cut -d':' -f2)"
biosdate="$(dmidecode -t 0 | grep 'Date:' | cut -d':' -f2)"
mem="$(grep MemTotal /proc/meminfo | cut -d':' -f2)"
video="$(lspci | grep VGA | cut -d':' -f3)"
nic="$(lspci | grep Network | cut -d':' -f3)"

if [ -n ${nic:-x} ]
then
  nic="$(lspci | grep Ethernet | cut -d':' -f3)"
fi

echo "Starting tests at ${datetime}"
echo "Running tests on ${hostip}"

# Run Functional Tests. runner.sh will run all tests listed in chromeos.suite
echo "Running Functional Tests..."
cd ../tests/testcases
./runner.sh chromeos.suite

# Linux Test Project
# Run all kernel and system tests located in chromeos.tests.
# The Linux Text Project is updated monthly, so ensure you grab the latest
# one and compile it each month.

echo "Running LTP tests..."
cd ../ltp-current/
./runltp -p -l "${ltplog}" -o "${ltpout}" -f chromeos.tests -g "${ltphtml}"
cd results
touch "${ltplog}"
echo "Finished running LTP tests..."
echo "--------------------------------------" >> "${ltplog}"
echo "Linux Test Project" >> "${ltplog}"
echo "Distribution:          ${GOOGLE_ID}" >> "${ltplog}"
echo "Google Release:        ${GOOGLE_RELEASE}" >> "${ltplog}"
echo "Codename:              ${GOOGLE_CODENAME}" >> "${ltplog}"
echo "Developer Track:       ${GOOGLE_TRACK}" >> "${ltplog}"
echo "System Model:          ${model}" >> "${ltplog}"
echo "Kernel Version:        ${kernel}" >> "${ltplog}"
echo "Architecture:          ${arch}" >> "${ltplog}"
echo "Processor Model:       ${cputype}" >> "${ltplog}"
echo "Number of Processors:  ${cpunum}" >> "${ltplog}"
echo "System Memory:         ${mem}" >> "${ltplog}"
echo "Bios Date:             ${biosdate}" >> "${ltplog}"
echo "Bios Version:          ${bios}" >> "${ltplog}"
echo "Graphics Adapter:      ${video}" >> "${ltplog}"
echo "Network Card:          ${nic}" >> "${ltplog}"
echo "--------------------------------------" >> "${ltplog}"

# UnixBench 5.1.2
# This needs to be precompiled. Also, the Run perl script needs to be built into
# a self-contained perl par file. I built one and called it run.

echo "Running UnixBench 5.1.2 benchmark..."
cd ../../unixbench-5.1.2/
# Must remove any existing test reports or it will bail out
rm -f results/*
./run -c 16
echo "Finished running UnixBench..."

cd "${startdir}"
