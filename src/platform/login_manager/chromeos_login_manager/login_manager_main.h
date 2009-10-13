// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromeos_login_manager/pam_client.h"
#include "third_party/chromeos_login_manager/textfield_controller.h"
#include "views/widget/widget_gtk.h"
#include "views/accelerator.h"

#ifndef LOGIN_MANAGER_MAIN_H_
#define LOGIN_MANAGER_MAIN_H_

class LoginManagerMain {
  friend class TextfieldController;

  public:
    LoginManagerMain();
    virtual ~LoginManagerMain() {}

    // Run main loop
    virtual void Run();

    // Creates all examples and start UI event loop.
  private:
    // Creates top level window
    views::Widget* main_window_;
    PamClient* pam_;
    PamClient::UserCredentials user_credentials_;
    views::Textfield username_field_;
    views::Textfield password_field_;

    // Helper functions to modularize class
    bool InitPam();
    void CreateWindow();
    views::Widget* CreateTopLevelWidget();

    DISALLOW_COPY_AND_ASSIGN(LoginManagerMain);
};

#endif /* LOGIN_MANAGER_MAIN_H_ */
