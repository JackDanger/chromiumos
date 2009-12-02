# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

Help('''\
Type: 'scons' to build and 'scons -c' to clean\
''')

""" Inputs:
        target: list of targets to compile to
        source: list of sources to compile
        env: the scons environment in which we are compiling
    Outputs:
        target: the list of targets we'll emit
        source: the list of sources we'll compile"""
def ProtocolBufferEmitter(target, source, env):
  output = str(source[0])
  output = output[0:output.rfind('.proto')]
  target = [
    output + '.pb.cc',
    output + '.pb.h',
  ]
  return target, source

""" Inputs:
        source: list of sources to process
        target: list of targets to generate
        env: scons environment in which we are working
        for_signature: unused
    Outputs: a list of commands to execute to generate the targets from
             the sources."""
def ProtocolBufferGenerator(source, target, env, for_signature):
  commands = [
    '/usr/bin/protoc '
    ' --proto_path . ${SOURCES} --cpp_out .']
  return commands

proto_builder = Builder(generator = ProtocolBufferGenerator,
                        emitter = ProtocolBufferEmitter,
                        single_source = 1,
                        suffix = '.pb.cc')

# Create a base environment including things that are likely to be common
# to all of the objects in this directory. We pull in overrides from the
# environment to enable cross-compile.
base_env = Environment()
for key in Split('CC CXX AR RANLIB LD NM CFLAGS CCFLAGS'):
  value = os.environ.get(key)
  if value != None:
    base_env[key] = value
if not base_env.has_key('CCFLAGS'):
  base_env['CCFLAGS'] = '-I.. -Wall -Werror -O3'
base_env['LINKFLAGS'] = '-lgflags -lprotobuf'

# Fix up the pkg-config path if it is present in the environment.
if os.environ.has_key('PKG_CONFIG_PATH'):
  base_env['ENV']['PKG_CONFIG_PATH'] = os.environ['PKG_CONFIG_PATH']

# Add a builder for .proto files
base_env['BUILDERS']['ProtocolBuffer'] = proto_builder

# Unless we disable strict aliasing, we get warnings about some of the
# program's command line flags processing code that look like:
#   'dereferencing type-punned pointer will break strict-aliasing rules'
base_env.Append(CCFLAGS=' -fno-strict-aliasing')

# We include files relative to the parent directory.  Let SCons know about
# this so it'll recompile as needed when a header changes.
base_env['CPPPATH'] = ['..',
                       '../../third_party/chrome/files',
                       '../../common']

base_env['LIBPATH'] = ['../../third_party/chrome',
                       '../../common']

base_env['LIBS'] = ['base', 'chromeos', 'rt']

base_env.ParseConfig('pkg-config --cflags --libs x11 libpcrecpp ' +
                     'gdk-2.0')

base_env.ProtocolBuffer('system_metrics.pb.cc', 'system_metrics.proto');

base_env.Program('screenshot', 'screenshot.cc')

# Define an IPC library that will be used both by the WM and by client apps.
srcs = Split('''\
  atom_cache.cc
  real_x_connection.cc
  system_metrics.pb.cc
  util.cc
  wm_ipc.cc
  x_connection.cc
''')
libwm_ipc = base_env.Library('wm_ipc', srcs)

# Define a library to be used by tests -- we build it in the same
# environment as libwm_ipc so we can re-list util.cc and x_connection.c to
# avoid linking errors.  Surely there's a better way to do this.
srcs = Split('''\
  mock_x_connection.cc
  test_lib.cc
  util.cc
  x_connection.cc
''')
libtest = base_env.Library('test', Split(srcs))

# Now create a new environment and add things needed just by the window
# manager.
wm_env = base_env.Clone()
wm_env.ParseConfig('pkg-config --cflags --libs xcomposite gdk-2.0')
if os.system('pkg-config clutter-1.0') == 0:
  wm_env.ParseConfig('pkg-config --cflags --libs clutter-1.0')
else:
  wm_env.ParseConfig('pkg-config --cflags --libs clutter-0.9')

# Make us still produce a usable binary (for now, at least) on Jaunty
# systems that are stuck at Clutter 0.9.2.
if os.system('pkg-config --exact-version=0.9.2 clutter-0.9') == 0:
  wm_env.Append(CCFLAGS='-DCLUTTER_0_9_2')

# Create a library with just the additional files needed by the window
# manager.  This is a bit ugly; we can't include any source files that are
# also compiled in different environments here (and hence we just get e.g.
# atom_cache.cc and util.cc via libwm_ipc).
srcs = Split('''\
  clutter_interface.cc
  hotkey_overlay.cc
  key_bindings.cc
  layout_manager.cc
  metrics_reporter.cc
  motion_event_coalescer.cc
  panel.cc
  panel_bar.cc
  shadow.cc
  window.cc
  window_manager.cc
''')
libwm_core = wm_env.Library('wm_core', srcs)

wm_env['LIBS'] += [libwm_core, libwm_ipc]
wm_env.Program('wm', 'main.cc')
# This is weird.  We get linking errors about StringPrintf() unless
# libbase.a appears at the end of the command line, so add it again here.
wm_env.Append(LIBS=File('../../third_party/chrome/libbase.a'))

test_clutter = wm_env.Clone()
test_clutter.ParseConfig('pkg-config --cflags --libs gtk+-2.0')
test_clutter.Program('test_clutter', 'test_clutter.cc')

test_env = wm_env.Clone()
test_env['LINKFLAGS'].append('-lgtest')
test_env['LIBS'] += [libtest]
test_env.Program('key_bindings_test', 'key_bindings_test.cc')
test_env.Program('layout_manager_test', 'layout_manager_test.cc')
test_env.Program('shadow_test', 'shadow_test.cc')
test_env.Program('window_test', 'window_test.cc')
test_env.Program('window_manager_test', 'window_manager_test.cc')
test_env.Program('util_test', 'util_test.cc')

mock_chrome_env = base_env.Clone()
mock_chrome_env['LIBS'] += libwm_ipc
mock_chrome_env.ParseConfig('pkg-config --cflags --libs gtkmm-2.4')
mock_chrome_env.Program('mock_chrome', 'mock_chrome.cc')
