# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'chromium_code': 1,
  },
  'target_defaults': {
    'sources/': [
      ['exclude', '/(cocoa|win)/'],
      ['exclude', '_(cocoa|mac|win)\\.(cc|mm?)$'],
      ['exclude', '/(win)_[^/]*\\.cc$'],
      ['include', '/gtk/'],
      ['include', '_(gtk|linux|posix|skia|x)\\.cc$'],
      ['include', '/(gtk|x11)_[^/]*\\.cc$'],
    ],    
  }, 
  'targets': [
    {
      'target_name': 'chromeos_login_manager',
      'type': 'executable',
      'dependencies': [
        '../../build/linux/system.gyp:gtk',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../views/views.gyp:views',        
      ],
      'include_dirs': [
        '../..',
      ],
      'sources': [
        'login_manager_main.h',
        'login_manager_main.cc',
      ],                          
    },
  ],
}
