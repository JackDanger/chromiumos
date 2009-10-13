// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromeos_login_manager/textfield_controller.h"
#include <gdk/gdk.h>
#include "third_party/chromeos_login_manager/login_manager_main.h"
#include "base/keyboard_codes.h"
#include "base/message_loop.h"

TextfieldController::TextfieldController(LoginManagerMain* login_manager)
    : login_manager_(login_manager) {}

bool TextfieldController::HandleKeystroke(views::Textfield* s,
    const views::Textfield::Keystroke& keystroke) {
  if (keystroke.GetKeyboardCode() == base::VKEY_RETURN) {
    string16 username, password;

    username = login_manager_->username_field_.text();
    password = login_manager_->password_field_.text();

    // Disallow 0 size username | passwords
    if (username.length() == 0 || password.length() == 0) {
      // Return true so that processing ends
      return true;
    } else {
      // Set up credentials to prepare for authentication.  Also
      // perform wstring to string conversion
      login_manager_->user_credentials_.username.assign(username.begin(),
                                                        username.end());
      login_manager_->user_credentials_.password.assign(password.begin(),
                                                        password.end());
      if (login_manager_->pam_->Authenticate()) {
        MessageLoopForUI::current()->Quit();
      }
      // Return true so that processing ends
      return true;
    }
  } else {
    // Return false so that processing does not end
    return false;
  }
}
