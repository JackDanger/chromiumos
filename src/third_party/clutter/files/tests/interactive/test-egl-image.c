
#include <config.h>

#include <stdlib.h>
#include <gmodule.h>

#include <X11/Xlib.h>
#include <clutter/clutter.h>
#include <clutter/x11/clutter-x11-texture-pixmap.h>
#include <clutter/eglx/clutter-eglx-egl-image.h>

#if HAVE_CLUTTER_EGL

G_MODULE_EXPORT int
test_egl_image_main (int argc, char **argv)
{
  static const ClutterColor stage_color = { 0x1f, 0x84, 0x56, 0xff };
  ClutterActor *stage;
  ClutterActor *texture;
  ClutterX11TexturePixmap *x11_texture;
  Window window;

  clutter_init (&argc, &argv);

  if (argc != 2)
    {
      g_printerr ("usage: clutter-test xid\n");
      return EXIT_FAILURE;
    }

  window = (Window)strtol (argv[1], NULL, 0);
  g_print ("Attempting to redirect window 0x%08x\n", (guint)window);

  stage = clutter_stage_get_default ();
  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);

  texture = clutter_eglx_egl_image_new_with_window (window);
  x11_texture = CLUTTER_X11_TEXTURE_PIXMAP (texture);
  clutter_x11_texture_pixmap_set_automatic (x11_texture, TRUE);

  clutter_actor_set_name (texture, "EGL Image");
  clutter_container_add (CLUTTER_CONTAINER (stage), texture, NULL);

  clutter_actor_set_size (stage, 512, 512);
  clutter_actor_show (stage);

  clutter_main ();

  return EXIT_SUCCESS;
}

#else /* HAVE_CLUTTER_EGL */
G_MODULE_EXPORT int
test_egl_image_main (int argc, char **argv)
{
  return EXIT_SUCCESS;
}
#endif

