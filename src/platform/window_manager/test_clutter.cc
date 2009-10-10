// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>
extern "C" {
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <gtk/gtkmain.h>
#include <X11/Xlib.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xcomposite.h>
}
#include <glog/logging.h>
#include <iostream>
#include <string>


using namespace std;

typedef ::Window XWindow;

static const char* kUsage =
    "Usage: test_clutter [mode]\n"
    "\n"
    "[mode] must be one of the following strings:\n"
    "\n"
    "  foreign\n"
    "    - Use clutter_x11_set_stage_foreign() to tell Clutter's stage to\n"
    "      target the XComposite overlay window.\n"
    "  fullscreen\n"
    "    - Call clutter_stage_fullscreen().  I think that this will only\n"
    "      work if you're already running a window manager.\n"
    "  raise\n"
    "    - Just treat the stage like a regular window.  Call XRaiseWindow()\n"
    "      to put it at the top of the stack.\n"
    "  reparent\n"
    "    - Reparent the stage's X window into the overlay window.\n";

// Different things we can try to get Clutter to work. :-(
enum Mode {
  MODE_FOREIGN,
  MODE_FULLSCREEN,
  MODE_RAISE,
  MODE_REPARENT,
};

// Remove an X window's input region.
void RemoveInputRegion(Display* display, Window xid) {
  XShapeCombineRectangles(display,
                          xid,
                          ShapeInput,
                          0,     // x_off
                          0,     // y_off
                          NULL,  // rectangles
                          0,     // n_rects
                          ShapeSet,
                          Unsorted);
}

ClutterActor* InitCanvas(Mode mode) {
  Display* display = GDK_DISPLAY();
  Window root = GDK_ROOT_WINDOW();

  XWindow root_ret;
  int x, y;
  unsigned int width, height, border_width, depth;
  XGetGeometry(display, root, &root_ret, &x, &y,
               &width, &height, &border_width, &depth);
  LOG(INFO) << "Root window is " << width << "x" << height;

  ClutterActor* stage = clutter_stage_get_default();
  Window stage_window = clutter_x11_get_stage_window(CLUTTER_STAGE(stage));
  ClutterColor stage_color = { 0x40, 0x20, 0x90, 0xff };
  clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_color);
  clutter_actor_set_size(stage, width, height);

  if (mode == MODE_FOREIGN) {
    Window overlay = XCompositeGetOverlayWindow(display, root);
    RemoveInputRegion(display, overlay);
    clutter_x11_set_stage_foreign(CLUTTER_STAGE(stage), overlay);
  } else if (mode == MODE_FULLSCREEN) {
#ifdef CLUTTER_0_9_2
    clutter_stage_fullscreen(CLUTTER_STAGE(stage));
#else
    clutter_stage_set_fullscreen(CLUTTER_STAGE(stage), TRUE);
#endif
  } else if (mode == MODE_RAISE) {
    XRaiseWindow(display, stage_window);
  } else if (mode == MODE_REPARENT) {
    Window overlay = XCompositeGetOverlayWindow(display, root);
    RemoveInputRegion(display, overlay);
    XReparentWindow(display, stage_window, overlay, 0, 0);
  } else {
    CHECK(false);
  }

  clutter_actor_show_all(stage);
  return stage;
}

// Callback for key presses on the stage widget.
static gboolean HandleKeyPress(GtkWidget* widget,
                               GdkEvent* event,
                               gpointer data) {
  LOG(INFO) << "Key pressed -- exiting";
  exit(0);
}

// Callback for GDK events.
static GdkFilterReturn FilterEvent(GdkXEvent* xevent,
                                   GdkEvent* event,
                                   gpointer data) {
  XEvent* xe = reinterpret_cast<XEvent*>(xevent);
  if (xe->type == KeyPress) {
    LOG(INFO) << "Key pressed -- exiting";
    exit(0);
  }
  return GDK_FILTER_CONTINUE;
}

int main(int argc, char** argv) {
  gdk_init(&argc, &argv);
  clutter_init(&argc, &argv);
  google::InitGoogleLogging(argv[0]);

  if (argc != 2) {
    cerr << kUsage;
    return 1;
  }
  string mode_str = argv[1];
  Mode mode;
  if (mode_str == "foreign") {
    mode = MODE_FOREIGN;
  } else if (mode_str == "fullscreen") {
    mode = MODE_FULLSCREEN;
  } else if (mode_str == "raise") {
    mode = MODE_RAISE;
  } else if (mode_str == "reparent") {
    mode = MODE_REPARENT;
  } else {
    cerr << kUsage;
    return 1;
  }

  ClutterActor* stage = InitCanvas(mode);

  // Get key press events from the stage and also ask for all events from
  // the root window.
  g_signal_connect(stage, "key-press-event", G_CALLBACK(HandleKeyPress), NULL);
  gdk_window_add_filter(NULL, FilterEvent, NULL);

  ClutterColor rect_color = { 0xa0, 0x0, 0x0, 0xff };
  ClutterActor* rect = clutter_rectangle_new_with_color(&rect_color);
  clutter_actor_set_position(rect, 200, 200);
  clutter_actor_set_size(rect, 200, 200);
  clutter_actor_show(rect);
  clutter_container_add_actor(CLUTTER_CONTAINER(stage), rect);
  clutter_actor_animate(rect, CLUTTER_LINEAR, 20000,
                        "x", 800.0,
                        "y", 800.0,
                        NULL);

  LOG(INFO) << "Entering main loop";
  clutter_main();

  g_object_unref(rect);
  return 0;
}
