// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/atom_cache.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/event_consumer.h"
#include "window_manager/key_bindings.h"
#include "window_manager/layout_manager.h"
#include "window_manager/motion_event_coalescer.h"
#include "window_manager/panel.h"
#include "window_manager/panel_container.h"
#include "window_manager/shadow.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/util.h"
#include "window_manager/window.h"
#include "window_manager/wm_ipc.h"
#include "window_manager/x_connection.h"

// This is not an executable test.  Rather, it is code that should fail to
// compile if symbols from Xlib leak into any of a number of
// commonly-included window manager header files.
//
// This is accomplished by declaring a Colormap type within the
// window_manager namespace with the same name as the Colormap type from
// Xlib's X.h file.  When we try to pull window_manager::Colormap into the
// global namespace, the compiler will complain if Xlib's symbol is already
// there.
namespace window_manager {
typedef unsigned int Colormap;
}

// If you receive a compilation error from the following line, please
// review your changes and find a way to avoid including <X11/X.h> in
// commonly-used header files.
using window_manager::Colormap;

int main(int argc, char** argv) {
  return 0;
}
