// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/opengl_visitor.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <gdk/gdkx.h>
#include <gflags/gflags.h>
#include <sys/time.h>
#include <time.h>
#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/randr.h>
#include <xcb/shape.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "window_manager/gl_interface.h"
#include "window_manager/image_container.h"

DECLARE_bool(tidy_display_debug_needle);

#ifndef TIDY_OPENGL
#error Need TIDY_OPENGL defined to compile this file
#endif

// Turn this on if you want to debug something in this file in depth.
#undef EXTRA_LOGGING

// #define GL_ERROR_DEBUGGING
#ifdef GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() do {                                              \
    GLenum gl_error = gl_interface()->GetError();                          \
    LOG_IF(ERROR, gl_error != GL_NO_ERROR) << "GL Error :" << gl_gl_error; \
  } while (0)
#else  // GL_ERROR_DEBUGGING
#define CHECK_GL_ERROR() void(0)
#endif  // GL_ERROR_DEBUGGING

namespace window_manager {

OpenGlQuadDrawingData::OpenGlQuadDrawingData(GLInterface* gl_interface)
    : gl_interface_(gl_interface) {
  gl_interface_->GenBuffers(1, &vertex_buffer_);
  gl_interface_->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  static float kQuad[] = {
    0.f, 0.f,
    0.f, 1.f,
    1.f, 0.f,
    1.f, 1.f,
  };

  gl_interface_->BufferData(GL_ARRAY_BUFFER, sizeof(kQuad),
                            kQuad, GL_STATIC_DRAW);
}

OpenGlQuadDrawingData::~OpenGlQuadDrawingData() {
  if (vertex_buffer_)
    gl_interface_->DeleteBuffers(1, &vertex_buffer_);
}

void OpenGlQuadDrawingData::set_vertex_buffer(GLuint vertex_buffer) {
  // Delete the old one first.
  if (vertex_buffer_)
    gl_interface_->DeleteBuffers(1, &vertex_buffer_);
  vertex_buffer_ = vertex_buffer;
}

OpenGlPixmapData::OpenGlPixmapData(GLInterface* gl_interface,
                                   XConnection* x_conn)
    : gl_interface_(gl_interface),
      x_conn_(x_conn),
      texture_(0),
      pixmap_(XCB_NONE),
      glx_pixmap_(XCB_NONE),
      damage_(XCB_NONE),
      has_alpha_(false) {}

OpenGlPixmapData::~OpenGlPixmapData() {
  if (damage_) {
    x_conn_->DestroyDamage(damage_);
    damage_ = XCB_NONE;
  }
  if (texture_) {
    gl_interface_->DeleteTextures(1, &texture_);
    texture_ = 0;
  }
  if (glx_pixmap_) {
    gl_interface_->DestroyGlxPixmap(glx_pixmap_);
    glx_pixmap_ = XCB_NONE;
  }
  if (pixmap_) {
    x_conn_->FreePixmap(pixmap_);
    pixmap_ = XCB_NONE;
  }
}

void OpenGlPixmapData::Refresh() {
  LOG_IF(ERROR, !texture_) << "Refreshing with no texture.";
  if (!texture_)
    return;

  gl_interface_->BindTexture(GL_TEXTURE_2D, texture_);
  gl_interface_->ReleaseGlxTexImage(glx_pixmap_, GLX_FRONT_LEFT_EXT);
  gl_interface_->BindGlxTexImage(glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);
  if (damage_) {
    x_conn_->SubtractRegionFromDamage(damage_, XCB_NONE, XCB_NONE);
  }
}

void OpenGlPixmapData::SetTexture(GLuint texture, bool has_alpha) {
  if (texture_ && texture_ != texture) {
    gl_interface_->DeleteTextures(1, &texture_);
  }
  texture_ = texture;
  has_alpha_ = has_alpha;
  Refresh();
}

// static
bool OpenGlPixmapData::BindToPixmap(
    OpenGlDrawVisitor* visitor,
    TidyInterface::TexturePixmapActor* actor) {
  GLInterface* gl_interface = visitor->gl_interface_;
  XConnection* x_conn = visitor->x_conn_;

  CHECK(actor);
  if (!actor->texture_pixmap_window()) {
    // This just means that the window hasn't been mapped yet, so
    // we don't have a pixmap to bind to yet.
    return false;
  }
  CHECK(!actor->GetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA))
      << "Pixmap data already exists.";

  scoped_ptr<OpenGlPixmapData> data(new OpenGlPixmapData(gl_interface, x_conn));

  data->pixmap_ = x_conn->GetCompositingPixmapForWindow(
      actor->texture_pixmap_window());
  if (data->pixmap_ == XCB_NONE) {
    return false;
  }

  XConnection::WindowGeometry geometry;
  x_conn->GetWindowGeometry(data->pixmap_, &geometry);
  int attribs[] = {
    GLX_TEXTURE_FORMAT_EXT,
    geometry.depth == 32 ?
      GLX_TEXTURE_FORMAT_RGBA_EXT :
      GLX_TEXTURE_FORMAT_RGB_EXT,
    GLX_TEXTURE_TARGET_EXT,
    GLX_TEXTURE_2D_EXT,
    0
  };
  data->has_alpha_ = (geometry.depth == 32);
  GLXFBConfig config = geometry.depth == 32 ?
                       visitor->config_32_ :
                       visitor->config_24_;
  data->glx_pixmap_ = gl_interface->CreateGlxPixmap(config,
                                                    data->pixmap_,
                                                    attribs);
  CHECK(data->glx_pixmap_ != XCB_NONE) << "Newly created GLX Pixmap is NULL";

  gl_interface->GenTextures(1, &data->texture_);
  gl_interface->BindTexture(GL_TEXTURE_2D, data->texture_);
  gl_interface->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl_interface->BindGlxTexImage(data->glx_pixmap_, GLX_FRONT_LEFT_EXT, NULL);
  data->damage_ = x_conn->CreateDamage(actor->texture_pixmap_window(),
                                       XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);
  actor->SetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA,
                        TidyInterface::DrawingDataPtr(data.release()));
  actor->set_dirty();
  return true;
}

OpenGlTextureData::OpenGlTextureData(GLInterface* gl_interface)
    : gl_interface_(gl_interface),
      texture_(0),
      has_alpha_(false) {}

OpenGlTextureData::~OpenGlTextureData() {
  if (texture_) {
    gl_interface_->DeleteTextures(1, &texture_);
  }
}

void OpenGlTextureData::SetTexture(GLuint texture, bool has_alpha) {
  if (texture_ && texture_ != texture) {
    gl_interface_->DeleteTextures(1, &texture_);
  }
  texture_ = texture;
  has_alpha_ = has_alpha;
}

OpenGlDrawVisitor::OpenGlDrawVisitor(GLInterfaceBase* gl_interface,
                                     TidyInterface* interface,
                                     ClutterInterface::StageActor* stage)
    : gl_interface_(dynamic_cast<GLInterface*>(gl_interface)),
      interface_(interface),
      x_conn_(interface->x_conn()),
      config_24_(0),
      config_32_(0),
      context_(0),
      num_frames_drawn_(0) {
  CHECK(gl_interface_);
  context_ = gl_interface_->CreateGlxContext();
  CHECK(context_) << "Unable to create a context from the available visuals.";

  gl_interface_->MakeGlxCurrent(stage->GetStageXWindow(), context_);

  int num_fb_configs;
  GLXFBConfig* fb_configs = gl_interface_->GetGlxFbConfigs(&num_fb_configs);
  bool rgba = false;
  for (int i = 0; i < num_fb_configs; ++i) {
    XVisualInfo* visual_info =
        gl_interface_->GetGlxVisualFromFbConfig(fb_configs[i]);
    if (!visual_info)
      continue;

    int visual_depth = visual_info->depth;
    gl_interface_->GlxFree(visual_info);
    if (visual_depth != 32 && visual_depth != 24)
      continue;

    int alpha = 0;
    int buffer_size = 0;
    gl_interface_->GetGlxFbConfigAttrib(fb_configs[i], GLX_ALPHA_SIZE, &alpha);
    gl_interface_->GetGlxFbConfigAttrib(fb_configs[i], GLX_BUFFER_SIZE,
                                        &buffer_size);
    if (buffer_size != visual_depth && (buffer_size - alpha) != visual_depth)
      continue;

    int has_rgba = 0;
    if (visual_depth == 32) {
      gl_interface_->GetGlxFbConfigAttrib(fb_configs[i],
                                          GLX_BIND_TO_TEXTURE_RGBA_EXT,
                                          &has_rgba);
      if (has_rgba)
        rgba = true;
    }

    if (!has_rgba) {
      if (rgba)
        continue;

      int has_rgb = 0;
      gl_interface_->GetGlxFbConfigAttrib(fb_configs[i],
                                          GLX_BIND_TO_TEXTURE_RGB_EXT,
                                          &has_rgb);
      if (!has_rgb)
        continue;
    }
    if (visual_depth == 32) {
      config_32_ = fb_configs[i];
    } else {
      config_24_ = fb_configs[i];
    }
  }
  gl_interface_->GlxFree(fb_configs);

  CHECK(config_24_ || config_32_)
      << "Unable to obtain a framebuffer configuration with appropriate depth.";

  gl_interface_->Enable(GL_DEPTH_TEST);
  gl_interface_->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  quad_drawing_data_.reset(new OpenGlQuadDrawingData(gl_interface_));
}

OpenGlDrawVisitor::~OpenGlDrawVisitor() {
  gl_interface_->Finish();
  // Make sure the vertex buffer is deleted.
  quad_drawing_data_ = TidyInterface::DrawingDataPtr();
  CHECK_GL_ERROR();
  gl_interface_->MakeGlxCurrent(0, 0);
  if (context_) {
    gl_interface_->DestroyGlxContext(context_);
  }
}

void OpenGlDrawVisitor::BindImage(const ImageContainer* container,
                                  TidyInterface::QuadActor* actor) {
  // Create an OpenGL texture with the loaded image data.
  GLuint new_texture;
  gl_interface_->Enable(GL_TEXTURE_2D);
  gl_interface_->GenTextures(1, &new_texture);
  gl_interface_->BindTexture(GL_TEXTURE_2D, new_texture);
  gl_interface_->TexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
  gl_interface_->TexParameterf(GL_TEXTURE_2D,
                               GL_TEXTURE_MIN_FILTER,
                               GL_LINEAR);
  gl_interface_->TexParameterf(GL_TEXTURE_2D,
                               GL_TEXTURE_MAG_FILTER,
                               GL_LINEAR);
  gl_interface_->TexParameterf(GL_TEXTURE_2D,
                               GL_TEXTURE_WRAP_S,
                               GL_CLAMP_TO_EDGE);
  gl_interface_->TexParameterf(GL_TEXTURE_2D,
                               GL_TEXTURE_WRAP_T,
                               GL_CLAMP_TO_EDGE);
  gl_interface_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                            container->width(), container->height(),
                            0, GL_RGBA, GL_UNSIGNED_BYTE,
                            container->data());
  OpenGlTextureData* data = new OpenGlTextureData(gl_interface_);
  // TODO: once ImageContainer supports non-alpha images, calculate
  // whether or not this texture has alpha (instead of just passing
  // 'true').
  data->SetTexture(new_texture, true);
  actor->SetSize(container->width(), container->height());
  actor->SetDrawingData(OpenGlDrawVisitor::TEXTURE_DATA,
                        TidyInterface::DrawingDataPtr(data));
  LOG(INFO) << "Binding image " << container->filename()
            << " to texture " << new_texture;
}

void OpenGlDrawVisitor::VisitActor(TidyInterface::Actor* actor) {
  // Base actors actually don't have anything to draw.
}

void OpenGlDrawVisitor::VisitTexturePixmap(
    TidyInterface::TexturePixmapActor* actor) {
  if (!actor->IsVisible()) return;
  // Make sure there's a bound texture.
  if (!actor->GetDrawingData(PIXMAP_DATA).get()) {
    if (!OpenGlPixmapData::BindToPixmap(this, actor)) {
      // We didn't find a bound pixmap, so let's just skip drawing this
      // actor.  (it's probably because it hasn't been mapped).
      return;
    }
  }

  // All texture pixmaps are also QuadActors, and so we let the
  // QuadActor do all the actual drawing.
  VisitQuad(actor);
}

void OpenGlDrawVisitor::VisitQuad(TidyInterface::QuadActor* actor) {
  if (!actor->IsVisible()) return;
#ifdef EXTRA_LOGGING
  LOG(INFO) << "Drawing quad " << actor->name() << ".";
#endif
  OpenGlQuadDrawingData* draw_data = dynamic_cast<OpenGlQuadDrawingData*>(
      actor->GetDrawingData(DRAWING_DATA).get());

  if (!draw_data) {
    // This actor hasn't been here before, so let's set the drawing
    // data on it.
    actor->SetDrawingData(DRAWING_DATA, quad_drawing_data_);
    draw_data = dynamic_cast<OpenGlQuadDrawingData*>(quad_drawing_data_.get());
  }

  gl_interface_->BindBuffer(GL_ARRAY_BUFFER, draw_data->vertex_buffer());

  gl_interface_->Color4f(actor->color().red,
                         actor->color().green,
                         actor->color().blue,
                         actor->opacity() * ancestor_opacity_);

  // Find out if this quad has pixmap or texture data to bind.
  OpenGlPixmapData* pixmap_data = dynamic_cast<OpenGlPixmapData*>(
      actor->GetDrawingData(PIXMAP_DATA).get());
  if (pixmap_data && pixmap_data->texture()) {
    // Actor has a pixmap texture to bind.
    gl_interface_->Enable(GL_TEXTURE_2D);
    gl_interface_->BindTexture(GL_TEXTURE_2D, pixmap_data->texture());
  } else {
    OpenGlTextureData* texture_data = dynamic_cast<OpenGlTextureData*>(
        actor->GetDrawingData(TEXTURE_DATA).get());
    if (texture_data && texture_data->texture()) {
      // Actor has a texture to bind.
      gl_interface_->Enable(GL_TEXTURE_2D);
      gl_interface_->BindTexture(GL_TEXTURE_2D, texture_data->texture());
    } else {
      // Actor has no texture.
      gl_interface_->Disable(GL_TEXTURE_2D);
    }
  }
  gl_interface_->PushMatrix();
  gl_interface_->Translatef(actor->x(), actor->y(), actor->z());
  gl_interface_->Scalef(actor->width() * actor->scale_x(),
                        actor->height() * actor->scale_y(), 1.f);
#ifdef EXTRA_LOGGING
  LOG(INFO) << "  at: (" << actor->x() << ", "  << actor->y()
            << ", " << actor->z() << ") with scale: ("
            << actor->scale_x() << ", "  << actor->scale_y() << ") at size ("
            << actor->width() << "x"  << actor->height()
            << ") and opacity " << actor->opacity() * ancestor_opacity_;
#endif
  gl_interface_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_interface_->PopMatrix();
  CHECK_GL_ERROR();
}

void OpenGlDrawVisitor::DrawNeedle() {
  OpenGlQuadDrawingData* draw_data = dynamic_cast<OpenGlQuadDrawingData*>(
      quad_drawing_data_.get());
  gl_interface_->BindBuffer(GL_ARRAY_BUFFER, draw_data->vertex_buffer());
  gl_interface_->EnableClientState(GL_VERTEX_ARRAY);
  gl_interface_->VertexPointer(2, GL_FLOAT, 0, 0);
  gl_interface_->DisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_interface_->Disable(GL_TEXTURE_2D);
  gl_interface_->PushMatrix();
  gl_interface_->Disable(GL_DEPTH_TEST);
  gl_interface_->Translatef(30, 30, 0);
  gl_interface_->Rotatef(num_frames_drawn_, 0.f, 0.f, 1.f);
  gl_interface_->Scalef(30, 3, 1.f);
  gl_interface_->Color4f(1.f, 0.f, 0.f, 0.8f);
  gl_interface_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_interface_->Enable(GL_DEPTH_TEST);
  gl_interface_->PopMatrix();
}

void OpenGlDrawVisitor::VisitStage(TidyInterface::StageActor* actor) {
  if (!actor->IsVisible()) return;

  stage_ = actor;
  OpenGlQuadDrawingData* draw_data = dynamic_cast<OpenGlQuadDrawingData*>(
      quad_drawing_data_.get());

  gl_interface_->MatrixMode(GL_PROJECTION);
  gl_interface_->LoadIdentity();
  gl_interface_->Ortho(0, actor->width(), actor->height(), 0,
                       TidyInterface::LayerVisitor::kMinDepth,
                       TidyInterface::LayerVisitor::kMaxDepth);
  gl_interface_->MatrixMode(GL_MODELVIEW);
  gl_interface_->LoadIdentity();
  gl_interface_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl_interface_->BindBuffer(GL_ARRAY_BUFFER,
                            draw_data->vertex_buffer());
  gl_interface_->EnableClientState(GL_VERTEX_ARRAY);
  gl_interface_->VertexPointer(2, GL_FLOAT, 0, 0);
  gl_interface_->EnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_interface_->TexCoordPointer(2, GL_FLOAT, 0, 0);
  CHECK_GL_ERROR();

  // Set the z-depths for the actors, update is_opaque.
  TidyInterface::LayerVisitor layer_visitor(interface_->actor_count());
  actor->Accept(&layer_visitor);

#ifdef EXTRA_LOGGING
  LOG(INFO) << "Starting OPAQUE pass.";
#endif
  // Disable blending because these actors are all opaque, and we're
  // drawing them front to back.
  gl_interface_->Disable(GL_BLEND);

  // For the first pass, we want to collect only opaque actors, in
  // front to back order.
  visit_opaque_ = true;
  VisitContainer(actor);

#ifdef EXTRA_LOGGING
  LOG(INFO) << "Ending OPAQUE pass.";
  LOG(INFO) << "Starting TRANSPARENT pass.";
#endif
  ancestor_opacity_ = actor->opacity();
  gl_interface_->DepthMask(GL_FALSE);
  gl_interface_->Enable(GL_BLEND);

  // Visiting back to front now, with no z-buffer, but with blending.
  visit_opaque_ = false;
  VisitContainer(actor);
  gl_interface_->DepthMask(GL_TRUE);
  CHECK_GL_ERROR();

  if (FLAGS_tidy_display_debug_needle) {
    DrawNeedle();
  }
  gl_interface_->SwapGlxBuffers(actor->GetStageXWindow());
  ++num_frames_drawn_;
#ifdef EXTRA_LOGGING
  LOG(INFO) << "Ending TRANSPARENT pass.";
#endif
  stage_ = NULL;
}

void OpenGlDrawVisitor::VisitContainer(
    TidyInterface::ContainerActor* actor) {
  if (!actor->IsVisible()) {
    return;
  }

  if (actor != stage_) {
    gl_interface_->PushMatrix();
    // Don't translate by Z because the actors already have their
    // absolute Z values from the layer calculation.
    gl_interface_->Translatef(actor->x(), actor->y(), 0.0f);
    gl_interface_->Scalef(actor->width() * actor->scale_x(),
                          actor->height() * actor->scale_y(), 1.f);
  }

#ifdef EXTRA_LOGGING
  LOG(INFO) << "Drawing container " << actor->name() << ".";
  LOG(INFO) << "  at: (" << actor->x() << ", "  << actor->y()
            << ", " << actor->z() << ") with scale: ("
            << actor->scale_x() << ", "  << actor->scale_y() << ") at size ("
            << actor->width() << "x"  << actor->height() << ")";
#endif
  TidyInterface::ActorVector children = actor->GetChildren();
  if (visit_opaque_) {
    for (TidyInterface::ActorVector::const_iterator iterator = children.begin();
         iterator != children.end(); ++iterator) {
      TidyInterface::Actor* child =
          dynamic_cast<TidyInterface::Actor*>(*iterator);
      // Only traverse if the child is visible, and opaque.
      if (child->IsVisible() && child->is_opaque()) {
#ifdef EXTRA_LOGGING
        LOG(INFO) << "Drawing opaque child " << child->name()
                  << " (visible: " << child->IsVisible()
                  << ", is_opaque: " << child->is_opaque() << ")";
#endif
        (*iterator)->Accept(this);
      } else {
#ifdef EXTRA_LOGGING
        LOG(INFO) << "NOT drawing transparent child " << child->name()
                  << " (visible: " << child->IsVisible()
                  << ", is_opaque: " << child->is_opaque() << ")";
#endif
      }
      CHECK_GL_ERROR();
    }
  } else {
    float original_opacity = ancestor_opacity_;
    ancestor_opacity_ *= actor->opacity();

    // Walk backwards so we go back to front.
    TidyInterface::ActorVector::const_reverse_iterator iterator;
    for (iterator = children.rbegin(); iterator != children.rend();
         ++iterator) {
      TidyInterface::Actor* child =
          dynamic_cast<TidyInterface::Actor*>(*iterator);
      // Only traverse if child is visible, and either transparent or
      // has children that might be transparent.
      if (child->IsVisible() &&
          (ancestor_opacity_ <= 0.999 || child->has_children() ||
           !child->is_opaque())) {
#ifdef EXTRA_LOGGING
        LOG(INFO) << "Drawing transparent child " << child->name()
                  << " (visible: " << child->IsVisible()
                  << ", has_children: " << child->has_children()
                  << ", ancestor_opacity: " << ancestor_opacity_
                  << ", is_opaque: " << child->is_opaque() << ")";
#endif
        (*iterator)->Accept(this);
      } else {
#ifdef EXTRA_LOGGING
        LOG(INFO) << "NOT drawing opaque child " << child->name()
                  << " (visible: " << child->IsVisible()
                  << ", has_children: " << child->has_children()
                  << ", ancestor_opacity: " << ancestor_opacity_
                  << ", is_opaque: " << child->is_opaque() << ")";
#endif
      }
      CHECK_GL_ERROR();
    }

    // Reset ancestor opacity.
    ancestor_opacity_ = original_opacity;
  }

  if (actor != stage_) {
    gl_interface_->PopMatrix();
  }
}

}  // namespace window_manager
