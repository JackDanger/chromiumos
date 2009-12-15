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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "clutter-egl-headers.h"
#include <EGL/eglext.h>
#include <GLES2/gl2ext.h>

/* Work around broken eglext.h headers */
#ifndef EGL_NO_IMAGE_KHR
#define EGL_NO_IMAGE_KHR ((EGLImageKHR)0)
#endif
#ifndef EGL_IMAGE_PRESERVED_KHR
#define EGL_IMAGE_PRESERVED_KHR 0x30D2
#endif

#include "../x11/clutter-x11-texture-pixmap.h"
#include "clutter-eglx-egl-image.h"
#include "clutter-backend-egl.h"

#include "../clutter-debug.h"

G_DEFINE_TYPE (ClutterEGLXEGLImage,
               clutter_eglx_egl_image,
               CLUTTER_X11_TYPE_TEXTURE_PIXMAP);

struct _ClutterEGLXEGLImagePrivate
{
  EGLImageKHR egl_image;
  CoglHandle cogl_tex;

  gboolean use_fallback;
};

static PFNEGLCREATEIMAGEKHRPROC _egl_create_image_khr = NULL;
static PFNEGLDESTROYIMAGEKHRPROC _egl_destroy_image_khr = NULL;
static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC
  _gl_egl_image_target_texture_2d_oes = NULL;
static gboolean _ext_check_done = FALSE;

static void
destroy_image_and_texture (ClutterEGLXEGLImage *self);

static gboolean
create_image_and_texture (ClutterEGLXEGLImage *self);

static void
clutter_eglx_egl_image_dispose (GObject *object)
{
  ClutterEGLXEGLImage *image_texture =
    CLUTTER_EGLX_EGL_IMAGE (object);

  /* unrealize should have cleaned up our resources */
  g_assert (image_texture->priv->egl_image == EGL_NO_IMAGE_KHR);
  g_assert (image_texture->priv->cogl_tex == COGL_INVALID_HANDLE);

  G_OBJECT_CLASS (clutter_eglx_egl_image_parent_class)->
    dispose (object);
}

static void
clutter_eglx_egl_image_notify (GObject *object, GParamSpec *pspec)
{
  if (g_str_equal (pspec->name, "pixmap"))
    {
      ClutterEGLXEGLImage        *image = CLUTTER_EGLX_EGL_IMAGE (object);
      ClutterEGLXEGLImagePrivate *priv  = image->priv;
      if (CLUTTER_ACTOR_IS_REALIZED (object) && !priv->use_fallback)
        {
          /* TODO(kwaters): consider just reattaching to the EGLimage instead
          of destroying and rebuilding everything */
          destroy_image_and_texture (image);
          if (!create_image_and_texture (image))
            CLUTTER_NOTE (TEXTURE,
                          "egl_image_notify failed in notify \"pixmap\"");
        }
    }
}

static void
clutter_eglx_egl_image_realize (ClutterActor *actor)
{
  ClutterEGLXEGLImage *image_texture;
  ClutterEGLXEGLImagePrivate *priv;

  image_texture = CLUTTER_EGLX_EGL_IMAGE (actor);
  priv = image_texture->priv;

  if (priv->use_fallback || !create_image_and_texture (image_texture))
    CLUTTER_ACTOR_CLASS (clutter_eglx_egl_image_parent_class)->realize (actor);
}

static void
clutter_eglx_egl_image_unrealize (ClutterActor *actor)
{
  ClutterEGLXEGLImage *image_texture = CLUTTER_EGLX_EGL_IMAGE (actor);
  ClutterEGLXEGLImagePrivate *priv = image_texture->priv;

  if (priv->use_fallback)
    {
      CLUTTER_ACTOR_CLASS (clutter_eglx_egl_image_parent_class)->
        unrealize (actor);
      return;
    }

  destroy_image_and_texture (image_texture);
}

static void
clutter_eglx_egl_image_update_area (ClutterX11TexturePixmap *texture,
                                    gint                     x,
                                    gint                     y,
                                    gint                     width,
                                    gint                     height)
{
  ClutterEGLXEGLImage        *image = CLUTTER_EGLX_EGL_IMAGE (texture);
  ClutterEGLXEGLImagePrivate *priv  = image->priv;

  if (!CLUTTER_ACTOR_IS_REALIZED (texture))
    return;

  if (priv->use_fallback)
    {
      CLUTTER_X11_TEXTURE_PIXMAP_CLASS (clutter_eglx_egl_image_parent_class)->
        update_area(texture,
                    x, y,
                    width, height);
      return;
    }

  clutter_actor_queue_redraw (CLUTTER_ACTOR (texture));
}

static void
clutter_eglx_egl_image_class_init (ClutterEGLXEGLImageClass *klass)
{
  GObjectClass          *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass     *actor_class = CLUTTER_ACTOR_CLASS (klass);
  ClutterX11TexturePixmapClass *x11_texture_class =
    CLUTTER_X11_TEXTURE_PIXMAP_CLASS (klass);

  g_type_class_add_private (klass,
                            sizeof (ClutterEGLXEGLImageClass));

  object_class->dispose = clutter_eglx_egl_image_dispose;
  object_class->notify  = clutter_eglx_egl_image_notify;


  actor_class->realize   = clutter_eglx_egl_image_realize;
  actor_class->unrealize = clutter_eglx_egl_image_unrealize;

  x11_texture_class->update_area =
    clutter_eglx_egl_image_update_area;
}

static gboolean
create_image_and_texture (ClutterEGLXEGLImage *self)
{
  static const EGLint image_attribs[] = {
    EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
    EGL_NONE
  };

  ClutterEGLXEGLImagePrivate *priv = self->priv;
  ClutterBackendEGL *backend_egl;
  Pixmap             pixmap;
  guint              pixmap_width = 0;
  guint              pixmap_height = 0;
  EGLContext         context;
  EGLImageKHR        image;
  GLuint             tex;
  CoglHandle         handle;

  backend_egl = CLUTTER_BACKEND_EGL (clutter_get_default_backend ());
  context = backend_egl->egl_context;

  g_object_get (self,
                "pixmap", &pixmap,
                "pixmap-width", &pixmap_width,
                "pixmap-height", &pixmap_height,
                NULL);

  CLUTTER_NOTE (TEXTURE, "pixmap=0x%x pixmap-width=%d pixmap-height=%d\n",
                (guint)pixmap, pixmap_width, pixmap_height);

  if (pixmap == None)
    return FALSE;

  g_assert (_egl_create_image_khr);
  image = _egl_create_image_khr (clutter_eglx_display (),
                                 context,
                                 EGL_NATIVE_PIXMAP_KHR,
                                 (EGLClientBuffer)pixmap,
                                 image_attribs);

  if (image == EGL_NO_IMAGE_KHR)
    goto fail;

  priv->egl_image = image;

  glGenTextures (1, &tex);
  glBindTexture (GL_TEXTURE_2D, tex);

  CLUTTER_NOTE (TEXTURE, "image=0x%x tex=0x%x", (guint)image, (guint)tex);

  g_assert (_gl_egl_image_target_texture_2d_oes);
  _gl_egl_image_target_texture_2d_oes (GL_TEXTURE_2D, (GLeglImageOES)image);

  /* There is no way to determine the depth of an EGL Image.  Fortunately,
  depth is only important for readback, which cannot be done in GLES, so we
  spoof it as RGBA_8888 */
  handle = cogl_texture_new_from_foreign (tex, GL_TEXTURE_2D,
                                          pixmap_width, pixmap_height, 0, 0,
                                          COGL_PIXEL_FORMAT_RGBA_8888);
  if (handle)
    {
      priv->cogl_tex = handle;
      clutter_texture_set_cogl_texture (CLUTTER_TEXTURE (self), handle);

      return TRUE;
    }

fail:
  CLUTTER_NOTE (TEXTURE, "create_image_and_texture failed.");

  /* cleanup GL resources */
  destroy_image_and_texture (self);

  priv->use_fallback = TRUE;
  return FALSE;
}

static void
destroy_image_and_texture (ClutterEGLXEGLImage *self)
{
  ClutterEGLXEGLImagePrivate *priv = self->priv;

  if (priv->cogl_tex != COGL_INVALID_HANDLE)
    {
      GLenum gl_target;
      GLuint gl_handle;

      /* It seems that if there are other references to this texture that are
      live we're going to be deleting it behind their backs.  I think
      cogl_texture_new_from_foreign is broken by design */
      cogl_texture_get_gl_texture (priv->cogl_tex, &gl_handle, &gl_target);
      cogl_texture_unref (priv->cogl_tex);
      priv->cogl_tex = COGL_INVALID_HANDLE;
      glDeleteTextures (1, &gl_handle);
    }

  if (priv->egl_image != EGL_NO_IMAGE_KHR)
    {
      g_assert (_egl_destroy_image_khr);
      /* eglDestroyImageKHR can only fail due to programmer error.  We report
      the error and ignore it. */
      if (!_egl_destroy_image_khr (clutter_eglx_display (), priv->egl_image))
        CLUTTER_NOTE (TEXTURE, "eglDestroyImageKHR failed.");
      priv->egl_image = EGL_NO_IMAGE_KHR;
    }
}

static void
clutter_eglx_egl_image_init (ClutterEGLXEGLImage *self)
{
  ClutterEGLXEGLImagePrivate *priv;

  priv = self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self,
                                 CLUTTER_EGLX_TYPE_EGL_IMAGE,
                                 ClutterEGLXEGLImagePrivate);

  priv->egl_image = EGL_NO_IMAGE_KHR;
  priv->cogl_tex = COGL_INVALID_HANDLE;

  if (G_UNLIKELY (_ext_check_done == FALSE))
    {
      const char *egl_extensions = NULL;
      const GLubyte *gl_extensions = NULL;
      gboolean extension_check_override;

      egl_extensions = eglQueryString (clutter_eglx_display (),
                                       EGL_EXTENSIONS);
      gl_extensions = glGetString (GL_EXTENSIONS);

      /* TODO(kwaters): remove this extension override environment variable
      when implementors correctly report their supported extensions */
      extension_check_override =
        g_getenv ("CLUTTER_EGL_IMAGE_NO_EXT_CHECK") != NULL;

      if ((cogl_check_extension ("EGL_KHR_image_base", egl_extensions)
           && cogl_check_extension ("EGL_KHR_image_pixmap", egl_extensions))
          || cogl_check_extension ("EGL_KHR_image", egl_extensions)
          || extension_check_override)
        {
          _egl_create_image_khr = (PFNEGLCREATEIMAGEKHRPROC)
            cogl_get_proc_address ("eglCreateImageKHR");
          _egl_destroy_image_khr = (PFNEGLDESTROYIMAGEKHRPROC)
            cogl_get_proc_address ("eglDestroyImageKHR");
        }
      else
       {
         CLUTTER_NOTE (TEXTURE,
                       "EGL_KHR_image_pixmap or EGL_KHR_image_pixmap "
                       "extensions unavailable");
       }

      if (cogl_check_extension ("GL_OES_EGL_image",
                                (const gchar*)gl_extensions)
          || extension_check_override)
        _gl_egl_image_target_texture_2d_oes =
          (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC)
          cogl_get_proc_address ("glEGLImageTargetTexture2DOES");
      else
        CLUTTER_NOTE (TEXTURE, "GL_OES_EGL_image extension unavailable");

      _ext_check_done = TRUE;
    }

  if (!_egl_create_image_khr || !_egl_destroy_image_khr
      || !_gl_egl_image_target_texture_2d_oes)
    {
      priv->use_fallback = TRUE;
      CLUTTER_NOTE (TEXTURE, "EGL Image unavailable using fallback");
    }
  else
    {
      priv->use_fallback = FALSE;
    }
}

/**
 * clutter_eglx_iamge_texture_pixmap_using_extension:
 * @texture: A #ClutterEGLXEGLImage
 *
 * Return value: A boolean indicating if the texture is using EGL Image or
 * falling back to a slower software mechanism.
 *
 * Since: 1.1
 **/
gboolean
clutter_eglx_egl_image_using_extension (ClutterEGLXEGLImage *image)
{
  ClutterEGLXEGLImagePrivate *priv;
  priv = image->priv;

  return !priv->use_fallback;
}

/**
 * clutter_eglx_egl_image_new:
 *
 * Return value: A new #ClutterEGLXEGLImage
 *
 * Since: 1.1
 **/
ClutterActor *
clutter_eglx_egl_image_new (void)
{
  ClutterActor *actor;
  actor = g_object_new (CLUTTER_EGLX_TYPE_EGL_IMAGE, NULL);
  return actor;
}

/**
 * clutter_eglx_egl_image_new_with_pixmap:
 * @pixmap: the X Pixmap to which this EGL Image should be bound
 *
 * Return value: A new #ClutterEGLXEGLImage bound to the given
 * X Pixmap
 *
 * Since: 1.1
 **/
ClutterActor *
clutter_eglx_egl_image_new_with_pixmap (Pixmap pixmap)
{
  ClutterActor *actor;
  actor = g_object_new (CLUTTER_EGLX_TYPE_EGL_IMAGE,
                        "pixmap", pixmap,
                        NULL);
  return actor;
}

/**
 * clutter_eglx_egl_image_new_with_window:
 * @window: the X window to which this EGL Image should be bound
 *
 * Return value: A new #ClutterEGLXEGLImage bound to the given
 * X Window
 *
 * Since: 1.1
 **/
ClutterActor *
clutter_eglx_egl_image_new_with_window (Window window)
{
  ClutterActor *actor;
  actor = g_object_new (CLUTTER_EGLX_TYPE_EGL_IMAGE,
                        "window", window,
                        NULL);
  return actor;
}
