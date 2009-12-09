#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

BUILDDIR=$PWD/build

# The number of jobs to pass to tools that can run in parallel (such as make
# and dpkg-buildpackage
NUM_JOBS=`cat /proc/cpuinfo | grep processor | awk '{a++} END {print a}'`

rm -rf $BUILDDIR
mkdir -p $BUILDDIR

# build connman
pushd connman-0.42
./configure \
    --enable-debug \
    --prefix=/usr \
    --mandir=/usr/share/man \
    --localstatedir=/var \
    --sysconfdir=/etc \
    --enable-loopback=builtin \
    --enable-ethernet=builtin \
    --enable-wifi=builtin \
    --enable-dhclient=builtin \
    --enable-dnsproxy=builtin 
automake
make clean || true
make -j$NUM_JOBS 
make install DESTDIR=$BUILDDIR
popd
