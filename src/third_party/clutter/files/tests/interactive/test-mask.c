#include <clutter/clutter.h>
#include <glib.h>
#include <gmodule.h>
#include <stdlib.h>

static CoglHandle
make_mask ()
{
  static const gint radius = 30;
  static const gint half_width = 64;
  static const gint half_height = 64;

  CoglHandle texture;
  guchar *data = NULL;
  guchar *p;
  gint x;
  gint y;

  data = g_malloc(2 * half_width * 2 * half_height);
  p = data;
  for (y = -half_height; y < half_height; ++y)
    for (x = -half_width; x < half_width; ++x)
      {
        *p++ = x * x + y * y < radius * radius ? 0x0u : 0xffu;
      }

  texture = cogl_texture_new_from_data (2 * half_width, 2 * half_height,
                                        COGL_TEXTURE_NO_AUTO_MIPMAP,
                                        COGL_PIXEL_FORMAT_A_8,
                                        COGL_PIXEL_FORMAT_A_8,
                                        2 * half_width,
                                        data);

  return texture;
}

G_MODULE_EXPORT int
test_mask_main (int argc, char *argv[])
{
  static const ClutterColor stage_color = { 0x1f, 0x84, 0x56, 0xff };

  ClutterActor *stage;
  ClutterActor *hand;
  CoglHandle mask;
  CoglHandle material;
  GError *error;

  clutter_init (&argc, &argv);

  error = NULL;
  hand = clutter_texture_new_from_file ("redhand.png", &error);
  if (hand == NULL)
    {
      g_error ("image load failed: %s", error->message);
      return EXIT_FAILURE;
    }

  mask = make_mask ();
  material = clutter_texture_get_cogl_material (CLUTTER_TEXTURE (hand));
  g_print ("layers = %d\n", cogl_material_get_n_layers (material));
  cogl_material_set_layer (material, 1, mask);
  cogl_material_set_layer_combine (material, 1,
                                   "RGB = MODULATE (PREVIOUS, TEXTURE[A]) "
                                   "A = MODULATE (PREVIOUS, TEXTURE) ",
                                   NULL);

  g_print ("layers = %d\n", cogl_material_get_n_layers (material));

  stage = clutter_stage_get_default ();
  clutter_container_add (CLUTTER_CONTAINER (stage), hand, NULL);

  clutter_stage_set_color (CLUTTER_STAGE (stage), &stage_color);
  clutter_actor_show (stage);

  g_print ("layers = %d\n", cogl_material_get_n_layers (material));
  clutter_main ();

  return EXIT_SUCCESS;
}


