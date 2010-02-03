# Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

env = Environment()
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value
for key in Split('CFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key)
  if value != None:
    env[key] = Split(value)
if not env.get('CCFLAGS'):
  env.Append(CCFLAGS=Split('-I.. -Wall -Werror -O3'))

# Fix issue with scons not passing pkg-config vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

env.Append(CPPPATH=['..'])
env.Append(LIBS=Split('base chromeos rt Xtst'))
env.ParseConfig('pkg-config --cflags --libs libpcrecpp x11')
env.Program('autox', Split('autox.cc script_runner.cc'))
