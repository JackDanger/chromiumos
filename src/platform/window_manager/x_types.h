// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_X_TYPES_H_
#define WINDOW_MANAGER_X_TYPES_H_

// This header contains renamed definitions of various POD types from Xlib.
// Code (and headers in particular) should include this header rather than
// Xlib.h whenever possible to avoid pulling in all of the additional
// (generically-named) stuff from Xlib.

typedef unsigned long XID;

typedef XID KeySym;
typedef XID XAtom;
typedef XID XDamage;
typedef XID XDrawable;
typedef XID XPixmap;
typedef XID XServerRegion;
typedef XID XVisualID;
typedef XID XWindow;

typedef unsigned char KeyCode;

typedef unsigned long XTime;

#endif
