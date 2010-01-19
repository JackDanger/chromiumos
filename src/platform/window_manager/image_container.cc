// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/image_container.h"

#include <png.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace window_manager {

static const int kPngSignatureSize = 8;

using std::string;

// static
ImageContainer* ImageContainer::CreateContainer(const string& filename) {
  if (PngImageContainer::IsPngImage(filename)) {
    return new PngImageContainer(filename);
  } else {
    LOG(ERROR) << "Unable to determine file type of '" << filename
               << "' in ImageContainer::CreateContainer";
    return NULL;
  }
}

////////// Begin PngImageContainer Functions ///////////////

// static
bool PngImageContainer::IsPngImage(const string& filename) {
  // Load the image.
  FILE *fp = fopen(filename.c_str(), "rb");
  if (!fp) {
    LOG(ERROR) << "Unable to open '" << filename
               << "' for reading in IsPngImage.";
    return false;
  }

  // Allocate a buffer where we can put the file signature.
  png_byte pngsig[kPngSignatureSize];

  // Read the signature from the file into the signature buffer.
  size_t bytes_read = fread(&pngsig[0], sizeof(png_byte),
                            kPngSignatureSize, fp);
  fclose(fp);

  if (bytes_read != (sizeof(png_byte) * kPngSignatureSize)) {
    LOG(ERROR) << "Unable to read data from '" << filename
               << "' in IsPngImage.";
    return false;
  }

  return png_sig_cmp(pngsig, 0, kPngSignatureSize) == 0 ? true : false;
}

PngImageContainer::PngImageContainer(const string& filename)
    : ImageContainer(filename) {
}

static void PngErrorHandler(png_structp container_ptr,
                            png_const_charp error_str) {
  PngImageContainer* container =
      reinterpret_cast<PngImageContainer*>(container_ptr);
  LOG(ERROR) << "PNG error while reading '" << container->filename()
             << "':" << error_str;
}

static void PngWarningHandler(png_structp container_ptr,
                              png_const_charp error_str) {
  PngImageContainer* container =
      reinterpret_cast<PngImageContainer*>(container_ptr);
  LOG(WARNING) << "PNG warning while reading '" << container->filename()
               << "':" << error_str;
}

ImageContainer::Result PngImageContainer::LoadImage() {
  png_structp read_obj = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                dynamic_cast<void*>(this),
                                                PngErrorHandler,
                                                PngWarningHandler);
  if (!read_obj) {
    LOG(ERROR) << "Couldn't initialize png read struct in LoadImage.";
    return ImageContainer::IMAGE_LOAD_FAILURE;
  }

  png_infop info_obj = png_create_info_struct(read_obj);
  if (!info_obj) {
    LOG(ERROR) << "Couldn't initialize png info struct in LoadImage.";
    png_destroy_read_struct(&read_obj, NULL, NULL);
    return ImageContainer::IMAGE_LOAD_FAILURE;
  }

  // Load the image.
  FILE *fp = fopen(filename().c_str(), "rb");
  if (!fp) {
    LOG(ERROR) << "Unable to open '" << filename()
               << "' for reading in LoadImage.";
    png_destroy_read_struct(&read_obj, &info_obj, NULL);
    return ImageContainer::IMAGE_LOAD_FAILURE;
  }

  png_init_io(read_obj, fp);
  png_read_info(read_obj, info_obj);
  set_width(png_get_image_width(read_obj, info_obj));
  set_height(png_get_image_height(read_obj, info_obj));
  png_uint_32 color_type = png_get_color_type(read_obj, info_obj);
  png_uint_32 depth = png_get_bit_depth(read_obj, info_obj);

  switch (color_type) {
    case PNG_COLOR_TYPE_PALETTE:
      // Read paletted images as RGB
      png_set_palette_to_rgb(read_obj);
      break;
    case PNG_COLOR_TYPE_GRAY_ALPHA:
    case PNG_COLOR_TYPE_GRAY:
      // Expand smaller bit depths to eight-bit.
      if (depth < 8)
        png_set_gray_1_2_4_to_8(read_obj);
      // Convert grayscale images to RGB.
      png_set_gray_to_rgb(read_obj);
      break;
    default:
      break;
  }

  // Add an opaque alpha channel if there isn't one already.
  png_set_filler(read_obj, 0xff, PNG_FILLER_AFTER);

  // If the image has a transperancy color set, convert it to an alpha
  // channel.
  if (png_get_valid(read_obj, info_obj, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(read_obj);
  }

  // We don't support 16 bit precision, so if the image has 16 bits
  // per channel, truncate it to 8 bits.
  if (depth == 16) {
    png_set_strip_16(read_obj);
  }

  scoped_array<char*> row_pointers(new char*[height()]);
  set_data(new char[height() * stride()]);

  for (int i = 0; i < height(); i++) {
    uint32 position = (height() - i - 1) * stride();
    row_pointers[i] = data() + position;
  }

  png_read_image(read_obj, reinterpret_cast<png_byte**>(row_pointers.get()));

  png_destroy_read_struct(&read_obj, &info_obj, NULL);
  fclose(fp);

  LOG(INFO) << "Successfully loaded image '" << filename() << "' ("
            << width() << "x" << height() << ", "
            << channels() << " channel(s), "
            << bits_per_channel() << " bit(s)/channel)";

  return ImageContainer::IMAGE_LOAD_SUCCESS;
}

}  // namespace window_manager
