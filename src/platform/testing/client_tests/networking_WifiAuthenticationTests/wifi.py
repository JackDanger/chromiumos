#!/usr/bin/python

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Some wrapper code for interacting with the wireless interface

This code should only be used for a quick and dirty control
panel implementation for Memento.

Note, some of this code was taken from connman testing scripts and then modified
to meet our needs.
"""

import os
import dbus
import dbus.mainloop.glib
import gobject
import string
import time

def CurrentNetwork():
  """Returns the name of the network that the wifi interface is connected to.

  Returns:
    The AP name or None if the wifi card isn't connected to anything.
  """
  bus_loop = dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus(mainloop=bus_loop)
  manager = dbus.Interface(bus.get_object("org.moblin.connman", "/"),
      "org.moblin.connman.Manager")

  properties = manager.GetProperties()
  for path in properties["Devices"]:
    device = dbus.Interface(bus.get_object("org.moblin.connman", path),
              "org.moblin.connman.Device")
    properties = device.GetProperties()
    if properties["Type"] not in ["wifi"]:
      continue
    for path in properties["Networks"]:
      network = dbus.Interface(bus.get_object("org.moblin.connman", path),
          "org.moblin.connman.Network")
      properties = network.GetProperties()
      if properties["Connected"] == True and "WiFi.SSID" in properties:
        ssid = convert_ssid(properties["WiFi.SSID"])
        return ssid
  return None

def AddNetwork(ssid, passphrase="", encryption="none"):
  """Attempts to connect to a network using ConnMan.

    TODO(rtc): Support wep.
  """
  bus_loop = dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus(mainloop=bus_loop)
  manager = dbus.Interface(
      bus.get_object("org.moblin.connman", "/"), "org.moblin.connman.Manager")

  if passphrase == "":
    security = "none"
  else:
    security = "rsn"
  path = manager.ConnectService(({
          "Type": "wifi",
          "Mode": "managed",
          "SSID": ssid,
          "Security": security,
          "Passphrase": passphrase }));
  service = dbus.Interface(
      bus.get_object("org.moblin.connman", path), "org.moblin.connman.Service") 

  status = ""
  wait_count = 0
  while status != "ready" and status != "failure" and wait_count < 30:
    properties = service.GetProperties()
    status = properties.get("State", None)
    time.sleep(.3)
    wait_count += 1

  return status == "ready" 

def convert_ssid(ssid_list):
  """
    Taken from the connman debug scripts
  """
  ssid = ""
  for byte in ssid_list:
    if (str(byte) in string.printable):
      ssid = ssid + str(byte)
    else:
      ssid = ssid + "."
  return ssid

def Scan():
  """Scans for all nearby APs.

  Returns:
    A dictionary that maps an ssid to its properties.
    The properties are 'ssid' and 'encryption'
  """
  bus_loop = dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus(mainloop=bus_loop)
  manager = dbus.Interface(bus.get_object("org.moblin.connman", "/"),
      "org.moblin.connman.Manager")

  networks = {}
  properties = manager.GetProperties()
  for path in properties["Devices"]:
    device = dbus.Interface(bus.get_object("org.moblin.connman", path),
              "org.moblin.connman.Device")
    properties = device.GetProperties()
    if properties["Type"] not in ["wifi"]:
      continue
    for path in properties["Networks"]:
      network = dbus.Interface(bus.get_object("org.moblin.connman", path),
          "org.moblin.connman.Network")
      properties = network.GetProperties()
      # Skip hidden networks.
      if properties.has_key("WiFi.SSID") != True:
        continue
      ssid = convert_ssid(properties["WiFi.SSID"])
      encryption = properties["WiFi.Security"]
      strength = int(properties.get("Strength", 0))
      networks[ssid] = {
          "ssid":ssid,
          "encryption":encryption,
          "signal":strength }

  return networks

def IsOnline():
  """ Checks to see if we are connected to the network
    Returns True iff there is a network service running.
  """
  bus_loop = dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
  bus = dbus.SystemBus(mainloop=bus_loop)
  manager = dbus.Interface(bus.get_object("org.moblin.connman", "/"),
      "org.moblin.connman.Manager")
  properties = manager.GetProperties()
  return (properties["State"] == "online")

if __name__ == "__main__":
  print Scan()
