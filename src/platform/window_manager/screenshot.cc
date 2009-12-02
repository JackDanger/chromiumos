// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <sstream>

#include <cairo/cairo.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "base/logging.h"

const static char* kUsage =
    "Usage: screenshot FILENAME [WINDOW]\n"
    "\n"
    "Writes the contents of the root window, by default, or a client\n"
    "window, if supplied (as a hexadecimal X ID), to a file.\n";

int main(int argc, char** argv) {
  Display* display = XOpenDisplay(NULL);
  CHECK(display);

  if (argc == 1 || argc > 3) {
    fprintf(stderr, "%s", kUsage);
    return 1;
  }

  const char* filename = argv[1];

  Window win = None;
  if (argc == 2) {
    win = DefaultRootWindow(display);
  } else {
    std::istringstream input(argv[2]);
    if ((input >> std::hex >> win).fail()) {
      fprintf(stderr, "%s", kUsage);
      return 1;
    }
  }

  XWindowAttributes attr;
  CHECK(XGetWindowAttributes(display, win, &attr) != 0);
  XImage* image = XGetImage(
      display, win, 0, 0, attr.width, attr.height, AllPlanes, ZPixmap);
  CHECK(image);
  CHECK(image->depth == 24) << "Unsupported image depth " << image->depth;

  cairo_surface_t* surface =
      cairo_image_surface_create_for_data(
          reinterpret_cast<unsigned char*>(image->data),
          CAIRO_FORMAT_RGB24,
          image->width,
          image->height,
          image->bytes_per_line);
  CHECK(surface) << "Unable to create Cairo surface from XImage data";
  CHECK(cairo_surface_write_to_png(surface, filename) == CAIRO_STATUS_SUCCESS);
  cairo_surface_destroy(surface);

  XDestroyImage(image);
  XCloseDisplay(display);
  return 0;
}
