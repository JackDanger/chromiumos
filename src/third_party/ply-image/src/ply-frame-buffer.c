/* ply-frame-buffer.c - framebuffer abstraction
 *
 * Copyright (C) 2006, 2007, 2008 Red Hat, Inc.
 *               2008 Charlie Brej <cbrej@cs.man.ac.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 *
 * Written by: Charlie Brej <cbrej@cs.man.ac.uk>
 *             Kristian Høgsberg <krh@redhat.com>
 *             Ray Strode <rstrode@redhat.com>
 */

#include "ply-list.h"
#include "ply-frame-buffer.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <values.h>
#include <unistd.h>

#include <linux/fb.h>

#ifndef PLY_FRAME_BUFFER_DEFAULT_FB_DEVICE_NAME
#define PLY_FRAME_BUFFER_DEFAULT_FB_DEVICE_NAME "/dev/fb0"
#endif

struct _ply_frame_buffer
{
  char *device_name;
  int   device_fd;

  char *map_address;
  size_t size;

  uint32_t *shadow_buffer;

  uint32_t red_bit_position;
  uint32_t green_bit_position;
  uint32_t blue_bit_position;
  uint32_t alpha_bit_position;

  uint32_t bits_for_red;
  uint32_t bits_for_green;
  uint32_t bits_for_blue;
  uint32_t bits_for_alpha;

  int32_t dither_red;
  int32_t dither_green;
  int32_t dither_blue;

  unsigned int bytes_per_pixel;
  unsigned int row_stride;

  ply_frame_buffer_area_t area;
  ply_list_t *areas_to_flush;

  void (*flush_area) (ply_frame_buffer_t      *buffer,
                      ply_frame_buffer_area_t *area_to_flush);

  int pause_count;
};

static bool ply_frame_buffer_open_device (ply_frame_buffer_t  *buffer);
static void ply_frame_buffer_close_device (ply_frame_buffer_t *buffer);
static bool ply_frame_buffer_query_device (ply_frame_buffer_t *buffer);
static bool ply_frame_buffer_map_to_device (ply_frame_buffer_t *buffer);
static inline uint_fast32_t ply_frame_buffer_pixel_value_to_device_pixel_value (
    ply_frame_buffer_t *buffer,
    uint32_t        pixel_value);

static bool
ply_frame_buffer_open_device (ply_frame_buffer_t  *buffer)
{
  assert (buffer != NULL);
  assert (buffer->device_name != NULL);

  buffer->device_fd = open (buffer->device_name, O_RDWR);

  if (buffer->device_fd < 0)
    {
      return false;
    }

  return true;
}

static void
ply_frame_buffer_close_device (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  if (buffer->map_address != MAP_FAILED)
    {
      munmap (buffer->map_address, buffer->size);
      buffer->map_address = MAP_FAILED;
    }

  if (buffer->device_fd >= 0)
    {
      close (buffer->device_fd);
      buffer->device_fd = -1;
    }
}

static void
flush_area_to_any_device (ply_frame_buffer_t      *buffer,
                          ply_frame_buffer_area_t *area_to_flush)
{
  unsigned long row, column;
  char *row_buffer;
  size_t bytes_per_row;
  unsigned long x1, y1, x2, y2;

  x1 = area_to_flush->x;
  y1 = area_to_flush->y;
  x2 = x1 + area_to_flush->width;
  y2 = y1 + area_to_flush->height;

  bytes_per_row = area_to_flush->width * buffer->bytes_per_pixel;
  row_buffer = malloc (buffer->row_stride * buffer->bytes_per_pixel);
  for (row = y1; row < y2; row++)
    {
      unsigned long offset;

      for (column = x1; column < x2; column++)
        {
          uint32_t pixel_value;
          uint_fast32_t device_pixel_value;

          pixel_value = buffer->shadow_buffer[row * buffer->area.width + column];

          device_pixel_value =
            ply_frame_buffer_pixel_value_to_device_pixel_value (buffer,
                                                                pixel_value);

          memcpy (row_buffer + column * buffer->bytes_per_pixel,
                  &device_pixel_value, buffer->bytes_per_pixel);
        }

      offset = row * buffer->row_stride * buffer->bytes_per_pixel + x1 * buffer->bytes_per_pixel;
      memcpy (buffer->map_address + offset, row_buffer + x1 * buffer->bytes_per_pixel,
              area_to_flush->width * buffer->bytes_per_pixel);
    }
  free (row_buffer);
}

static void
flush_area_to_xrgb32_device (ply_frame_buffer_t      *buffer,
                             ply_frame_buffer_area_t *area_to_flush)
{
  unsigned long x1, y1, x2, y2, y;
  char *dst, *src;

  x1 = area_to_flush->x;
  y1 = area_to_flush->y;
  x2 = x1 + area_to_flush->width;
  y2 = y1 + area_to_flush->height;

  dst = &buffer->map_address[(y1 * buffer->row_stride + x1) * 4];
  src = (char *) &buffer->shadow_buffer[y1 * buffer->area.width + x1];

  if (area_to_flush->width == buffer->row_stride)
    {
      memcpy (dst, src, area_to_flush->width * area_to_flush->height * 4);
      return;
    }

  for (y = y1; y < y2; y++)
    {
      memcpy (dst, src, area_to_flush->width * 4);
      dst += buffer->row_stride * 4;
      src += buffer->area.width * 4;
    }
}


static bool
ply_frame_buffer_query_device (ply_frame_buffer_t *buffer)
{
  struct fb_var_screeninfo variable_screen_info;
  struct fb_fix_screeninfo fixed_screen_info;

  assert (buffer != NULL);
  assert (buffer->device_fd >= 0);

  if (ioctl (buffer->device_fd, FBIOGET_VSCREENINFO, &variable_screen_info) < 0)
    return false;

  if (ioctl(buffer->device_fd, FBIOGET_FSCREENINFO, &fixed_screen_info) < 0)
    return false;

  /* Normally the pixel is divided into channels between the color components.
   * Each channel directly maps to a color channel on the hardware.
   *
   * There are some odd ball modes that use an indexed palette instead.  In
   * those cases (pseudocolor, direct color, etc), the pixel value is just an
   * index into a lookup table of the real color values.
   *
   * We don't support that.
   */
  if (fixed_screen_info.visual != FB_VISUAL_TRUECOLOR)
    {
      int rc = -1;
      int i;
      int depths[] = {32, 24, 16, 0};

      // ply_trace("Visual was %s, trying to find usable mode.\n",
      // p_visual(fixed_screen_info.visual));

      for (i = 0; depths[i] != 0; i++)
        {
          variable_screen_info.bits_per_pixel = depths[i];
          variable_screen_info.activate |= FB_ACTIVATE_NOW | FB_ACTIVATE_FORCE;

          rc = ioctl(buffer->device_fd, FBIOPUT_VSCREENINFO, &variable_screen_info);
          if (rc >= 0)
            {
              if (ioctl(buffer->device_fd, FBIOGET_FSCREENINFO, &fixed_screen_info) < 0)
                return false;
              if (fixed_screen_info.visual == FB_VISUAL_TRUECOLOR)
                break;
            }
        }

      if (ioctl(buffer->device_fd, FBIOGET_VSCREENINFO, &variable_screen_info) < 0)
        return false;

      if (ioctl(buffer->device_fd, FBIOGET_FSCREENINFO, &fixed_screen_info) < 0)
        return false;
    }

  if (fixed_screen_info.visual != FB_VISUAL_TRUECOLOR ||
      variable_screen_info.bits_per_pixel < 16)
    {
	    // ply_trace("Visual is %s; not using graphics\n",
	    // p_visual(fixed_screen_info.visual));
      return false;
    }

  buffer->area.x = variable_screen_info.xoffset;
  buffer->area.y = variable_screen_info.yoffset;
  buffer->area.width = variable_screen_info.xres;
  buffer->area.height = variable_screen_info.yres;

  buffer->red_bit_position = variable_screen_info.red.offset;
  buffer->bits_for_red = variable_screen_info.red.length;

  buffer->green_bit_position = variable_screen_info.green.offset;
  buffer->bits_for_green = variable_screen_info.green.length;

  buffer->blue_bit_position = variable_screen_info.blue.offset;
  buffer->bits_for_blue = variable_screen_info.blue.length;

  buffer->alpha_bit_position = variable_screen_info.transp.offset;
  buffer->bits_for_alpha = variable_screen_info.transp.length;

  buffer->bytes_per_pixel = variable_screen_info.bits_per_pixel >> 3;
  buffer->row_stride = fixed_screen_info.line_length / buffer->bytes_per_pixel;
  buffer->size = buffer->area.height * buffer->row_stride * buffer->bytes_per_pixel;
  
  buffer->dither_red = 0;
  buffer->dither_green = 0;
  buffer->dither_blue = 0;

  if (buffer->bytes_per_pixel == 4 &&
      buffer->red_bit_position == 16 && buffer->bits_for_red == 8 &&
      buffer->green_bit_position == 8 && buffer->bits_for_green == 8 &&
      buffer->blue_bit_position == 0 && buffer->bits_for_blue == 8)
    buffer->flush_area = flush_area_to_xrgb32_device;
  else
    buffer->flush_area = flush_area_to_any_device;

  return true;
}

static bool
ply_frame_buffer_map_to_device (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  assert (buffer->device_fd >= 0);
  assert (buffer->size > 0);

  buffer->map_address = mmap (NULL, buffer->size, PROT_WRITE,
                              MAP_SHARED, buffer->device_fd, 0);

  return buffer->map_address != MAP_FAILED;
}

static inline uint_fast32_t 
ply_frame_buffer_pixel_value_to_device_pixel_value (ply_frame_buffer_t *buffer,
                                                    uint32_t        pixel_value)
{
  uint8_t r, g, b, a;
  int orig_r, orig_g, orig_b, orig_a;
  int i;
  
  orig_a = pixel_value >> 24; 
  a = orig_a >> (8 - buffer->bits_for_alpha);

  orig_r = ((pixel_value >> 16) & 0xff) - buffer->dither_red; 
  r = CLAMP(orig_r, 0, 255) >> (8 - buffer->bits_for_red);

  orig_g = ((pixel_value >> 8) & 0xff) - buffer->dither_green;
  g = CLAMP(orig_g, 0, 255) >> (8 - buffer->bits_for_green);

  orig_b = (pixel_value & 0xff) - buffer->dither_blue;
  b = CLAMP(orig_b, 0, 255) >> (8 - buffer->bits_for_blue);
  
  uint8_t new_r = r << (8 - buffer->bits_for_red);
  uint8_t new_g = g << (8 - buffer->bits_for_green);
  uint8_t new_b = b << (8 - buffer->bits_for_blue);
  for (i=buffer->bits_for_red;   i<8; i*=2) new_r |= new_r >> i;
  for (i=buffer->bits_for_green; i<8; i*=2) new_g |= new_g >> i;
  for (i=buffer->bits_for_blue;  i<8; i*=2) new_b |= new_b >> i;
  
  buffer->dither_red = new_r - orig_r;
  buffer->dither_green = new_g - orig_g;
  buffer->dither_blue = new_b - orig_b;
  

  return ((a << buffer->alpha_bit_position)
          | (r << buffer->red_bit_position)
          | (g << buffer->green_bit_position)
          | (b << buffer->blue_bit_position));
}


static inline void 
ply_frame_buffer_place_value_at_pixel (ply_frame_buffer_t *buffer,
                                       int             x,
                                       int             y,
                                       uint32_t        pixel_value)
{
  buffer->shadow_buffer[y * buffer->area.width + x] = pixel_value;
}


ply_frame_buffer_t *
ply_frame_buffer_new (const char *device_name)
{
  ply_frame_buffer_t *buffer;

  buffer = calloc (1, sizeof (ply_frame_buffer_t));

  if (device_name != NULL)
    buffer->device_name = strdup (device_name);
  else if (getenv ("FRAMEBUFFER") != NULL)
    buffer->device_name = strdup (getenv ("FRAMEBUFFER"));
  else
    buffer->device_name = 
      strdup (PLY_FRAME_BUFFER_DEFAULT_FB_DEVICE_NAME);

  buffer->map_address = MAP_FAILED;
  buffer->shadow_buffer = NULL;
  buffer->areas_to_flush = ply_list_new ();

  buffer->pause_count = 0;

  return buffer;
}

static void
free_flush_areas (ply_frame_buffer_t *buffer)
{
  ply_list_node_t *node;

  node = ply_list_get_first_node (buffer->areas_to_flush);
  while (node != NULL)
    {
      ply_list_node_t *next_node;
      ply_frame_buffer_area_t *area_to_flush;

      area_to_flush = (ply_frame_buffer_area_t *) ply_list_node_get_data (node);

      next_node = ply_list_get_next_node (buffer->areas_to_flush, node);

      free (area_to_flush);
      ply_list_remove_node (buffer->areas_to_flush, node);

      node = next_node;
    }

  ply_list_free (buffer->areas_to_flush);
}

void
ply_frame_buffer_free (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  if (ply_frame_buffer_device_is_open (buffer))
    ply_frame_buffer_close (buffer);

  free_flush_areas (buffer);

  free (buffer->device_name);
  free (buffer->shadow_buffer);
  free (buffer);
}

bool 
ply_frame_buffer_open (ply_frame_buffer_t *buffer)
{
  bool is_open;

  assert (buffer != NULL);

  is_open = false;

  if (!ply_frame_buffer_open_device (buffer))
    {
      goto out;
    }

  if (!ply_frame_buffer_query_device (buffer))
    {
      goto out;
    }

  if (!ply_frame_buffer_map_to_device (buffer))
    {
      goto out;
    }

  buffer->shadow_buffer =
    realloc (buffer->shadow_buffer, 4 * buffer->area.width * buffer->area.height);
  is_open = true;

out:

  if (!is_open)
    {
      int saved_errno;

      saved_errno = errno;
      ply_frame_buffer_close_device (buffer);
      errno = saved_errno;
    }

  return is_open;
}


bool 
ply_frame_buffer_device_is_open (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  return buffer->device_fd >= 0 && buffer->map_address != MAP_FAILED;
}

char *
ply_frame_buffer_get_device_name (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));
  assert (buffer->device_name != NULL);

  return strdup (buffer->device_name);
}

void
ply_frame_buffer_set_device_name (ply_frame_buffer_t *buffer,
                                  const char     *device_name)
{
  assert (buffer != NULL);
  assert (!ply_frame_buffer_device_is_open (buffer));
  assert (device_name != NULL);
  assert (buffer->device_name != NULL);

  if (strcmp (buffer->device_name, device_name) != 0)
    {
      free (buffer->device_name);
      buffer->device_name = strdup (device_name);
    }
}

void 
ply_frame_buffer_close (ply_frame_buffer_t *buffer)
{
  assert (buffer != NULL);

  assert (ply_frame_buffer_device_is_open (buffer));
  ply_frame_buffer_close_device (buffer);

  buffer->bytes_per_pixel = 0;
  buffer->area.x = 0;
  buffer->area.y = 0;
  buffer->area.width = 0;
  buffer->area.height = 0;
}

void 
ply_frame_buffer_get_size (ply_frame_buffer_t     *buffer,
                           ply_frame_buffer_area_t *size)
{
  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));
  assert (size != NULL);

  *size = buffer->area;
}


bool 
ply_frame_buffer_fill (ply_frame_buffer_t      *buffer,
		       ply_frame_buffer_area_t *area,
		       unsigned long            x,
		       unsigned long            y,
		       uint32_t                *data)
{
  char *dst, *src;
  int hdiff, vdiff;

  assert (buffer != NULL);
  assert (ply_frame_buffer_device_is_open (buffer));
  assert (area != NULL);

  hdiff = area->width - buffer->row_stride;
  vdiff = area->height - buffer->area.height;

  if (hdiff >= 0)
    {
      /* image is wider than buffer */
      dst = &buffer->map_address[0];
      src = (char *) (data + hdiff / 2);
    } else {
      dst = &buffer->map_address[(-hdiff / 2) * sizeof(*data)];
      src = (char *) data;
    }

  if (vdiff >= 0)
    {
      /* image is taller than buffer */
      src += (vdiff / 2) * area->width * sizeof(*data);
    }
  else
    {
      dst += (-vdiff / 2) * buffer->row_stride * sizeof(*data);
    }

  if (hdiff == 0)
    {
      memcpy (dst, src, area->width * area->height * sizeof(*data));
    }
  else
    {
      int line;
      int lines = vdiff > 0 ? buffer->area.height : area->height;
      int width = hdiff > 0 ? buffer->area.width : area->width;
    
      for (line = 0; line < lines; line++) {
	memcpy (dst, src, width * sizeof(*data));
	dst += buffer->row_stride * sizeof(*data);
	src += area->width * sizeof(*data);
      }
    }

  return 1;
}


const char *
ply_frame_buffer_get_bytes (ply_frame_buffer_t *buffer)
{
  return (char *) buffer->shadow_buffer;
}
