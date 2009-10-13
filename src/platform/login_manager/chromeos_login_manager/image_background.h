// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IMAGE_BACKGROUND_H_
#define IMAGE_BACKGROUND_H_

#include "base/basictypes.h"
#include "views/background.h"
#include "app/gfx/canvas.h"

namespace gfx {
class Canvas;
}

namespace views {

class ImageBackground : public Background {
  public:
    explicit ImageBackground(GdkPixbuf* background_image) :
      background_image_(background_image) {
    }

    virtual ~ImageBackground() {
      gdk_pixbuf_unref(background_image_);
    }

    void Paint(gfx::Canvas* canvas, View* view) const {
      canvas->DrawGdkPixbuf(background_image_, 0, 0);
    }
  private:
    // Background image that is drawn by this background.
    // This class takes over ownership and the image is dereferenced
    // upon deletion of the corresponding instance
    GdkPixbuf* background_image_;
    DISALLOW_COPY_AND_ASSIGN(ImageBackground);
};

}  // namespace views

#endif /* IMAGE_BACKGROUND_H_ */
