// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THEME_THEME_DRAW_H_
#define THEME_THEME_DRAW_H_

#include <gtk/gtk.h>

// Set of theme engine functions that Gtk ends up calling into. This is not a
// complete set, nor a complete implementation. We're only implementing the set
// of functions and arguments needed by the set of widgets used in Chrome.

void ThemeDrawArrow(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GtkShadowType shadow_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    GtkArrowType arrow_type,
                    gboolean fill,
                    gint x,
                    gint y,
                    gint w,
                    gint h);

void ThemeDrawBox(GtkStyle* style,
                  GdkWindow* window,
                  GtkStateType state_type,
                  GtkShadowType shadow_type,
                  GdkRectangle* area,
                  GtkWidget* widget,
                  const gchar* detail,
                  gint x,
                  gint y,
                  gint w,
                  gint h);

void ThemeDrawBoxGap(GtkStyle* style,
                     GdkWindow* window,
                     GtkStateType state_type,
                     GtkShadowType shadow_type,
                     GdkRectangle* area,
                     GtkWidget* widget,
                     const gchar* detail,
                     gint x,
                     gint y,
                     gint w,
                     gint h,
                     GtkPositionType gap_side,
                     gint gap_x,
                     gint gap_w);

void ThemeDrawCheck(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GtkShadowType shadow_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint x,
                    gint y,
                    gint w,
                    gint h);

void ThemeDrawExtension(GtkStyle* style,
                        GdkWindow* window,
                        GtkStateType state_type,
                        GtkShadowType shadow_type,
                        GdkRectangle* area,
                        GtkWidget* widget,
                        const gchar* detail,
                        gint x,
                        gint y,
                        gint w,
                        gint h,
                        GtkPositionType gap_side);

void ThemeDrawFlatBox(GtkStyle* style,
                      GdkWindow* window,
                      GtkStateType state_type,
                      GtkShadowType shadow_type,
                      GdkRectangle* area,
                      GtkWidget* widget,
                      const gchar* detail,
                      gint x,
                      gint y,
                      gint w,
                      gint h);

void ThemeDrawFocus(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint x,
                    gint y,
                    gint w,
                    gint h);

void ThemeDrawHline(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint x1,
                    gint x2,
                    gint y);

void ThemeDrawOption(GtkStyle* style,
                     GdkWindow* window,
                     GtkStateType state_type,
                     GtkShadowType shadow_type,
                     GdkRectangle* area,
                     GtkWidget* widget,
                     const gchar* detail,
                     gint x,
                     gint y,
                     gint w,
                     gint h);

void ThemeDrawShadow(GtkStyle* style,
                     GdkWindow* window,
                     GtkStateType state_type,
                     GtkShadowType shadow_type,
                     GdkRectangle* area,
                     GtkWidget* widget,
                     const gchar* detail,
                     gint x,
                     gint y,
                     gint w,
                     gint h);

void ThemeDrawSlider(GtkStyle* style,
                     GdkWindow* window,
                     GtkStateType state_type,
                     GtkShadowType shadow_type,
                     GdkRectangle* area,
                     GtkWidget* widget,
                     const gchar* detail,
                     gint x,
                     gint y,
                     gint w,
                     gint h,
                     GtkOrientation orientation);

void ThemeDrawVline(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint y1,
                    gint y2,
                    gint x);

#endif  // THEME_THEME_DRAW_H_

