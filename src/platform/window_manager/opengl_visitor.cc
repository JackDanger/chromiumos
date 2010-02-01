// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/opengl_visitor.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>
#include <gdk/gdkx.h>
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
#include "window_manager/util.h"

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

const float OpenGlLayerVisitor::kMinDepth = -2048.0f;
const float OpenGlLayerVisitor::kMaxDepth = 2048.0f;

void OpenGlLayerVisitor::VisitContainer(
    TidyInterface::ContainerActor* actor) {
  CHECK(actor);
  TidyInterface::ActorVector::const_iterator iterator =
      actor->children().begin();
  while (iterator != actor->children().end()) {
    if (*iterator) {
      (*iterator)->Accept(this);
    }
    ++iterator;
  }

  // The containers should be "closer" than all their children.
  this->VisitActor(actor);
}

void OpenGlLayerVisitor::VisitStage(TidyInterface::StageActor* actor) {
  // This calculates the next power of two for the actor count, so
  // that we can avoid roundoff errors when computing the depth.
  // Also, add two empty layers at the front and the back that we
  // won't use in order to avoid issues at the extremes.  The eventual
  // plan here is to have three depth ranges, one in the front that is
  // 4096 deep, one in the back that is 4096 deep, and the remaining
  // in the middle for drawing 3D UI elements.  Currently, this code
  // represents just the front layer range.  Note that the number of
  // layers is NOT limited to 4096 (this is an arbitrary value that is
  // a power of two) -- the maximum number of layers depends on the
  // number of actors and the bit-depth of the hardware's z-buffer.
  uint32 count = NextPowerOfTwo(static_cast<uint32>(count_ + 2));
  layer_thickness_ = -(kMaxDepth - kMinDepth) / count;

  // Don't start at the very edge of the z-buffer depth.
  depth_ = kMaxDepth + layer_thickness_;

  VisitContainer(actor);
}

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
      damage_(XCB_NONE) {}

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

void OpenGlPixmapData::set_texture(GLuint texture) {
  if (texture_ && texture_ != texture) {
    gl_interface_->DeleteTextures(1, &texture_);
  }
  texture_ = texture;
  Refresh();
}

bool OpenGlPixmapData::BindToPixmap(
    OpenGlDrawVisitor* visitor,
    TidyInterface::TexturePixmapActor* actor) {
  GLInterface* gl_interface = visitor->gl_interface_;
  XConnection* x_conn = visitor->x_conn_;

  CHECK(actor);
  CHECK(actor->texture_pixmap_window()) << "Missing window.";
  CHECK(!actor->GetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA))
      << "Pixmap data already exists.";

  OpenGlPixmapData* data = new OpenGlPixmapData(gl_interface, x_conn);

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
                          TidyInterface::DrawingDataPtr(data));
  actor->set_dirty();
  return true;
}

OpenGlTextureData::OpenGlTextureData(GLInterface* gl_interface)
    : gl_interface_(gl_interface),
      texture_(0) {}

OpenGlTextureData::~OpenGlTextureData() {
  if (texture_) {
    gl_interface_->DeleteTextures(1, &texture_);
  }
}

void OpenGlTextureData::set_texture(GLuint texture) {
  if (texture_ && texture_ != texture) {
    gl_interface_->DeleteTextures(1, &texture_);
  }
  texture_ = texture;
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

  XWindow root = x_conn_->GetRootWindow();
  XConnection::WindowAttributes attributes;
  x_conn_->GetWindowAttributes(root, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = attributes.visual_id;
  int visual_info_count = 0;
  XVisualInfo* visual_info_list =
      x_conn_->GetVisualInfo(VisualIDMask,
                            &visual_info_template,
                            &visual_info_count);
  CHECK(visual_info_list);
  CHECK(visual_info_count > 0);
  context_ = 0;
  for (int i = 0; i < visual_info_count; ++i) {
    context_ = gl_interface_->CreateGlxContext(visual_info_list + i);
    if (context_) {
      break;
    }
  }

  x_conn_->Free(visual_info_list);
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
  OpenGlTextureData* data = new OpenGlTextureData(gl_interface_);
  data->set_texture(new_texture);
  actor->SetDrawingData(OpenGlDrawVisitor::TEXTURE_DATA,
                          TidyInterface::DrawingDataPtr(data));
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
  LOG(INFO) << "Binding image " << container->filename()
            << " to texture " << new_texture;
}

void OpenGlDrawVisitor::VisitActor(TidyInterface::Actor* actor) {
  // Base actors actually don't have anything to draw.
}

void OpenGlDrawVisitor::VisitTexturePixmap(
    TidyInterface::TexturePixmapActor* actor) {
  // Make sure there's a bound texture.
  bool bound = true;
  if (!actor->GetDrawingData(PIXMAP_DATA).get())
    bound = OpenGlPixmapData::BindToPixmap(this, actor);
  CHECK(bound);

  // All texture pixmaps are also QuadActors, and so we let the
  // QuadActor do all the actual drawing.
  VisitQuad(actor);
}

void OpenGlDrawVisitor::VisitQuad(TidyInterface::QuadActor* actor) {
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
                         actor->opacity());

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
  gl_interface_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_interface_->PopMatrix();
  CHECK_GL_ERROR();
}

void OpenGlDrawVisitor::DrawNeedle() {
  OpenGlQuadDrawingData* draw_data = dynamic_cast<OpenGlQuadDrawingData*>(
      quad_drawing_data_.get());
  gl_interface_->BindBuffer(GL_ARRAY_BUFFER, draw_data->vertex_buffer());
  gl_interface_->EnableClientState(GL_VERTEX_ARRAY);
  gl_interface_->VertexPointer(2, GL_FLOAT, 0, NULL);
  gl_interface_->DisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_interface_->Disable(GL_TEXTURE_2D);
  gl_interface_->PushMatrix();
  gl_interface_->Disable(GL_DEPTH_TEST);
  gl_interface_->Translatef(30, 30, 0);
  gl_interface_->Rotatef(num_frames_drawn_, 0.f, 0.f, 1.f);
  gl_interface_->Scalef(30, 3, 1.f);
  gl_interface_->Color4f(1.f, 0.f, 0.f, 1.f);
  gl_interface_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_interface_->Enable(GL_DEPTH_TEST);
  gl_interface_->PopMatrix();
}

static bool CompareFrontToBack(ClutterInterface::Actor* a,
                               ClutterInterface::Actor* b) {
  return dynamic_cast<TidyInterface::Actor*>(a)->z() <
      dynamic_cast<TidyInterface::Actor*>(b)->z();
}

static bool CompareBackToFront(ClutterInterface::Actor* a,
                               ClutterInterface::Actor* b) {
  return dynamic_cast<TidyInterface::Actor*>(a)->z() >
      dynamic_cast<TidyInterface::Actor*>(b)->z();
}

void OpenGlDrawVisitor::VisitStage(TidyInterface::StageActor* actor) {
  OpenGlQuadDrawingData* draw_data = dynamic_cast<OpenGlQuadDrawingData*>(
      quad_drawing_data_.get());

  gl_interface_->MatrixMode(GL_PROJECTION);
  gl_interface_->LoadIdentity();
  gl_interface_->Ortho(0, actor->width(), actor->height(), 0,
                       OpenGlLayerVisitor::kMinDepth,
                       OpenGlLayerVisitor::kMaxDepth);
  gl_interface_->MatrixMode(GL_MODELVIEW);
  gl_interface_->LoadIdentity();
  gl_interface_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  gl_interface_->BindBuffer(GL_ARRAY_BUFFER,
                            draw_data->vertex_buffer());
  gl_interface_->EnableClientState(GL_VERTEX_ARRAY);
  gl_interface_->VertexPointer(2, GL_FLOAT, 0, NULL);
  gl_interface_->EnableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_interface_->TexCoordPointer(2, GL_FLOAT, 0, NULL);
  CHECK_GL_ERROR();

  OpenGlLayerVisitor layer_visitor(interface_->actor_count());
  actor->Accept(&layer_visitor);
  TidyInterface::ActorCollector collector;
  collector.CollectVisible(TidyInterface::ActorCollector::VALUE_TRUE);
  collector.CollectOpaque(TidyInterface::ActorCollector::VALUE_TRUE);
  actor->Accept(&collector);
  TidyInterface::ActorVector actors = collector.results();
  if (!actors.empty()) {
    gl_interface_->Disable(GL_BLEND);
    std::sort(actors.begin(), actors.end(), CompareFrontToBack);
    for (TidyInterface::ActorVector::iterator iterator = actors.begin();
         iterator != actors.end(); ++iterator) {
      if ((*iterator) != actor)
        (*iterator)->Accept(this);
      CHECK_GL_ERROR();
    }
  }

  collector.clear();
  collector.CollectOpaque(TidyInterface::ActorCollector::VALUE_FALSE);
  actor->Accept(&collector);
  actors = collector.results();
  if (!actors.empty()) {
    gl_interface_->DepthMask(GL_FALSE);
    gl_interface_->Enable(GL_BLEND);
    std::sort(actors.begin(), actors.end(), CompareBackToFront);
    for (TidyInterface::ActorVector::iterator iterator = actors.begin();
         iterator != actors.end(); ++iterator) {
      if ((*iterator) != actor)
        (*iterator)->Accept(this);
      CHECK_GL_ERROR();
    }
    gl_interface_->DepthMask(GL_TRUE);
  }
  CHECK_GL_ERROR();

  DrawNeedle();
  gl_interface_->SwapGlxBuffers(actor->GetStageXWindow());
  ++num_frames_drawn_;
}

void OpenGlDrawVisitor::VisitContainer(
    TidyInterface::ContainerActor* actor) {
  // Do nothing, for now.
  // TODO: Implement group attribute propagation.  Right now, the
  // opacity and transform of the group isn't added to the state
  // anywhere.  We should be setting up the group's opacity and
  // transform as we traverse.
}

}  // namespace window_manager
