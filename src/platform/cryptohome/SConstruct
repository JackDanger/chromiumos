# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

env = Environment()

# define generic dbus server/client bindings builders
dbus_server_builder = Builder(action = 'dbus-binding-tool --mode=glib-server --prefix=`basename $SOURCE .xml` $SOURCE > $TARGET')
dbus_client_builder = Builder(action = 'dbus-binding-tool --mode=glib-client --prefix=`basename $SOURCE .xml` $SOURCE > $TARGET')
env.Append(BUILDERS = { 'DbusServerBindings' : dbus_server_builder,
                        'DbusClientBindings' : dbus_client_builder})
# setup sources
serverlib_sources = env.Split("""service.cc
                                 interface.cc
                              """)
server_sources = env.Split("""cryptohomed.cc""")
example_sources = env.Split("""example_client.cc
                            """)
test_sources = env.Split("""service_unittest.cc
                            cryptohome_testrunner.cc
                         """)



env.Append(
    CPPPATH=['.', '..'],
    CPPFLAGS=['-pie', '-fstack-protector-all', '-m32',
              '-fno-exceptions', '-O2', '-Wall', '-Werror'],
    LINKFLAGS=['-m32' ],
    LIBS = ['base', 'chromeos', 'rt'],
)
env.ParseConfig('pkg-config --cflags --libs gobject-2.0 dbus-1 dbus-glib-1')

env_serverlib = env.Clone()
env_serverlib.DbusServerBindings('bindings/server.h', 'cryptohome.xml')
env_serverlib.SharedLibrary('cryptohome_service', serverlib_sources)

env_server = env.Clone()
env_server.Append(LIBPATH = ['.'], LIBS=['cryptohome_service'])
env_server.Program('cryptohomed', server_sources)

# TODO(wad) we'll probably want a separate runner for client roundtrip tests
env_tests = env.Clone()
env_tests.Append(LIBPATH = ['.'], LIBS=['cryptohome_service', 'gtest'])
env_tests.Program('cryptohome_testrunner', test_sources)

env_client = env.Clone()
env_client.DbusClientBindings('bindings/client.h', 'cryptohome.xml')
env_client.Append(LIBPATH = ['.'])
env_client.Program('example_client', example_sources)
