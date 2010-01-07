# -*- python -*-

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

env = Environment()

driver_sources = env.Split("""microbenchmark.cc
                              main.cc""")

# TODO: References to ../../third_party/chrome/ should be replaced by using
# installed packages in the chroot.
env.Append(
    CPPPATH=['..', '../../third_party/chrome/files', '../../common'],
    CCFLAGS=['-g', '-fno-exceptions', '-Wall', '-Werror'],
    LIBPATH=['../../third_party/chrome'],
    # These need to be included by anyone linking against _main.
    LIBS=['base', 'gtest', 'rt', 'pthread'],
)
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    env[key] = value

env_mainlib = env.Clone()
env_mainlib.StaticLibrary('microbenchmark_main', driver_sources)
