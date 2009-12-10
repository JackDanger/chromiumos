# -*- python -*-

# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

env = Environment()

driver_sources = env.Split("""microbenchmark.cc
                              main.cc""")

benchmark_sources = env.Split("""benchmarks/getpid.cc
                                 benchmarks/readwrite.cc""")

env.Append(
    CPPPATH=['..', '../../third_party/chrome/files', '../../common'],
    CCFLAGS=['-g', '-fno-exceptions', '-Wall', '-Werror'],
    LIBPATH=['../../third_party/chrome'],
    LIBS=['base', 'pthread', 'rt', File('/usr/lib/libgtest.a')],
)

env_bin = env.Clone()
env_bin.Program('microbenchmark_runner', driver_sources + benchmark_sources)
