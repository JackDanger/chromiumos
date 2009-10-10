#!/usr/bin/python

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Some wrapper code for interacting with the touchpad
"""

import os

def GetSynclientValue(key):
  binary = "/usr/bin/synclient -l | grep %s" % key
  line = os.popen(binary).readline()
  return line.split('=')[1].strip()

def SetSynclientValue(key, value):
  binary = '/usr/bin/synclient %s=%s' % (key, value)
  os.popen(binary)

def GetTapToClick():
  return int(GetSynclientValue('MaxTapTime')) > 0

def SetTapToClick(taptoclick):
  SetSynclientValue('MaxTapTime', (taptoclick and 180 or 0))

def GetVertEdgeScroll():
  return int(GetSynclientValue('VertEdgeScroll')) > 0

def SetVertEdgeScroll(vertedgescroll):
  SetSynclientValue('VertEdgeScroll', (vertedgescroll and 1 or 0))
