# Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import itertools

import make_shaders

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
for key in Split('CC CXX AR RANLIB LD NM'):
  value = os.environ.get(key)
  if value != None:
    base_env[key] = value
for key in Split('CFLAGS CCFLAGS CPPPATH LIBPATH'):
  value = os.environ.get(key)
  if value != None:
    base_env[key] = Split(value)
if not base_env.get('CCFLAGS'):
  base_env.Append(CCFLAGS=Split('-I.. -Wall -Wno-psabi -Werror -O3'))

# Fix issue with scons not passing some vars through the environment.
for key in Split('PKG_CONFIG_LIBDIR PKG_CONFIG_PATH SYSROOT'):
  if os.environ.has_key(key):
    base_env['ENV'][key] = os.environ[key]

# Unless we disable strict aliasing, we get warnings about some of the
# program's command line flags processing code that look like:
#   'dereferencing type-punned pointer will break strict-aliasing rules'
base_env.Append(CCFLAGS=['-fno-strict-aliasing'])

base_env.Append(CPPPATH=['..'])

base_env.Append(LIBS=Split('base chromeos rt'))

base_env.ParseConfig('pkg-config --cflags --libs x11')

# Fork off a new environment, add Cairo to it, and build the screenshot
# program.
screenshot_env = base_env.Clone()
screenshot_env.ParseConfig('pkg-config --cflags --libs cairo')
screenshot_env.Program('screenshot', 'screenshot.cc')

# Check for BACKEND on the build line
backend = ARGUMENTS.get('BACKEND', 'OPENGL').lower()

# Start a new environment for the window manager.
wm_env = base_env.Clone()

# Add a builder for .proto files
wm_env['BUILDERS']['ProtocolBuffer'] = proto_builder

wm_env.Append(LIBS=Split('gflags protobuf'))

wm_env.ParseConfig('pkg-config --cflags --libs gdk-2.0 libpcrecpp ' +
                   'xcb x11-xcb xcb-composite xcb-randr xcb-shape xcb-damage')

# Add builder for .glsl* files
if backend == 'opengles':
  make_shaders.AddBuildRules(wm_env)

if os.system('pkg-config clutter-1.0') == 0:
  wm_env.ParseConfig('pkg-config --cflags --libs clutter-1.0')
else:
  wm_env.ParseConfig('pkg-config --cflags --libs clutter-0.9')
# Make us still produce a usable binary (for now, at least) on Jaunty
# systems that are stuck at Clutter 0.9.2.
if os.system('pkg-config --exact-version=0.9.2 clutter-0.9') == 0:
  wm_env.Append(CPPDEFINES=['CLUTTER_0_9_2'])

# This is needed so that glext headers include glBindBuffer and
# related APIs.
if backend == 'opengl':
  wm_env.Append(CPPDEFINES=['GL_GLEXT_PROTOTYPES'])

wm_env.ProtocolBuffer('system_metrics.pb.cc', 'system_metrics.proto');

# Define an IPC library that will be used both by the WM and by client apps.
srcs = Split('''\
  atom_cache.cc
  real_x_connection.cc
  system_metrics.pb.cc
  util.cc
  wm_ipc.cc
  x_connection.cc
''')
if backend == 'opengl':
  srcs.append(Split('''\
    real_gl_interface.cc
  '''))
libwm_ipc = wm_env.Library('wm_ipc', srcs)

# Create a library with just the additional files needed by the window
# manager.  This is a bit ugly; we can't include any source files that are
# also compiled in different environments here (and hence we just get e.g.
# atom_cache.cc and util.cc via libwm_ipc).
srcs = Split('''\
  event_consumer_registrar.cc
  hotkey_overlay.cc
  image_container.cc
  key_bindings.cc
  layout_manager.cc
  metrics_reporter.cc
  mock_clutter_interface.cc
  motion_event_coalescer.cc
  panel.cc
  panel_bar.cc
  panel_dock.cc
  panel_manager.cc
  pointer_position_watcher.cc
  shadow.cc
  stacking_manager.cc
  window.cc
  window_manager.cc
''')
if backend == 'clutter':
  srcs.append(Split('''\
    clutter_interface.cc
  '''))
elif backend == 'opengl':
  srcs.append(Split('''\
    gl_interface_base.cc
    opengl_visitor.cc
  tidy_interface.cc
  '''))
elif backend == 'opengles':
  srcs.append(Split('''\
    gl_interface_base.cc
    tidy_interface.cc
    gles/opengles_visitor.cc
    gles/shader_base.cc
    gles/shaders.cc
    gles/real_gles2_interface.cc
  '''))
libwm_core = wm_env.Library('wm_core', srcs)

# Define a library to be used by tests.
srcs = Split('''\
  mock_gl_interface.cc
  mock_x_connection.cc
  test_lib.cc
''')
libtest = wm_env.Library('test', Split(srcs))

wm_env.Append(LIBS=[libwm_core, libwm_ipc])
if 'USE_BREAKPAD' in ARGUMENTS:
  wm_env.Append(CPPDEFINES=['USE_BREAKPAD'], LIBS=['libbreakpad'])

backend_defines = {'clutter': ['USE_CLUTTER'],
                   'opengl': ['TIDY_OPENGL', 'USE_TIDY'],
                   'opengles': ['TIDY_OPENGLES', 'USE_TIDY']}
wm_env.Append(CPPDEFINES=backend_defines[backend])

wm_env.Program('wm', 'main.cc')

test_env = wm_env.Clone()
test_env.Append(LIBS=['gtest'])
# libtest needs to be listed first since it depends on wm_core and wm_ipc.
test_env.Prepend(LIBS=[libtest])
tests = []

# These are tests that only get built when we use particular backends
backend_tests = {'clutter': [],
                 'opengl': ['tidy_interface_test.cc',
                            'opengl_visitor_test.cc'],
                 'opengles': []}
all_backend_tests = set(itertools.chain(*backend_tests.values()))
for test_src in Glob('*_test.cc', strings=True):
  if test_src in all_backend_tests and test_src not in backend_tests[backend]:
    continue
  tests += test_env.Program(test_src)
# Create a 'tests' target that will build all tests.
test_env.Alias('tests', tests)

mock_chrome_env = wm_env.Clone()
mock_chrome_env.ParseConfig('pkg-config --cflags --libs gtkmm-2.4')
mock_chrome_env.Program('mock_chrome', 'mock_chrome.cc')
