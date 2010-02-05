// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_OPENGL_VISITOR_H_
#define WINDOW_MANAGER_OPENGL_VISITOR_H_

#include <GL/glx.h>

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/gl_interface.h"
#include "window_manager/image_container.h"
#include "window_manager/tidy_interface.h"
#include "window_manager/x_connection.h"

namespace window_manager {

class OpenGlDrawVisitor;

class OpenGlQuadDrawingData : public TidyInterface::DrawingData  {
 public:
  OpenGlQuadDrawingData(GLInterface* gl_interface);
  virtual ~OpenGlQuadDrawingData();

  GLuint vertex_buffer() { return vertex_buffer_; }
  void set_vertex_buffer(GLuint vertex_buffer);

 private:
  // This is the gl interface to use for communicating with GL.
  GLInterface* gl_interface_;

  // This is the vertex buffer that holds the rect we use for
  // rendering quads.
  GLuint vertex_buffer_;
};

class OpenGlPixmapData : public TidyInterface::DrawingData  {
 public:
  OpenGlPixmapData(GLInterface* gl_interface,
                   XConnection* x_conn);
  virtual ~OpenGlPixmapData();

  void Refresh();

  // This creates a new OpenGlTextureData for the given actor, setting
  // up the texture id on the texture data object, and attaching it to
  // the actor.  Returns false if texture cannot be bound.
  static bool BindToPixmap(OpenGlDrawVisitor* visitor,
                           TidyInterface::TexturePixmapActor* actor);

  void SetTexture(GLuint texture, bool has_alpha);

  XPixmap pixmap() const { return pixmap_; }
  GLuint texture() const { return texture_; }
  bool has_alpha() const { return has_alpha_; }

 private:
  // This is the gl interface to use for communicating with GL.
  GLInterface* gl_interface_;

  // This is the X connection to use for communicating with X.
  XConnection* x_conn_;

  // This is the texture ID of the bound texture.
  GLuint texture_;

  // This is the compositing pixmap associated with the window.
  XPixmap pixmap_;

  // This is the GLX pixmap we draw into, created from the pixmap above.
  GLXPixmap glx_pixmap_;

  // This is the id of the damage region.
  XID damage_;

  // Whether or not this pixmap has an alpha channel.
  bool has_alpha_;
};

class OpenGlTextureData : public TidyInterface::DrawingData  {
 public:
  OpenGlTextureData(GLInterface* gl_interface);
  virtual ~OpenGlTextureData();

  void SetTexture(GLuint texture, bool has_alpha);

  GLuint texture() const { return texture_; }
  bool has_alpha() const { return has_alpha_; }

 private:
  // This is the gl interface to use for communicating with GL.
  GLInterface* gl_interface_;

  // This is the texture ID of the bound texture.
  GLuint texture_;

  // True if associated texture has an alpha channel.
  bool has_alpha_;
};

// This class visits an actor tree and draws it using OpenGL.
class OpenGlDrawVisitor
    : virtual public TidyInterface::ActorVisitor {
 public:

  // These are IDs used when storing drawing data on the actors.
  enum DataId {
    TEXTURE_DATA = 1,
    PIXMAP_DATA = 2,
    DRAWING_DATA = 3,
  };

  OpenGlDrawVisitor(GLInterfaceBase* gl_interface,
                    TidyInterface* interface,
                    ClutterInterface::StageActor* stage);
  virtual ~OpenGlDrawVisitor();

  void BindImage(const ImageContainer* container,
                 TidyInterface::QuadActor* actor);

  virtual void VisitActor(TidyInterface::Actor* actor);
  virtual void VisitStage(TidyInterface::StageActor* actor);
  virtual void VisitContainer(TidyInterface::ContainerActor* actor);
  virtual void VisitTexturePixmap(
      TidyInterface::TexturePixmapActor* actor);
  virtual void VisitQuad(TidyInterface::QuadActor* actor);

 private:
  // So it can get access to the config data.
  friend class OpenGlPixmapData;

  // This draws a debugging "needle" in the upper left corner.
  void DrawNeedle();

  GLInterface* gl_interface_;  // Not owned.
  TidyInterface* interface_;  // Not owned.
  XConnection* x_conn_;  // Not owned.

  // This holds the drawing data used for quads.  Note that only
  // QuadActors use this drawing data, and they all share the same
  // one (to keep from allocating a lot of quad vertex buffers).
  TidyInterface::DrawingDataPtr quad_drawing_data_;

  GLXFBConfig config_24_;
  GLXFBConfig config_32_;
  GLXContext context_;

  // If set to true, this indicates that we will be visiting only
  // opaque actors (in front to back order), and if false, only (at
  // least partially) transparent ones (in back to front order).
  bool visit_opaque_;

  // This is the cumulative opacity of all the ancestors of the
  // currently visited node. It is recalculated each time we enter or
  // leave a container node.
  float ancestor_opacity_;

  // This keeps track of the number of frames drawn so we can draw the
  // debugging needle.
  int num_frames_drawn_;

  DISALLOW_COPY_AND_ASSIGN(OpenGlDrawVisitor);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_OPENGL_VISITOR_H_
