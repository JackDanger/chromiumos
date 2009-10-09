/*
 * Clutter.
 *
 * An OpenGL based 'interactive canvas' Library.
 *
 * Authored By Johan Bilien <johan.bilen@nokia.com>
 *             Kenneth Waters <kwaters@chromium.org>
 *
 * Copyright (C) 2007 OpenedHand
 * Copyright (C) 2009 The Chromium Authors
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __CLUTTER_EGLX_EGL_IMAGE_H__
#define __CLUTTER_EGLX_EGL_IMAGE_H__

#include <glib.h>
#include <glib-object.h>
#include <clutter/x11/clutter-x11-texture-pixmap.h>

G_BEGIN_DECLS

#define CLUTTER_EGLX_TYPE_EGL_IMAGE            (clutter_eglx_egl_image_get_type ())
#define CLUTTER_EGLX_EGL_IMAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), CLUTTER_EGLX_TYPE_EGL_IMAGE, ClutterEGLXEGLImage))
#define CLUTTER_EGLX_EGL_IMAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), CLUTTER_EGLX_TYPE_EGL_IMAGE, ClutterEGLXEGLImageClass))
#define CLUTTER_EGLX_IS_EGL_IMAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), CLUTTER_EGLX_TYPE_EGL_IMAGE))
#define CLUTTER_EGLX_IS_EGL_IMAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLUTTER_EGLX_TYPE_EGL_IMAGE))
#define CLUTTER_EGLX_EGL_IMAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), CLUTTER_EGLX_TYPE_EGL_IMAGE, ClutterEGLXEGLImageClass))

typedef struct _ClutterEGLXEGLImage        ClutterEGLXEGLImage;
typedef struct _ClutterEGLXEGLImageClass   ClutterEGLXEGLImageClass;
typedef struct _ClutterEGLXEGLImagePrivate ClutterEGLXEGLImagePrivate;

struct _ClutterEGLXEGLImageClass
{
  ClutterX11TexturePixmapClass   parent_class;
};

struct _ClutterEGLXEGLImage
{
  ClutterX11TexturePixmap     parent;

  ClutterEGLXEGLImagePrivate *priv;
};

GType         clutter_eglx_egl_image_get_type        (void);

gboolean      clutter_eglx_egl_image_using_extension (ClutterEGLXEGLImage *image);

ClutterActor *clutter_eglx_egl_image_new             (void);

ClutterActor *clutter_eglx_egl_image_new_with_pixmap (Pixmap pixmap);

ClutterActor *clutter_eglx_egl_image_new_with_window (Window window);

G_END_DECLS

#endif /* __CLUTTER_EGLX_EGL_IMAGE_H__ */
