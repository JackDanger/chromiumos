// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cairo.h>
#include <math.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <string.h>

#include "theme/scoped_pattern.h"
#include "theme/scoped_surface.h"
#include "theme/theme_draw.h"

using theme::ScopedSurface;
using theme::ScopedPattern;

// Common colors first:

// Border color used for many widgets.
static const double BASE_STROKE_R = static_cast<double>(0x8F) / 255.0;
static const double BASE_STROKE_G = static_cast<double>(0x8F) / 255.0;
static const double BASE_STROKE_B = static_cast<double>(0x8F) / 255.0;

// Disabled border color used for many widgets.
static const double DISABLED_BASE_STROKE_R = static_cast<double>(0xB7) / 255.0;
static const double DISABLED_BASE_STROKE_G = static_cast<double>(0xB7) / 255.0;
static const double DISABLED_BASE_STROKE_B = static_cast<double>(0xB7) / 255.0;

static const double FOCUSED_STROKE_R = static_cast<double>(0x50) / 255.0;
static const double FOCUSED_STROKE_G = static_cast<double>(0x7a) / 255.0;
static const double FOCUSED_STROKE_B = static_cast<double>(0xD5) / 255.0;

// Common gradient stop and colors.
static const double GRADIENT_STOP_0 = 0;
static const double GRADIENT_STOP_1 = .5;
static const double GRADIENT_STOP_2 = 1.0;
static const double GRADIENT_R0 = 1.0;
static const double GRADIENT_G0 = 1.0;
static const double GRADIENT_B0 = 1.0;
static const double GRADIENT_R1 = 1.0;
static const double GRADIENT_G1 = 1.0;
static const double GRADIENT_B1 = 1.0;
static const double GRADIENT_R2 = static_cast<double>(0xD8) / 255.0;
static const double GRADIENT_G2 = static_cast<double>(0xD8) / 255.0;
static const double GRADIENT_B2 = static_cast<double>(0xD8) / 255.0;

static const double PRESSED_GRADIENT_R0 = static_cast<double>(0x95) / 255.0;
static const double PRESSED_GRADIENT_G0 = static_cast<double>(0x95) / 255.0;
static const double PRESSED_GRADIENT_B0 = static_cast<double>(0x95) / 255.0;
static const double PRESSED_GRADIENT_R1 = static_cast<double>(0xE3) / 255.0;
static const double PRESSED_GRADIENT_G1 = static_cast<double>(0xE3) / 255.0;
static const double PRESSED_GRADIENT_B1 = static_cast<double>(0xE3) / 255.0;

// Color used for selected text and a couple of other things.
static const double SELECTED_TEXT_BG_R = static_cast<double>(0xDC) / 255.0;
static const double SELECTED_TEXT_BG_G = static_cast<double>(0xE4) / 255.0;
static const double SELECTED_TEXT_BG_B = static_cast<double>(0xFA) / 255.0;

// Radius of the rounded rects drawn.
static const int BORDER_CORNER_RADIUS = 3;

// Stroke width when focused.
static const int FOCUSED_STROKE_WIDTH = 2;

// Stroke width when not focused.
static const int STROKE_WIDTH = 1;

// Then per widget colors/settings.

static const int COMBOBOX_IDEAL_ARROW_SIZE = 7;

static const double H_SEPARATOR_R0 = static_cast<double>(0xDA) / 255.0;
static const double H_SEPARATOR_G0 = static_cast<double>(0xDA) / 255.0;
static const double H_SEPARATOR_B0 = static_cast<double>(0xDA) / 255.0;
static const double H_SEPARATOR_R1 = static_cast<double>(0xF8) / 255.0;
static const double H_SEPARATOR_G1 = static_cast<double>(0xF8) / 255.0;
static const double H_SEPARATOR_B1 = static_cast<double>(0xF8) / 255.0;

static const double H_SLIDER_TRACK_R = static_cast<double>(0xDF) / 255.0;
static const double H_SLIDER_TRACK_G = static_cast<double>(0xDF) / 255.0;
static const double H_SLIDER_TRACK_B = static_cast<double>(0xDF) / 255.0;

static const double H_SLIDER_TRACK_FILL_R = 1;
static const double H_SLIDER_TRACK_FILL_G = 1;
static const double H_SLIDER_TRACK_FILL_B = 1;

static const double H_SLIDER_TRACK_HEIGHT = 6;

static const double INDICATOR_STROKE_DISABLED_R =
    static_cast<double>(0xB4) / 255.0;
static const double INDICATOR_STROKE_DISABLED_G =
    static_cast<double>(0xB4) / 255.0;
static const double INDICATOR_STROKE_DISABLED_B =
    static_cast<double>(0xB4) / 255.0;

// TODO: these are wrong, what should they be?
static const double INDICATOR_STROKE_PRESSED_R =
    static_cast<double>(0x0) / 255.0;
static const double INDICATOR_STROKE_PRESSED_G =
    static_cast<double>(0x0) / 255.0;
static const double INDICATOR_STROKE_PRESSED_B =
    static_cast<double>(0x0) / 255.0;

static const double INDICATOR_STROKE_R = 0;
static const double INDICATOR_STROKE_G = 0;
static const double INDICATOR_STROKE_B = 0;

static const double MENU_BG_R = 1;
static const double MENU_BG_G = 1;
static const double MENU_BG_B = 1;

static const double MENU_BG_HIGHLIGHT_R = SELECTED_TEXT_BG_R;
static const double MENU_BG_HIGHLIGHT_G = SELECTED_TEXT_BG_G;
static const double MENU_BG_HIGHLIGHT_B = SELECTED_TEXT_BG_B;

static const double MENU_BORDER_R = static_cast<double>(0x55) / 255.0;
static const double MENU_BORDER_G = static_cast<double>(0x55) / 255.0;
static const double MENU_BORDER_B = static_cast<double>(0x55) / 255.0;

// Ideal arrow size for menus.
static const int MENU_IDEAL_ARROW_SIZE = 5;

// Ideal size of the inner circle for selected radio buttons.
static const int MENU_RADIO_BUTTON_INDICATOR_IDEAL_SIZE = 5;

// Ideal size of the inner circle for selected radio buttons.
static const int RADIO_BUTTON_INDICATOR_IDEAL_SIZE = 7;

static const double RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_R0 =
    static_cast<double>(0xB4) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_G0 =
    static_cast<double>(0xB4) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_B0 =
    static_cast<double>(0xB4) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_R1 =
    static_cast<double>(0xB7) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_G1 =
    static_cast<double>(0xB7) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_B1 =
    static_cast<double>(0xB7) / 255.0;

// TODO: these are wrong, what should they be?
static const double RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_R0 =
    static_cast<double>(0xFF) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_G0 =
    static_cast<double>(0xFF) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_B0 =
    static_cast<double>(0xFF) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_R1 =
    static_cast<double>(0xFF) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_G1 =
    static_cast<double>(0xFF) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_B1 =
    static_cast<double>(0xFF) / 255.0;

static const double RADIO_BUTTON_INDICATOR_GRADIENT_R0 = 0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_G0 = 0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_B0 = 0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_R1 =
    static_cast<double>(0x83) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_G1 =
    static_cast<double>(0x83) / 255.0;
static const double RADIO_BUTTON_INDICATOR_GRADIENT_B1 =
    static_cast<double>(0x83) / 255.0;

static const double SCROLLBAR_ARROW_BORDER_R = BASE_STROKE_R;
static const double SCROLLBAR_ARROW_BORDER_G = BASE_STROKE_G;
static const double SCROLLBAR_ARROW_BORDER_B = BASE_STROKE_B;

static const double SCROLLBAR_ARROW_FILL_R = 1;
static const double SCROLLBAR_ARROW_FILL_G = 1;
static const double SCROLLBAR_ARROW_FILL_B = 1;

static const double SCROLLBAR_BG_R = static_cast<double>(0xF0) / 255.0;
static const double SCROLLBAR_BG_G = static_cast<double>(0xF0) / 255.0;
static const double SCROLLBAR_BG_B = static_cast<double>(0xF0) / 255.0;

static const double SCROLLBAR_BORDER_R = BASE_STROKE_R;
static const double SCROLLBAR_BORDER_G = BASE_STROKE_G;
static const double SCROLLBAR_BORDER_B = BASE_STROKE_B;

static const int SCROLLBAR_IDEAL_ARROW_SIZE = 11;

static const double SCROLLBAR_THUMB_BG_R = 1;
static const double SCROLLBAR_THUMB_BG_G = 1;
static const double SCROLLBAR_THUMB_BG_B = 1;

static const double TAB_PANE_BORDER_R = BASE_STROKE_R;
static const double TAB_PANE_BORDER_G = BASE_STROKE_G;
static const double TAB_PANE_BORDER_B = BASE_STROKE_B;

static const double TEXT_GRADIENT_R0 = static_cast<double>(0xC5) / 255.0;
static const double TEXT_GRADIENT_G0 = static_cast<double>(0xC5) / 255.0;
static const double TEXT_GRADIENT_B0 = static_cast<double>(0xC5) / 255.0;
static const double TEXT_GRADIENT_R1 = 1;
static const double TEXT_GRADIENT_G1 = 1;
static const double TEXT_GRADIENT_B1 = 1;
static const double TEXT_GRADIENT_R2 = 1;
static const double TEXT_GRADIENT_G2 = 1;
static const double TEXT_GRADIENT_B2 = 1;

static const double TEXT_GRADIENT_STOP_0 = 0;
static const double TEXT_GRADIENT_STOP_1 = .2;
static const double TEXT_GRADIENT_STOP_2 = 1;

static const double TREE_ITEM_BG_R = 1;
static const double TREE_ITEM_BG_G = 1;
static const double TREE_ITEM_BG_B = 1;

static const double TREE_ITEM_SELECTED_BG_R = SELECTED_TEXT_BG_R;
static const double TREE_ITEM_SELECTED_BG_G = SELECTED_TEXT_BG_G;
static const double TREE_ITEM_SELECTED_BG_B = SELECTED_TEXT_BG_B;

// Tooltip border.
static const double TOOLTIP_R = 1;
static const double TOOLTIP_G = 1;
static const double TOOLTIP_B = 1;

// NOTE: Cairo strokes with the pen between pixels. This results in a line width
// of 1 touching two pixels. To account for this you have to add .5 so that the
// pen ends up touching only one pixel. The pen is best though of as extending
// line_stroke_width / 2 in the opposite direction you are drawing and 0 pixels
// in the direction you are drawing. This means to draw a horizontal line with
// a width of 1 pixel you draw from (x, y + .5) to (x + 1, y + 5) and similary
// a vertical line of 1 pixel is drawn using (x + .5, y) to (x + .5, y + 1).
// See http://www.cairographics.org/FAQ/ for details.

// Strokes a rectangle a single pixel wide.
static void DrawSinglePixelWideRectangle(cairo_t* cr,
                                         int x,
                                         int y,
                                         int w,
                                         int h) {
  cairo_set_line_width(cr, 1);
  cairo_translate(cr, x, y);

  cairo_move_to(cr, 0, .5);
  cairo_line_to(cr, w, .5);
  cairo_stroke(cr);

  cairo_move_to(cr, 0, h - .5);
  cairo_line_to(cr, w, h - .5);
  cairo_stroke(cr);

  cairo_move_to(cr, .5, 0);
  cairo_line_to(cr, .5, h);
  cairo_stroke(cr);

  cairo_move_to(cr, w - .5, 0);
  cairo_line_to(cr, w - .5, h);
  cairo_stroke(cr);

  cairo_translate(cr, -x, -y);
}

// All arrows are drawn down. To get the right directionality the context is
// first rotated by this many degrees.
static double GetRotationAngle(GtkArrowType arrow_type) {
  switch (arrow_type) {
    case GTK_ARROW_UP:
      return M_PI;
    case GTK_ARROW_LEFT:
      return M_PI / 2;
    case GTK_ARROW_RIGHT:
      return M_PI * 3 / 2;
    default:
      return 0;
  }
  return 0;
}

// Draws a filled arrow.
static void DrawFilledArrow(cairo_t* cr,
                            GtkArrowType arrow_type,
                            int x,
                            int y,
                            int w,
                            int h,
                            int ideal_size) {
  cairo_set_line_width(cr, 1);

  int size = std::min(h - 1, std::min(ideal_size, w - 1));
  if (size % 2 == 0)
    size--;  // Force the size to be odd.

  int arrow_height = size / 2 + 1;
  int center_x = x + w / 2;
  int center_y = y + h / 2;

  cairo_translate(cr, center_x, center_y);
  cairo_rotate(cr, GetRotationAngle(arrow_type));

  int i;
  for (i = 0; i < arrow_height; ++i) {
    cairo_move_to(cr, i - size / 2, i - arrow_height / 2 + .5);
    cairo_line_to(cr, size / 2 - i + 1, i - arrow_height / 2 + .5);
    cairo_stroke(cr);
  }

  cairo_rotate(cr, -GetRotationAngle(arrow_type));
  cairo_translate(cr, -center_x, -center_y);
}

// Adds a rounded rect path to |cr| of the specified size. |stroke_width| gives
// the width of the stroking line and |arc_radius| the radius of the edges of
// the rectangle.
static void AddRoundedRectPath(cairo_t* cr, int w, int h, int stroke_width,
                               int arc_radius) {
  double offset = (stroke_width % 2 == 1) ? .5 : 0;
  int half_stroke_width = stroke_width / 2;

  cairo_translate(cr, half_stroke_width, half_stroke_width);

  w -= 2 * half_stroke_width;
  h -= 2 * half_stroke_width;

  cairo_move_to(cr, arc_radius, offset);
  cairo_line_to(cr, w - arc_radius, offset);
  cairo_arc(cr, w - arc_radius - offset, arc_radius + offset, arc_radius,
            -M_PI / 2, 0);

  cairo_line_to(cr, w - offset, h - arc_radius);
  cairo_arc(cr, w - arc_radius - offset, h - arc_radius - offset,
            arc_radius, 0, M_PI / 2);

  cairo_line_to(cr, arc_radius, h - offset);
  cairo_arc(cr, arc_radius + offset, h - arc_radius - offset, arc_radius,
            M_PI / 2, M_PI);

  cairo_line_to(cr, offset, arc_radius);
  cairo_arc(cr, arc_radius + offset, arc_radius + offset, arc_radius, M_PI,
            M_PI * 3 / 2);

  cairo_translate(cr, -half_stroke_width, -half_stroke_width);
}

// Sets the source color of |cr| to the appropriate indicator color.
static void SetIndicatorStrokeColor(cairo_t* cr, GtkWidget* widget,
                                    bool pressed) {
  if (!GTK_WIDGET_SENSITIVE(widget)) {
    cairo_set_source_rgb( cr, INDICATOR_STROKE_DISABLED_R,
                          INDICATOR_STROKE_DISABLED_G,
                          INDICATOR_STROKE_DISABLED_B);
  } else if (pressed) {
    cairo_set_source_rgb(cr, INDICATOR_STROKE_PRESSED_R,
                         INDICATOR_STROKE_DISABLED_G,
                         INDICATOR_STROKE_DISABLED_B);
  } else {
    cairo_set_source_rgb(cr, INDICATOR_STROKE_R,
                         INDICATOR_STROKE_G, INDICATOR_STROKE_B);
  }
}

// Sets the color used for many widgets.
static void SetStrokeColor(cairo_t* cr, bool enabled, bool focused) {
  if (!enabled) {
    cairo_set_source_rgb(cr, DISABLED_BASE_STROKE_R,
                         DISABLED_BASE_STROKE_G, DISABLED_BASE_STROKE_B);
  } if (focused) {
    cairo_set_source_rgb(cr, FOCUSED_STROKE_R, FOCUSED_STROKE_G,
                         FOCUSED_STROKE_B);
  } else {
    cairo_set_source_rgb(cr, BASE_STROKE_R, BASE_STROKE_G,
                         BASE_STROKE_B);
  }
}

static void DrawTextBorder(cairo_t* cr,
                           GtkWidget* widget,
                           int x,
                           int y,
                           int w,
                           int h) {
  if (!GTK_WIDGET_HAS_FOCUS(widget)) {
    x++;
    y++;
    w -= 2;
    h -= 2;
  }
  cairo_translate(cr, x, y);

  int stroke_width = GTK_WIDGET_HAS_FOCUS(widget)
      ? FOCUSED_STROKE_WIDTH : STROKE_WIDTH;
  cairo_set_line_width(cr, stroke_width);

  AddRoundedRectPath(cr, w, h, stroke_width, BORDER_CORNER_RADIUS);

  ScopedPattern pattern(cairo_pattern_create_linear(0, 0, 0, h));
  cairo_pattern_add_color_stop_rgb(pattern.get(), TEXT_GRADIENT_STOP_0,
                                   TEXT_GRADIENT_R0, TEXT_GRADIENT_G0,
                                   TEXT_GRADIENT_B0);
  cairo_pattern_add_color_stop_rgb(pattern.get(), TEXT_GRADIENT_STOP_1,
                                   TEXT_GRADIENT_R1, TEXT_GRADIENT_G1,
                                   TEXT_GRADIENT_B1);
  cairo_pattern_add_color_stop_rgb(pattern.get(), TEXT_GRADIENT_STOP_2,
                                   TEXT_GRADIENT_R2, TEXT_GRADIENT_G2,
                                   TEXT_GRADIENT_B2);

  cairo_set_source(cr, pattern.get());
  cairo_fill_preserve(cr);

  SetStrokeColor(cr, GTK_WIDGET_SENSITIVE(widget),
                 GTK_WIDGET_HAS_FOCUS(widget));
  cairo_stroke(cr);
}

// Adds the gradient used for buttons to cr.
static void AddRoundRectGradient(cairo_t* cr,
                                 ScopedPattern* pattern,
                                 int h,
                                 bool pressed) {
  pattern->reset(cairo_pattern_create_linear(0, 0, 0, h));
  // TODO: need disabled.
  if (pressed) {
    cairo_pattern_add_color_stop_rgb(pattern->get(), 0, PRESSED_GRADIENT_R0,
                                     PRESSED_GRADIENT_G0, PRESSED_GRADIENT_B0);
    cairo_pattern_add_color_stop_rgb(pattern->get(), 1, PRESSED_GRADIENT_R1,
                                     PRESSED_GRADIENT_G1, PRESSED_GRADIENT_B1);
  } else {
    cairo_pattern_add_color_stop_rgb(pattern->get(), GRADIENT_STOP_0,
                                     GRADIENT_R0, GRADIENT_G0, GRADIENT_B0);
    cairo_pattern_add_color_stop_rgb(pattern->get(), GRADIENT_STOP_1,
                                     GRADIENT_R1, GRADIENT_G1, GRADIENT_B1);
    cairo_pattern_add_color_stop_rgb(pattern->get(), GRADIENT_STOP_2,
                                     GRADIENT_R2, GRADIENT_G2, GRADIENT_B2);
  }
  cairo_set_source(cr, pattern->get());
}

// Draws a rounded rect and stroke.
static void DrawRoundRectBorderWithStroke(GdkWindow* window,
                                          GdkRectangle* area,
                                          int x,
                                          int y,
                                          int w,
                                          int h,
                                          bool enabled,
                                          bool pressed,
                                          bool focused,
                                          bool inset) {
  if (inset && !focused) {
    // Inset the non-focused border slightly so that the focus border visually
    // pops out.
    x++;
    y++;
    w -= 2;
    h -= 2;
  }

  ScopedSurface cr(window, area);

  cairo_translate(cr.get(), x, y);

  int stroke_width = focused ? FOCUSED_STROKE_WIDTH : STROKE_WIDTH;
  cairo_set_line_width(cr.get(), stroke_width);

  AddRoundedRectPath(cr.get(), w, h, stroke_width, BORDER_CORNER_RADIUS);

  ScopedPattern pattern;
  AddRoundRectGradient(cr.get(), &pattern, h, pressed);
  cairo_fill_preserve(cr.get());

  SetStrokeColor(cr.get(), enabled, focused);
  cairo_stroke(cr.get());
}

// Draws a check.
static void DrawCheckMark(GtkWidget* widget, GdkWindow* window,
                          GdkRectangle* area, int x, int y, int w, int h,
                          bool pressed) {
  ScopedSurface cr(window, area);

  cairo_translate(cr.get(), x + (w - 8) / 2, y + h / 2);
  SetIndicatorStrokeColor(cr.get(), widget, pressed);
  cairo_move_to(cr.get(), 0, 0);
  cairo_line_to(cr.get(), 3, 2);
  cairo_line_to(cr.get(), 8, -4);
  cairo_stroke(cr.get());
}

// Draws the indicator for a button button.
static void DrawRadioIndicator(GtkWidget* widget,
                               GdkWindow* window,
                               GdkRectangle* area,
                               int x,
                               int y,
                               int w,
                               int h,
                               bool selected,
                               bool pressed,
                               int ideal_selected_size) {
  ScopedSurface cr(window, area);
  // Inset the non-focused border.
  int offset = GTK_WIDGET_HAS_FOCUS(widget) ? 0 : 1;
  int indicator_size = std::min(w, h) - offset - offset;

  cairo_translate(cr.get(), x + (w - indicator_size) / 2,
                  y + (h - indicator_size) / 2);

  // Draw the outer circle first.
  ScopedPattern pattern;
  AddRoundRectGradient(cr.get(), &pattern, indicator_size, pressed);
  cairo_arc(cr.get(), indicator_size / 2, indicator_size / 2,
            indicator_size / 2, 0, M_PI * 2);
  cairo_fill_preserve(cr.get());

  cairo_set_line_width(cr.get(), GTK_WIDGET_HAS_FOCUS(widget) ?
                       FOCUSED_STROKE_WIDTH : STROKE_WIDTH);
  if (GTK_WIDGET_HAS_FOCUS(widget)) {
    cairo_set_source_rgb(cr.get(), FOCUSED_STROKE_R, FOCUSED_STROKE_G,
                         FOCUSED_STROKE_B);
  } else {
    cairo_set_source_rgb(cr.get(), BASE_STROKE_R, BASE_STROKE_G,
                         BASE_STROKE_B);
  }
  cairo_stroke(cr.get());

  if (!selected)
    return;

  // Draw selected indicator.
  int selected_indicator_size =
      std::min(indicator_size - 2, ideal_selected_size);
  pattern.reset(cairo_pattern_create_linear(0, 0, 0, h));
  if (!GTK_WIDGET_SENSITIVE(widget)) {
    cairo_pattern_add_color_stop_rgb(
        pattern.get(), 0, RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_R0,
        RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_G0,
        RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_B0);
    cairo_pattern_add_color_stop_rgb(
        pattern.get(), 1, RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_R1,
        RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_G1,
        RADIO_BUTTON_INDICATOR_GRADIENT_DISABLED_B1);
  } else if (pressed) {
    cairo_pattern_add_color_stop_rgb(
        pattern.get(), 0, RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_R0,
        RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_G0,
        RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_B0);
    cairo_pattern_add_color_stop_rgb(
        pattern.get(), 1, RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_R1,
        RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_G1,
        RADIO_BUTTON_INDICATOR_GRADIENT_PRESSED_B1);
  } else {
    cairo_pattern_add_color_stop_rgb(
        pattern.get(), 0, RADIO_BUTTON_INDICATOR_GRADIENT_R0,
        RADIO_BUTTON_INDICATOR_GRADIENT_G0,
        RADIO_BUTTON_INDICATOR_GRADIENT_B0);
    cairo_pattern_add_color_stop_rgb(
        pattern.get(), 1, RADIO_BUTTON_INDICATOR_GRADIENT_R1,
        RADIO_BUTTON_INDICATOR_GRADIENT_G1,
        RADIO_BUTTON_INDICATOR_GRADIENT_B1);
  }
  cairo_set_source(cr.get(), pattern.get());
  cairo_arc(cr.get(), indicator_size / 2, indicator_size / 2,
            selected_indicator_size / 2, 0, M_PI * 2);
  cairo_fill_preserve(cr.get());

  cairo_set_line_width(cr.get(), 1);
  SetIndicatorStrokeColor(cr.get(), widget, pressed);
  cairo_stroke(cr.get());
}

// All the theme engine functions decode the params and call the appropriate
// function below.

static void DrawButtonBorder(GtkStyle* style,
                             GdkWindow* window,
                             GtkStateType state_type,
                             GtkShadowType shadow_type,
                             GdkRectangle* area,
                             GtkWidget* widget,
                             gint x,
                             gint y,
                             gint w,
                             gint h) {
  DrawRoundRectBorderWithStroke(window, area, x, y, w, h,
                                GTK_WIDGET_SENSITIVE(widget),
                                GTK_BUTTON(widget)->depressed,
                                GTK_WIDGET_HAS_FOCUS(widget),
                                true);
}

static void DrawCheckboxCheck(GtkStyle* style,
                              GdkWindow* window,
                              GtkStateType state_type,
                              GtkShadowType shadow_type,
                              GdkRectangle* area,
                              GtkWidget* widget,
                              gint x,
                              gint y,
                              gint w,
                              gint h) {
  DrawRoundRectBorderWithStroke(window, area, x, y, w, h,
                                GTK_WIDGET_SENSITIVE(widget),
                                (state_type == GTK_STATE_ACTIVE),
                                GTK_WIDGET_HAS_FOCUS(widget),
                                true);
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    DrawCheckMark(widget, window, area, x, y, w, h,
                  (state_type == GTK_STATE_ACTIVE));
  }
}

static void DrawComboboxArrow(GtkStyle* style,
                              GdkWindow* window,
                              GtkStateType state_type,
                              GtkShadowType shadow_type,
                              GdkRectangle* area,
                              GtkWidget* widget,
                              GtkArrowType arrow_type,
                              gboolean fill,
                              gint x,
                              gint y,
                              gint w,
                              gint h) {
  ScopedSurface cr(window, area);

  DrawFilledArrow(cr.get(), arrow_type, x, y, w, h, COMBOBOX_IDEAL_ARROW_SIZE);
}

static void DrawHorizontalSliderThumb(GtkStyle* style,
                                      GdkWindow* window,
                                      GtkStateType state_type,
                                      GtkShadowType shadow_type,
                                      GdkRectangle* area,
                                      GtkWidget* widget,
                                      gint x,
                                      gint y,
                                      gint w,
                                      gint h,
                                      GtkOrientation orientation) {
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    y += 2;
    h -= 4;
  } else {
    x += 2;
    w -= 4;
  }
  DrawRoundRectBorderWithStroke(window, area, x, y, w, h,
                                GTK_WIDGET_SENSITIVE(widget), false, false,
                                false);
}

static void DrawHorizontalSliderTrack(GtkStyle* style,
                                      GdkWindow* window,
                                      GtkStateType state_type,
                                      GtkShadowType shadow_type,
                                      GdkRectangle* area,
                                      GtkWidget* widget,
                                      gint x,
                                      gint y,
                                      gint w,
                                      gint h) {
  ScopedSurface cr(window, area);
  cairo_translate(cr.get(), x, y + (h - H_SLIDER_TRACK_HEIGHT) / 2);
  AddRoundedRectPath(cr.get(), w, H_SLIDER_TRACK_HEIGHT, 1,
                     BORDER_CORNER_RADIUS);
  cairo_set_source_rgb(cr.get(), H_SLIDER_TRACK_FILL_R, H_SLIDER_TRACK_FILL_G,
                       H_SLIDER_TRACK_FILL_G);
  cairo_fill_preserve(cr.get());
  cairo_set_source_rgb(cr.get(), H_SLIDER_TRACK_R, H_SLIDER_TRACK_G,
                       H_SLIDER_TRACK_B);
  cairo_set_line_width(cr.get(), 1);
  cairo_stroke(cr.get());
}

static void DrawMenuArrow(GtkStyle* style,
                          GdkWindow* window,
                          GtkStateType state_type,
                          GtkShadowType shadow_type,
                          GdkRectangle* area,
                          GtkWidget* widget,
                          GtkArrowType arrow_type,
                          gboolean fill,
                          gint x,
                          gint y,
                          gint w,
                          gint h) {
  ScopedSurface cr(window, area);
  // Expand the size so we get a descent arrow. It's ok to do expand the size
  // as this size goes into the borders, which we don't render into.
  w += 2;
  h += 2;
  x--;
  y--;
  DrawFilledArrow(cr.get(), arrow_type, x, y, w, h, MENU_IDEAL_ARROW_SIZE);
}

static void DrawMenuBorder(GtkStyle* style,
                           GdkWindow* window,
                           GtkStateType state_type,
                           GtkShadowType shadow_type,
                           GdkRectangle* area,
                           GtkWidget* widget,
                           gint x,
                           gint y,
                           gint w,
                           gint h) {
  ScopedSurface cr(window, area);

  cairo_rectangle(cr.get(), x, y, w, h);

  cairo_set_source_rgb(cr.get(), MENU_BG_R, MENU_BG_G, MENU_BG_B);
  cairo_fill(cr.get());

  cairo_set_source_rgb(cr.get(), MENU_BORDER_R, MENU_BORDER_G, MENU_BORDER_B);
  DrawSinglePixelWideRectangle(cr.get(), x, y, w, h);
}

static void DrawMenuHorizontalSeparator(GtkStyle* style,
                                        GdkWindow* window,
                                        GtkStateType state_type,
                                        GdkRectangle* area,
                                        GtkWidget* widget,
                                        gint x1,
                                        gint x2,
                                        gint y) {
  // The separator is inset by padding and xthickness. Offset by that so the
  // separator extends through the whole menu item.
  int horizontal_padding;
  gtk_widget_style_get(widget, "horizontal-padding", &horizontal_padding,
                       NULL);
  int x_padding = horizontal_padding + widget->style->xthickness;
  x1 -= x_padding;
  x2 += x_padding + x_padding;

  ScopedSurface cr(window, area);
  ScopedPattern pattern(cairo_pattern_create_linear(0, 0, x2 - x1, 0));
  cairo_pattern_add_color_stop_rgb(pattern.get(), 0, H_SEPARATOR_R0,
                                   H_SEPARATOR_G0, H_SEPARATOR_B0);
  cairo_pattern_add_color_stop_rgb(pattern.get(), 1, H_SEPARATOR_R1,
                                   H_SEPARATOR_G1, H_SEPARATOR_B1);
  cairo_set_source(cr.get(), pattern.get());
  cairo_set_line_width(cr.get(), 1);
  cairo_move_to(cr.get(), x1, y + .5);
  cairo_line_to(cr.get(), x2, y + .5);
  cairo_stroke(cr.get());
}

static void DrawMenuItemBorder(GtkStyle* style,
                               GdkWindow* window,
                               GtkStateType state_type,
                               GtkShadowType shadow_type,
                               GdkRectangle* area,
                               GtkWidget* widget,
                               gint x,
                               gint y,
                               gint w,
                               gint h) {
  if (state_type == GTK_STATE_PRELIGHT) {
    ScopedSurface cr(window, area);

    cairo_set_source_rgb(cr.get(), MENU_BG_HIGHLIGHT_R, MENU_BG_HIGHLIGHT_G,
                         MENU_BG_HIGHLIGHT_B);
    cairo_rectangle(cr.get(), x, y, w, h);
    cairo_fill(cr.get());
  }
}

static void DrawMenuItemCheck(GtkStyle* style,
                              GdkWindow* window,
                              GtkStateType state_type,
                              GtkShadowType shadow_type,
                              GdkRectangle* area,
                              GtkWidget* widget,
                              gint x,
                              gint y,
                              gint w,
                              gint h) {
  if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget)))
    DrawCheckMark(widget, window, area, x, y, w, h, false);
}

static void DrawMenuItemRadio(GtkStyle* style,
                              GdkWindow* window,
                              GtkStateType state_type,
                              GtkShadowType shadow_type,
                              GdkRectangle* area,
                              GtkWidget* widget,
                              gint x,
                              gint y,
                              gint w,
                              gint h) {
  DrawRadioIndicator(widget, window, area, x, y, w, h,
                     gtk_check_menu_item_get_active(
                         GTK_CHECK_MENU_ITEM(widget)),
                     false,
                     MENU_RADIO_BUTTON_INDICATOR_IDEAL_SIZE);
}

static void DrawRadioButtonIndicator(GtkStyle* style,
                                     GdkWindow* window,
                                     GtkStateType state_type,
                                     GtkShadowType shadow_type,
                                     GdkRectangle* area,
                                     GtkWidget* widget,
                                     gint x,
                                     gint y,
                                     gint w,
                                     gint h) {
  DrawRadioIndicator(widget, window, area, x, y, w, h,
                     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)),
                     state_type == GTK_STATE_ACTIVE,
                     RADIO_BUTTON_INDICATOR_IDEAL_SIZE);
}

static void DrawScrollbarArrow(GtkStyle* style,
                               GdkWindow* window,
                               GtkStateType state_type,
                               GtkShadowType shadow_type,
                               GdkRectangle* area,
                               GtkWidget* widget,
                               GtkArrowType arrow_type,
                               gboolean fill,
                               gint x,
                               gint y,
                               gint w,
                               gint h) {
  // We want a slightly bigger arrow so we expand to the actual size that
  // range gives. GtkRange gives the arrow half the available width, so that
  // it's ok to expand the area here.
  x = x + w / 2 - w;
  y = y + h / 2 - h;
  w += w;
  h += h;
  ScopedSurface cr(window, area);
  cairo_set_line_width(cr.get(), 1);
  cairo_set_source_rgb(cr.get(), SCROLLBAR_ARROW_FILL_R,
                       SCROLLBAR_ARROW_FILL_G, SCROLLBAR_ARROW_FILL_B);

  cairo_translate(cr.get(), x + w / 2, y + h / 2);

  // Nudge things slightly so they look pretty.
  if (arrow_type == GTK_ARROW_UP)
    cairo_translate(cr.get(), 1, 1);

  cairo_rotate(cr.get(), GetRotationAngle(arrow_type));

  int arrow_w = std::min(h, std::min(SCROLLBAR_IDEAL_ARROW_SIZE, w));
  if (arrow_w % 2 == 0)
    arrow_w--;  // Force size to be odd.
  int arrow_h = arrow_w - 1;

  // Create the path first. We don't stroke this path as it doesn't line
  // up as nicely as the path below.
  cairo_translate(cr.get(), -arrow_w / 2, -arrow_h / 2);
  cairo_move_to(cr.get(), 0, .5);
  cairo_line_to(cr.get(), arrow_w, .5);
  cairo_line_to(cr.get(), arrow_w / 2 + .5, arrow_h - .5);
  cairo_line_to(cr.get(), arrow_w / 2 + .5, arrow_h - .5);
  cairo_close_path(cr.get());
  cairo_fill(cr.get());

  // Then the stroke path.
  cairo_set_source_rgb(cr.get(), SCROLLBAR_ARROW_BORDER_R,
                       SCROLLBAR_ARROW_BORDER_G, SCROLLBAR_ARROW_BORDER_B);
  cairo_move_to(cr.get(), 0, .5);
  cairo_line_to(cr.get(), arrow_w, .5);
  cairo_stroke(cr.get());

  cairo_move_to(cr.get(), arrow_w - .5, .5);
  cairo_line_to(cr.get(), arrow_w / 2 + .5, arrow_h - .5);
  cairo_stroke(cr.get());

  cairo_move_to(cr.get(), .5, .5);
  cairo_line_to(cr.get(), arrow_w / 2 + .5, arrow_h - .5);
  cairo_stroke(cr.get());
}

static void DrawScrollbarBorder(GtkStyle* style,
                                GdkWindow* window,
                                GtkStateType state_type,
                                GtkShadowType shadow_type,
                                GdkRectangle* area,
                                GtkWidget* widget,
                                gint x,
                                gint y,
                                gint w,
                                gint h) {
  ScopedSurface cr(window, area);
  cairo_rectangle(cr.get(), x, y, w, h);
  cairo_set_source_rgb(cr.get(), SCROLLBAR_BORDER_R, SCROLLBAR_BORDER_G,
                       SCROLLBAR_BORDER_B);
  cairo_stroke(cr.get());
}

static void DrawScrollbarThumb(GtkStyle* style,
                               GdkWindow* window,
                               GtkStateType state_type,
                               GtkShadowType shadow_type,
                               GdkRectangle* area,
                               GtkWidget* widget,
                               gint x,
                               gint y,
                               gint w,
                               gint h,
                               GtkOrientation orientation) {
  // Draw a slightly smaller thumb.
  if (orientation == GTK_ORIENTATION_HORIZONTAL) {
    y += 1;
    h -= 2;
  } else {
    x += 1;
    w -= 2;
  }
  ScopedSurface cr(window, area);

  cairo_translate(cr.get(), x, y);

  cairo_set_line_width(cr.get(), 1);

  AddRoundedRectPath(cr.get(), w, h, 1, BORDER_CORNER_RADIUS);

  cairo_set_source_rgb(cr.get(), SCROLLBAR_THUMB_BG_R, SCROLLBAR_THUMB_BG_G,
                       SCROLLBAR_THUMB_BG_B);
  cairo_fill_preserve(cr.get());

  SetStrokeColor(cr.get(), true, false);
  cairo_stroke(cr.get());
}

static void DrawScrollbarTrack(GtkStyle* style,
                               GdkWindow* window,
                               GtkStateType state_type,
                               GtkShadowType shadow_type,
                               GdkRectangle* area,
                               GtkWidget* widget,
                               gint x,
                               gint y,
                               gint w,
                               gint h) {
  ScopedSurface cr(window, area);
  cairo_set_source_rgb(cr.get(), SCROLLBAR_BG_R, SCROLLBAR_BG_G,
                       SCROLLBAR_BG_B);
  cairo_rectangle(cr.get(), x, y, w, h);
  cairo_fill(cr.get());
}

static void DrawTabBorder(GtkStyle* style,
                          GdkWindow* window,
                          GtkStateType state_type,
                          GtkShadowType shadow_type,
                          GdkRectangle* area,
                          GtkWidget* widget,
                          gint x,
                          gint y,
                          gint w,
                          gint h,
                          GtkPositionType gap_side) {
  ScopedSurface cr(window, area);

  if (x + w != widget->allocation.x + widget->allocation.width) {
    // So that tabs don't visually overlap.
    w--;
  }

  if (state_type != GTK_STATE_NORMAL) {
    // Because we have an thickness specified in the rc file non-selected tabs
    // shift down. This forces all tabs to be rendered the same.
    y--;
    h++;
  }

  cairo_translate(cr.get(), x, y);
  cairo_set_line_width(cr.get(), 1);
  cairo_set_source_rgb(cr.get(), TAB_PANE_BORDER_R, TAB_PANE_BORDER_G,
                       TAB_PANE_BORDER_B);

  cairo_move_to(cr.get(), .5, h);
  cairo_line_to(cr.get(), .5, BORDER_CORNER_RADIUS);
  cairo_arc(cr.get(), BORDER_CORNER_RADIUS + .5, BORDER_CORNER_RADIUS + .5,
            BORDER_CORNER_RADIUS, M_PI, M_PI * 3 / 2);

  cairo_line_to(cr.get(), BORDER_CORNER_RADIUS, .5);
  cairo_line_to(cr.get(), w - BORDER_CORNER_RADIUS, .5);
  cairo_arc(cr.get(), w - BORDER_CORNER_RADIUS - .5,
            BORDER_CORNER_RADIUS + .5, BORDER_CORNER_RADIUS, -M_PI / 2, 0);

  cairo_line_to(cr.get(), w - .5, BORDER_CORNER_RADIUS);
  cairo_line_to(cr.get(), w - .5, h);

  cairo_stroke(cr.get());
}

static void DrawTabPaneBorder(GtkStyle* style,
                              GdkWindow* window,
                              GtkStateType state_type,
                              GtkShadowType shadow_type,
                              GdkRectangle* area,
                              GtkWidget* widget,
                              gint x,
                              gint y,
                              gint w,
                              gint h,
                              GtkPositionType gap_side,
                              gint gap_x,
                              gint gap_w) {
  ScopedSurface cr(window, area);

  cairo_translate(cr.get(), static_cast<double>(x),
                  static_cast<double>(y));
  cairo_set_source_rgb(cr.get(), TAB_PANE_BORDER_R, TAB_PANE_BORDER_G,
                       TAB_PANE_BORDER_B);
  cairo_set_line_width(cr.get(), 1);

  cairo_move_to(cr.get(), .5, 0);
  cairo_line_to(cr.get(), .5, h);
  cairo_stroke(cr.get());

  cairo_move_to(cr.get(), 0, h - .5);
  cairo_line_to(cr.get(), w, h - .5);
  cairo_stroke(cr.get());

  cairo_move_to(cr.get(), w - .5, 0);
  cairo_line_to(cr.get(), w - .5, h);
  cairo_stroke(cr.get());

  if (gap_x > 0) {
    cairo_move_to(cr.get(), 0, .5);
    cairo_line_to(cr.get(), gap_x + 1, .5);
    cairo_stroke(cr.get());
  }

  if (gap_x + gap_w < w) {
    cairo_move_to(cr.get(), gap_x + gap_w - 2, .5);
    cairo_line_to(cr.get(), w, .5);
    cairo_stroke(cr.get());
  }
}

static void DrawTextFieldBackground(GtkStyle* style,
                                    GdkWindow* window,
                                    GtkStateType state_type,
                                    GtkShadowType shadow_type,
                                    GdkRectangle* area,
                                    GtkWidget* widget,
                                    gint x,
                                    gint y,
                                    gint w,
                                    gint h) {
  if (!gtk_entry_get_has_frame(GTK_ENTRY(widget)))
    return;

  ScopedSurface cr(window, area);
  DrawTextBorder(cr.get(), widget, -(widget->allocation.width - w) / 2,
                 -(widget->allocation.height - h) / 2,
                 widget->allocation.width,
                 widget->allocation.height);
}

static void DrawTextFieldBorder(GtkStyle* style,
                                GdkWindow* window,
                                GtkStateType state_type,
                                GtkShadowType shadow_type,
                                GdkRectangle* area,
                                GtkWidget* widget,
                                gint x,
                                gint y,
                                gint w,
                                gint h) {
  if (!gtk_entry_get_has_frame(GTK_ENTRY(widget)))
    return;

  ScopedSurface cr(window, area);
  DrawTextBorder(cr.get(), widget, x, y, w, h);
}

static void DrawTooltipBorder(GtkStyle* style,
                              GdkWindow* window,
                              GtkStateType state_type,
                              GtkShadowType shadow_type,
                              GdkRectangle* area,
                              GtkWidget* widget,
                              gint x,
                              gint y,
                              gint w,
                              gint h) {
  ScopedSurface cr(window, area);
  cairo_set_source_rgb(cr.get(), TOOLTIP_R, TOOLTIP_G, TOOLTIP_B);
  cairo_rectangle(cr.get(), x, y, w, h);
  cairo_stroke(cr.get());
}

static void DrawTreeItemBackground(GtkStyle* style,
                                   GdkWindow* window,
                                   GtkStateType state_type,
                                   GtkShadowType shadow_type,
                                   GdkRectangle* area,
                                   GtkWidget* widget,
                                   gint x,
                                   gint y,
                                   gint w,
                                   gint h) {
  ScopedSurface cr(window, area);
  if (state_type == GTK_STATE_SELECTED) {
    cairo_set_source_rgb(cr.get(), TREE_ITEM_SELECTED_BG_R,
                         TREE_ITEM_SELECTED_BG_G, TREE_ITEM_SELECTED_BG_B);
  } else {
    cairo_set_source_rgb(cr.get(), TREE_ITEM_BG_R, TREE_ITEM_BG_G,
                         TREE_ITEM_BG_B);
  }
  cairo_rectangle(cr.get(), x, y, w, h);
  cairo_fill(cr.get());
}

static void DrawViewportBorder(GtkStyle* style,
                               GdkWindow* window,
                               GtkStateType state_type,
                               GtkShadowType shadow_type,
                               GdkRectangle* area,
                               GtkWidget* widget,
                               gint x,
                               gint y,
                               gint w,
                               gint h) {
  // NOTE: we ignore w/h as they are always -1,-1 here.
  ScopedSurface cr(window, area);
  cairo_set_source_rgb(cr.get(), SCROLLBAR_BORDER_R, SCROLLBAR_BORDER_G,
                       SCROLLBAR_BORDER_B);
  DrawSinglePixelWideRectangle(cr.get(), 0, 0, widget->allocation.width,
                               widget->allocation.height);
}

// Theme engine functions. These all call into the more specific functions
// above.

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
                    gint h) {
  if (GTK_IS_ARROW(widget) && detail && !strcmp("arrow", detail)) {
    DrawComboboxArrow(style, window, state_type, shadow_type, area, widget,
                      arrow_type, fill, x, y, w, h);
  } else if (GTK_IS_RANGE(widget)) {
    DrawScrollbarArrow(style, window, state_type, shadow_type, area, widget,
                       arrow_type, fill, x, y, w, h);
  } else if (GTK_IS_MENU_ITEM(widget) && detail &&
             !strcmp(detail, "menuitem")) {
    DrawMenuArrow(style, window, state_type, shadow_type, area, widget,
                  arrow_type, fill, x, y, w, h);
  }
}

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
                  gint h) {
  if (GTK_IS_BUTTON(widget) && detail && !strcmp("button", detail)) {
    DrawButtonBorder(style, window, state_type, shadow_type, area, widget, x, y,
                     w, h);
  } else if (GTK_IS_HSCALE(widget) && detail && !strcmp(detail, "trough")) {
    DrawHorizontalSliderTrack(style, window, state_type, shadow_type, area,
                              widget, x, y, w, h);
  } else if (GTK_IS_RANGE(widget) && detail && !strcmp(detail, "trough")) {
    DrawScrollbarTrack(style, window, state_type, shadow_type, area, widget, x,
                       y, w, h);
  } else if (GTK_IS_MENU(widget) && detail && !strcmp(detail, "menu")) {
    DrawMenuBorder(style, window, state_type, shadow_type, area, widget, x, y,
                   w, h);
  } else if (GTK_IS_MENU_ITEM(widget) && detail &&
             !strcmp(detail, "menuitem")) {
    DrawMenuItemBorder(style, window, state_type, shadow_type, area, widget, x,
                       y, w, h);
  }
}

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
                     gint gap_w) {
  if (GTK_IS_NOTEBOOK(widget) && detail && !strcmp(detail, "notebook")) {
    DrawTabPaneBorder(style, window, state_type, shadow_type, area, widget,
                      x, y, w, h, gap_side, gap_x, gap_w);
  }
}

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
                    gint h) {
  if (GTK_IS_CHECK_BUTTON(widget) && detail && !strcmp("checkbutton", detail)) {
    DrawCheckboxCheck(style, window, state_type, shadow_type, area, widget, x,
                      y, w, h);
  } else if (GTK_IS_CHECK_MENU_ITEM(widget) && detail &&
             !strcmp("check", detail)) {
    DrawMenuItemCheck(style, window, state_type, shadow_type, area, widget, x,
                      y, w, h);
  }
}

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
                        GtkPositionType gap_side) {
  if (GTK_IS_NOTEBOOK(widget) && detail && !strcmp(detail, "tab")) {
    DrawTabBorder(style, window, state_type, shadow_type, area, widget, x, y,
                  w, h, gap_side);
  }
}

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
                      gint h) {
  if (GTK_IS_ENTRY(widget) && detail && !strcmp(detail, "entry_bg")) {
    DrawTextFieldBackground(style, window, state_type, shadow_type, area,
                            widget, x, y, w, h);
  } else if (GTK_IS_WINDOW(widget) && detail && !strcmp(detail, "tooltip")) {
    // NOTE: the if checks GTK_IS_WINDOW as that is what GtkTooltip supplies to
    // this function.

    DrawTooltipBorder(style, window, state_type, shadow_type, area, widget,
                      x, y, w, h);
  } else if (GTK_IS_TREE_VIEW(widget)) {
    DrawTreeItemBackground(style, window, state_type, shadow_type, area, widget,
                           x, y, w, h);
  }
}

void ThemeDrawFocus(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint x,
                    gint y,
                    gint w,
                    gint h) {
  // Focus is currently rendered in the border/background, so this does nothing.
}

void ThemeDrawHline(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint x1,
                    gint x2,
                    gint y) {
  if (GTK_IS_MENU_ITEM(widget) && detail && !strcmp(detail, "menuitem")) {
    DrawMenuHorizontalSeparator(style, window, state_type, area, widget, x1,
                                x2, y);
  }
}

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
                     gint h) {
  if (GTK_IS_RADIO_BUTTON(widget) && detail && !strcmp("radiobutton", detail)) {
    DrawRadioButtonIndicator(style, window, state_type, shadow_type, area,
                             widget, x, y, w, h);
  } else if (GTK_IS_CHECK_MENU_ITEM(widget) && detail &&
             !strcmp(detail, "option")) {
    DrawMenuItemRadio(style, window, state_type, shadow_type, area, widget,
                      x, y, w, h);
  }
}

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
                     gint h) {
  if (GTK_IS_ENTRY(widget) && detail && !strcmp(detail, "entry")) {
    DrawTextFieldBorder(style, window, state_type, shadow_type, area, widget, x,
                        y, w, h);
  } else if (GTK_IS_SCROLLED_WINDOW(widget) && detail &&
             !strcmp(detail, "scrolled_window")) {
    DrawScrollbarBorder(style, window, state_type, shadow_type, area, widget,
                        x, y, w, h);
  } else if (GTK_IS_VIEWPORT(widget) && detail &&
             !strcmp(detail, "viewport")) {
    DrawViewportBorder(style, window, state_type, shadow_type, area, widget, x,
                       y, w, h);
  }
}

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
                     GtkOrientation orientation) {
  if (GTK_IS_RANGE(widget) && detail && !strcmp(detail, "slider")) {
    DrawScrollbarThumb(style, window, state_type, shadow_type, area, widget, x,
                       y, w, h, orientation);
  } else if (GTK_IS_HSCALE(widget) && detail && !strcmp(detail, "hscale")) {
    DrawHorizontalSliderThumb(style, window, state_type, shadow_type, area,
                              widget, x, y, w, h, orientation);
  }
}

void ThemeDrawVline(GtkStyle* style,
                    GdkWindow* window,
                    GtkStateType state_type,
                    GdkRectangle* area,
                    GtkWidget* widget,
                    const gchar* detail,
                    gint y1,
                    gint y2,
                    gint x) {
  // We currently don't have any vertical separators. If we do need them, be
  // sure and special case so that we don't draw the separator for comboboxs:
}
