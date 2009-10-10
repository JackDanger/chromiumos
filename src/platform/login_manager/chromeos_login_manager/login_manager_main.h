// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/widget_gtk.h"
#include "views/accelerator.h"

#ifndef LOGIN_MANAGER_MAIN_H_
#define LOGIN_MANAGER_MAIN_H_

class LoginManagerMain : public views::AcceleratorTarget {
  public:
    LoginManagerMain() {}
    virtual ~LoginManagerMain() {}

    virtual bool AcceleratorPressed(const views::Accelerator& accelerator);

    // Creates all examples and start UI event loop.
    void Run();
  private:
    // Creates top level window
    views::Widget* CreateTopLevelWidget();
    DISALLOW_COPY_AND_ASSIGN(LoginManagerMain);
};

#endif /* LOGIN_MANAGER_MAIN_H_ */
