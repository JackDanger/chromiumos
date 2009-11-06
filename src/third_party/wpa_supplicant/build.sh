#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

HOSTAP_DIR=${HOSTAP_DIR:-hostap.git}

BUILDDIR=$PWD/build

# The number of jobs to pass to tools that can run in parallel (such as make
# and dpkg-buildpackage
NUM_JOBS=`cat /proc/cpuinfo | grep processor | awk '{a++} END {print a}'`

rm -rf $BUILDDIR
mkdir -p $BUILDDIR

# build wpa_supplicant
pushd ${HOSTAP_DIR}/wpa_supplicant
export LIBDIR=/lib
export BINDIR=/sbin

make clean || true

cat >.config<<EOF
CONFIG_DRIVER_WEXT=y
#CONFIG_DRIVER_NL80211=y
CONFIG_DRIVER_WIRED=y
CONFIG_IEEE8021X_EAPOL=y
CONFIG_EAP_MD5=y
CONFIG_EAP_MSCHAPV2=y
CONFIG_EAP_TLS=y
CONFIG_EAP_PEAP=y
CONFIG_EAP_TTLS=y
CONFIG_EAP_GTC=y
CONFIG_EAP_OTP=y
CONFIG_EAP_LEAP=y
CONFIG_PKCS12=y
CONFIG_SMARTCARD=y
CONFIG_CTRL_IFACE=y
CONFIG_BACKEND=file
CONFIG_PEERKEY=y
CONFIG_TLS=openssl
CONFIG_CTRL_IFACE_DBUS=y
CONFIG_DYNAMIC_EAP_METHODS=y
CONFIG_DEBUG_SYSLOG=y
EOF
make -j$NUM_JOBS 

make install DESTDIR=$BUILDDIR
popd
