// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromeos_login_manager/login_manager_window.h"
#include "views/widget/widget_gtk.h"

LoginManagerWindow::LoginManagerWindow()
    : views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW) {
}

void LoginManagerWindow::Init(const gfx::Rect& dimensions) {
  views::WidgetGtk::Init(NULL, dimensions);
}

LoginManagerWindow::~LoginManagerWindow() {}
