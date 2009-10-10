#!/bin/bash

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Sets up the wireless interfaces and runs the DHCP client at boot time.

# Gets the name of the wireless interface. 
# TODO(rtc): We'll need better error handline once we start targeting 
# more than two platforms.
WIFI_IFACE=`/sbin/iwconfig |grep 802.11 | cut -c -6 | sed -e 's/ *$//'`

if [ -z $IFACE ]
then 
  IFACE=$WIFI_IFACE
fi

LOG_FILE="/var/log/chromeos_wifi.${IFACE}.log"
echo "" >> $LOG_FILE
echo "" >> $LOG_FILE
echo "mode - $MODE" >> $LOG_FILE
echo "phase - $PHASE" >> $LOG_FILE

MATCH=`expr match $WIFI_IFACE $IFACE`
if [ "$MATCH" -le "0" ]
then
  echo "trying to configure $IFACE which is not wireless, aborting" >> $LOG_FILE
  exit 0
fi

WPA_FILE="/var/run/wpa_supplicant/$WIFI_IFACE" 
if [ -e $WPA_FILE ] 
then 
  echo "wpa_supplicant is already running, aborting" >> $LOG_FILE
  exit 0
fi

SYSTEM_NAME=`dmidecode -s system-product-name`

echo "Starting wpa_supplicant" >> $LOG_FILE
wpa_supplicant -i $WIFI_IFACE -Dwext -c /etc/wpa_supplicant.conf -B

echo "Starting dhclient" >> $LOG_FILE
dhclient -nw
