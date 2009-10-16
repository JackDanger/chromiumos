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
        '../../build/linux/system.gyp:x11',
        '../../base/base.gyp:base',
        '../../skia/skia.gyp:skia',
        '../../views/views.gyp:views',
        '../cros/cros_api.gyp:cros_api',
        '../icu/icu.gyp:icui18n',
        '../icu/icu.gyp:icuuc',
        '../../chrome/chrome.gyp:chrome_resources',
      ],
      'include_dirs': [
        '../..',        
      ],
      'libraries': [
        'pam',
      ],
      'sources': [        
        'login_manager_main.cc',
        'login_manager_window.cc',
        'textfield_controller.cc',
        'pam_client.cc',        
        '../../chrome/browser/chromeos/network_menu_button.cc',
        '../../chrome/browser/chromeos/cros_network_library.cc',
        '../../chrome/browser/chromeos/cros_library.cc',
        '../../chrome/browser/chromeos/password_dialog_view.cc',
        '../../chrome/browser/chrome_thread.cc',                        
      ],                          
    },
  ],
}
