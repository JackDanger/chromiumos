// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_WINDOW_H_
#define LOGIN_MANAGER_WINDOW_H_

#include "views/window/window_gtk.h"

class LoginManagerWindow : public views::WidgetGtk {
 public:
  LoginManagerWindow();
  virtual void Init(const gfx::Rect& dimensions);
  virtual ~LoginManagerWindow();
};

#endif /* LOGIN_MANAGER_WINDOW_H_ */
