// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TEXTFIELD_CONTROLLER_H_
#define TEXTFIELD_CONTROLLER_H_

#include "views/controls/textfield/textfield.h"

class LoginManagerMain;

class TextfieldController : public views::Textfield::Controller {
 public:
  explicit TextfieldController(LoginManagerMain* login_manager);

  // This method is called whenever the text in the field changes.
  virtual void ContentsChanged(views::Textfield* sender,
                               const string16& new_contents) {}

  // We override this method to handle enter in the textfield
  virtual bool HandleKeystroke(views::Textfield* sender,
                               const views::Textfield::Keystroke& keystroke);
 private:
  LoginManagerMain* login_manager_;
};

#endif /* TEXTFIELD_CONTROLLER_H_ */
