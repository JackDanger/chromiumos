# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

SOURCES=['chromeos/dbus/dbus.cc',
         'chromeos/string.cc',
         'chromeos/utility.cc']

env = Environment(
    CPPPATH=[ '.', '../third_party/chrome/files' ],
    CCFLAGS=['-fno-exceptions', '-fPIC'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value

# Fix issue with scons not passing pkg-config vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH'):
  if os.environ.has_key(key):
    env['ENV'][key] = os.environ[key]

# glib and dbug environment
env.ParseConfig('pkg-config --cflags --libs dbus-1 glib-2.0 dbus-glib-1')
env.StaticLibrary('chromeos', SOURCES)

# Unit test
if ARGUMENTS.get('debug', 0):
  env.Append(
    CCFLAGS = ['-fprofile-arcs', '-ftest-coverage', '-fno-inline'],
    LIBS = ['gcov'],
  )

env_test = env.Clone()

env_test.Append(
    LIBS = ['gtest', 'chromeos', 'base', 'rt'],
    LIBPATH = ['.', '../third_party/chrome'],
  )
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env_test[key] = value

unittest_sources =['chromeos/glib/object_unittest.cc']
unittest_main = ['testrunner.cc']
unittest_cmd = env_test.Program('unittests',
                           unittest_sources + unittest_main)

Clean(unittest_cmd, Glob('*.gcda') + Glob('*.gcno') + Glob('*.gcov') +
                    Split('html app.info'))

