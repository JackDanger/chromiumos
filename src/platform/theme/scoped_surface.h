// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THEME_SCOPED_SURFACE_H_
#define THEME_SCOPED_SURFACE_H_

#include <gtk/gtk.h>

namespace theme {

// ScopedSurface creates a cairo_t from it's constructor and deletes it in it's
// destructor. ScopedSurface is intended to be used on the stack to manage the
// lifetime of a cairo_t.
class ScopedSurface {
 public:
  ScopedSurface(GdkWindow* window, GdkRectangle* area) {
    cr_ = gdk_cairo_create(window);
    if (area) {
      cairo_rectangle(cr_, area->x, area->y, area->width, area->height);
      cairo_clip(cr_);
      cairo_new_path(cr_);
    }
  }

  ~ScopedSurface() {
    cairo_destroy(cr_);
  }

  cairo_t* get() const { return cr_; }

 private:
  cairo_t* cr_;

  // DISALLOW_COPY_AND_ASSIGN
  void operator=(const ScopedSurface& cr);
  ScopedSurface(const ScopedSurface& cr);
};

}  // namespace theme

#endif  // THEME_SCOPED_SURFACE_H_
