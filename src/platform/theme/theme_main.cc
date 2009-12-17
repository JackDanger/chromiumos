// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gmodule.h>
#include <gtk/gtk.h>

#include "theme/theme_draw.h"

typedef struct _ThemeRcStyle ThemeRcStyle;
typedef struct _ThemeRcStyleClass ThemeRcStyleClass;

typedef struct _ThemeStyle ThemeStyle;
typedef struct _ThemeStyleClass ThemeStyleClass;

static GType theme_type_rc_style;
static GType theme_type_style;

#define THEME_TYPE_RC_STYLE theme_type_rc_style
#define THEME_RC_STYLE(object) \
    (G_TYPE_CHECK_INSTANCE_CAST ((object), THEME_TYPE_RC_STYLE, ThemeRcStyle))
#define THEME_RC_STYLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), THEME_TYPE_RC_STYLE, ThemeRcStyleClass))
#define THEME_IS_RC_STYLE(object) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((object), THEME_TYPE_RC_STYLE))
#define THEME_IS_RC_STYLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), THEME_TYPE_RC_STYLE))
#define THEME_RC_STYLE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), THEME_TYPE_RC_STYLE, ThemeRcStyleClass))

#define THEME_TYPE_STYLE theme_type_style
#define THEME_STYLE(object) \
    (G_TYPE_CHECK_INSTANCE_CAST((object), THEME_TYPE_STYLE, ThemeStyle))
#define THEME_STYLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), THEME_TYPE_STYLE, ThemeStyleClass))
#define THEME_IS_STYLE(object) \
    (G_TYPE_CHECK_INSTANCE_TYPE((object), THEME_TYPE_STYLE))
#define THEME_IS_STYLE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), THEME_TYPE_STYLE))
#define THEME_STYLE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), THEME_TYPE_STYLE, ThemeStyleClass))

struct _ThemeRcStyle {
  GtkRcStyle parent_instance;
};

struct _ThemeRcStyleClass {
  GtkRcStyleClass parent_class;
};

struct _ThemeStyle {
  GtkStyle parent_instance;
};

struct _ThemeStyleClass {
  GtkStyleClass parent_class;
};

// Rc style --------------------------------------------------------------------

static GtkStyle* theme_rc_style_create_style(GtkRcStyle* rc_style);

static void theme_rc_style_class_init(ThemeRcStyleClass* klass) {
  GtkRcStyleClass* rc_style_class = GTK_RC_STYLE_CLASS(klass);

  rc_style_class->create_style = theme_rc_style_create_style;
}

static void theme_rc_style_init(ThemeRcStyle *style) {
}

static void theme_rc_style_register_type(GTypeModule* module) {
  static const GTypeInfo object_info = {
    sizeof(ThemeRcStyleClass),
    NULL,
    NULL,
    (GClassInitFunc)theme_rc_style_class_init,
    NULL,
    NULL,
    sizeof(ThemeRcStyle),
    0,
    (GInstanceInitFunc)theme_rc_style_init,
  };

  theme_type_rc_style = g_type_module_register_type(
      module,
      GTK_TYPE_RC_STYLE,
      "ThemeRcStyle",
      &object_info,
      static_cast<GTypeFlags>(0));
}

// Style -----------------------------------------------------------------------

static void theme_style_init(ThemeStyle *style) {
}

static void theme_style_class_init(ThemeStyleClass* es_class) {
  GtkStyleClass* style_class = GTK_STYLE_CLASS(es_class);

  style_class->draw_arrow = ThemeDrawArrow;
  style_class->draw_box = ThemeDrawBox;
  style_class->draw_box_gap = ThemeDrawBoxGap;
  style_class->draw_check = ThemeDrawCheck;
  style_class->draw_extension = ThemeDrawExtension;
  style_class->draw_flat_box = ThemeDrawFlatBox;
  style_class->draw_focus = ThemeDrawFocus;
  style_class->draw_hline = ThemeDrawHline;
  style_class->draw_option = ThemeDrawOption;
  style_class->draw_shadow = ThemeDrawShadow;
  style_class->draw_slider = ThemeDrawSlider;
  style_class->draw_vline = ThemeDrawVline;
}

static void theme_style_register_type(GTypeModule* module) {
  static const GTypeInfo object_info = {
    sizeof(ThemeStyleClass),
    NULL,
    NULL,
    (GClassInitFunc)theme_style_class_init,
    NULL,
    NULL,
    sizeof(ThemeStyle),
    0,
    (GInstanceInitFunc)theme_style_init,
  };
  theme_type_style = g_type_module_register_type(module,
                                                 GTK_TYPE_STYLE,
                                                 "ThemeStyle",
                                                 &object_info,
                                                 static_cast<GTypeFlags>(0));
}

static GtkStyle* theme_rc_style_create_style(GtkRcStyle* rc_style) {
  return reinterpret_cast<GtkStyle*>(g_object_new(THEME_TYPE_STYLE, NULL));
}

// Theme init ------------------------------------------------------------------

G_BEGIN_DECLS

G_MODULE_EXPORT void theme_init(GTypeModule* module) {
  theme_rc_style_register_type(module);
  theme_style_register_type(module);
}

G_MODULE_EXPORT void theme_exit() {
}

G_MODULE_EXPORT GtkRcStyle* theme_create_rc_style() {
  return reinterpret_cast<GtkRcStyle*>(
      g_object_new(THEME_TYPE_RC_STYLE, NULL));
}

G_MODULE_EXPORT const gchar* g_module_check_init(GModule* module) {
  return gtk_check_version(GTK_MAJOR_VERSION,
                           GTK_MINOR_VERSION,
                           GTK_MICRO_VERSION - GTK_INTERFACE_AGE);
}

G_END_DECLS
