// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_IMAGE_CONTAINER_H_
#define WINDOW_MANAGER_IMAGE_CONTAINER_H_

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"

namespace window_manager {

// This is the image container base class which knows how to create image
// containers from an appropriate file and load it.  It provides a
// consistent API for loading and accessing loaded images.

class ImageContainer {
 public:
  enum Result {
    IMAGE_LOAD_SUCCESS = 0,
    IMAGE_LOAD_FAILURE,
  };

  enum Format {
    IMAGE_FORMAT_RGBA_32 = 0,
  };

  // This determines the type of image container to use automatically
  // from the file contents, and returns a newly allocated image
  // container of the correct type.  The caller is responsible for
  // deleting the returned image container.  Returns NULL if unable to
  // determine file type or access the file.  Note that the image data
  // isn't loaded until the LoadImage method returns successfully.
  static ImageContainer* CreateContainer(const std::string& filename);

  // Create a new container for this file.
  explicit ImageContainer(const std::string& filename) : filename_(filename) {}
  virtual ~ImageContainer() {}

  // Loads the image, and returns a result code.
  virtual Result LoadImage() = 0;

  const std::string& filename() const { return filename_; }
  char* data() const { return data_.get(); }
  int width() const { return width_; }
  int height() const { return height_; }

  // Return stride in bytes of a row of pixels in the image data.
  int stride() const {
    return channels() * bits_per_channel() * width() / 8;
  }

  // The number of channels in the image.
  int channels() const { return 4; }

  // The number of bits per channel in the image.
  int bits_per_channel() const { return 8; }

  // Currently, this class only supports results in 32-bit RGBA
  // format.  When other formats are added, they should be added to
  // the Format enum, and accessors made to support them.
  Format format() { return IMAGE_FORMAT_RGBA_32; }

 protected:
  // Takes ownership of the given new allocated array.
  void set_data(char *new_data) { data_.reset(new_data); }

  // Set parameters read from image.
  void set_width(int new_width) { width_ = new_width; }
  void set_height(int new_height) { height_ = new_height; }

 private:
  // The filename we were constructed with.
  std::string filename_;

  // The associated 32-bit RGBA data oriented with (0, 0) in the
  // bottom left.  LoadImage must have been called first, or this will
  // return NULL.  The returned pointer is still owned by this object.
  scoped_array<char> data_;

  // Width in pixels of a row in the image.
  uint32 width_;

  // Height in pixels of a column in the image.
  uint32 height_;

  DISALLOW_COPY_AND_ASSIGN(ImageContainer);
};

// This is the PNG-specific version of the image container.  It can
// detect PNG image files from their contents, and load them into
// memory, converting them to the proper form for the ImageContainer
// class.
class PngImageContainer : public virtual ImageContainer {
 public:
  // Determines if the given file is a PNG image.
  static bool IsPngImage(const std::string& filename);

  explicit PngImageContainer(const std::string& filename);
  virtual ~PngImageContainer() {}

  ImageContainer::Result LoadImage();

 private:
  DISALLOW_COPY_AND_ASSIGN(PngImageContainer);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_IMAGE_CONTAINER_H_
