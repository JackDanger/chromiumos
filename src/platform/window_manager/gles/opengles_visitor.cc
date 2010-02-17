// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/gles/opengles_visitor.h"

#include <X11/Xlib.h>
#include <xcb/damage.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/logging.h"
#include "window_manager/gles/shaders.h"
#include "window_manager/gles/gles2_interface.h"
#include "window_manager/image_container.h"
#include "window_manager/real_x_connection.h"
#include "window_manager/x_connection.h"

#ifndef TIDY_OPENGLES
#error Need TIDY_OPENGLES defined to compile this file
#endif

// Work around broken eglext.h headers
#ifndef EGL_NO_IMAGE_KHR
#define EGL_NO_IMAGE_KHR (reinterpret_cast<EGLImageKHR>(0))
#endif
#ifndef EGL_IMAGE_PRESERVED_KHR
#define EGL_IMAGE_PRESERVED_KHR 0x30D2
#endif

namespace window_manager {

OpenGlesDrawVisitor::OpenGlesDrawVisitor(GLInterfaceBase* gl,
                                         TidyInterface* interface,
                                         ClutterInterface::StageActor* stage)
    : gl_(dynamic_cast<Gles2Interface*>(gl)),
      interface_(interface),
      stage_(stage),
      x_connection_(interface_->x_conn()) {
  CHECK(gl_);
  egl_display_ = gl_->egl_display();

  // TODO: We need to allocate a 32 bit color buffer, when all of the
  // platforms properly support it
  static const EGLint egl_config_attributes[] = {
    EGL_DEPTH_SIZE, 16,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
  };
  EGLConfig egl_config;
  EGLint num_configs = 0;
  CHECK(gl_->EglChooseConfig(egl_display_, egl_config_attributes, &egl_config,
                             1, &num_configs) == EGL_TRUE)
      << "eglChooseConfig() failed: " << eglGetError();
  CHECK(num_configs == 1) << "Couldn't find EGL config.";

  egl_surface_ = gl_->EglCreateWindowSurface(
      egl_display_, egl_config,
      static_cast<EGLNativeWindowType>(stage->GetStageXWindow()),
      NULL);
  CHECK(egl_surface_ != EGL_NO_SURFACE) << "Failed to create EGL window.";

  static const EGLint egl_context_attributes[] = {
    EGL_CONTEXT_CLIENT_VERSION, 2,
    EGL_NONE
  };
  egl_context_ = gl_->EglCreateContext(egl_display_, egl_config, EGL_FALSE,
                                       egl_context_attributes);
  CHECK(egl_context_ != EGL_NO_CONTEXT) << "Failed to create EGL context.";

  CHECK(gl_->EglMakeCurrent(egl_display_, egl_surface_, egl_surface_,
                            egl_context_))
      << "eglMakeCurrent() failed: " << eglGetError();

  CHECK(gl_->InitExtensions()) << "Failed to load EGL/GL-ES extensions.";

  // Allocate shaders
  tex_color_shader_ = new TexColorShader();
  gl_->ReleaseShaderCompiler();

  // TODO: Move away from one global Vertex Buffer Object
  gl_->GenBuffers(1, &vertex_buffer_object_);
  CHECK(vertex_buffer_object_ > 0) << "VBO allocation failed.";
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object_);
  static float kQuad[] = {
    0.f, 0.f,
    0.f, 1.f,
    1.f, 0.f,
    1.f, 1.f,
  };
  gl_->BufferData(GL_ARRAY_BUFFER, sizeof(kQuad), kQuad, GL_STATIC_DRAW);
}

OpenGlesDrawVisitor::~OpenGlesDrawVisitor() {
  delete tex_color_shader_;

  gl_->DeleteBuffers(1, &vertex_buffer_object_);

  LOG_IF(ERROR, gl_->EglMakeCurrent(egl_display_, EGL_NO_SURFACE,
                                    EGL_NO_SURFACE,
                                    EGL_NO_CONTEXT) != EGL_TRUE)
      << "eglMakeCurrent() failed: " << eglGetError();
  LOG_IF(ERROR, gl_->EglDestroySurface(egl_display_, egl_surface_) != EGL_TRUE)
      << "eglDestroySurface() failed: " << eglGetError();
  LOG_IF(ERROR, gl_->EglDestroyContext(egl_display_, egl_context_) != EGL_TRUE)
      << "eglDestroyCotnext() failed: " << eglGetError();
}

void OpenGlesDrawVisitor::BindImage(const ImageContainer* container,
                                    TidyInterface::QuadActor* actor) {
  GLuint texture;
  gl_->GenTextures(1, &texture);
  CHECK(texture > 0) << "Failed to allocated texture.";
  gl_->BindTexture(GL_TEXTURE_2D, texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                  container->width(), container->height(),
                  0, GL_RGBA, GL_UNSIGNED_BYTE, container->data());

  OpenGlesTextureData* data = new OpenGlesTextureData(gl_);
  data->SetTexture(texture, true);
  actor->SetDrawingData(kTextureData,
                        TidyInterface::DrawingDataPtr(data));
  LOG(INFO) << "Binding image " << container->filename()
            << " to texture " << texture;
}

void OpenGlesDrawVisitor::VisitStage(TidyInterface::StageActor* actor) {
  // TODO: We don't need to clear color, remove this when background
  // images work correctly.
  gl_->ClearColor(.86f, .2f, .44f, 1.f);
  gl_->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  perspective_ = Matrix4::orthographic(0, actor->width(), actor->height(), 0,
                                       TidyInterface::LayerVisitor::kMinDepth,
                                       TidyInterface::LayerVisitor::kMaxDepth);
  model_view_ = Matrix4::identity();

  // Set the z-depths for the actors.
  TidyInterface::LayerVisitor layer_visitor(interface_->actor_count());
  actor->Accept(&layer_visitor);

  // Bind shader
  // TODO: Implement VertexAttribArray tracking in the shader objects.
  gl_->UseProgram(tex_color_shader_->program());
  gl_->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_object_);
  gl_->VertexAttribPointer(tex_color_shader_->PosLocation(),
                           2, GL_FLOAT, GL_FALSE, 0, 0);
  gl_->EnableVertexAttribArray(tex_color_shader_->PosLocation());
  gl_->VertexAttribPointer(tex_color_shader_->TexInLocation(),
                        2, GL_FLOAT, GL_FALSE, 0, 0);
  gl_->EnableVertexAttribArray(tex_color_shader_->TexInLocation());

  ancestor_opacity_ = actor->opacity();

  // Back to front rendering
  // TODO: Switch to two pass Z-buffered rendering
  gl_->Enable(GL_BLEND);

  // Back to front rendering
  const TidyInterface::ActorVector children = actor->GetChildren();
  for (TidyInterface::ActorVector::const_reverse_iterator i =
       children.rbegin(); i != children.rend(); ++i) {
    (*i)->Accept(this);
  }

  gl_->EglSwapBuffers(egl_display_, egl_surface_);
}

void OpenGlesDrawVisitor::VisitTexturePixmap(
    TidyInterface::TexturePixmapActor* actor) {
  OpenGlesEglImageData* image_data = dynamic_cast<OpenGlesEglImageData*>(
      actor->GetDrawingData(kEglImageData).get());

  if (!image_data) {
    image_data = new OpenGlesEglImageData(x_connection_, gl_);
    actor->SetDrawingData(kEglImageData,
                          TidyInterface::DrawingDataPtr(image_data));
  }

  if (!image_data->bound()) {
    if (image_data->Bind(actor, egl_context_)) {
      OpenGlesTextureData* texture = new OpenGlesTextureData(gl_);
      image_data->BindTexture(texture);
      actor->SetDrawingData(kTextureData,
                            TidyInterface::DrawingDataPtr(texture));
      VisitQuad(actor);
    }
  } else {
    VisitQuad(actor);
  }
}

void OpenGlesDrawVisitor::VisitQuad(TidyInterface::QuadActor* actor) {
  // color
  gl_->Uniform4f(tex_color_shader_->ColorLocation(), actor->color().red,
                 actor->color().green, actor->color().blue,
                 actor->opacity() * ancestor_opacity_);

  // texture
  OpenGlesTextureData* texture_data = reinterpret_cast<OpenGlesTextureData*>(
      actor->GetDrawingData(kTextureData).get());
  gl_->BindTexture(GL_TEXTURE_2D, texture_data ? texture_data->texture() : 0);
  gl_->Uniform1i(tex_color_shader_->SamplerLocation(), 0);

  // mvp matrix
  Matrix4 new_model_view = model_view_;
  new_model_view *= Matrix4::translation(Vector3(actor->x(), actor->y(),
                                                 actor->z()));
  new_model_view *= Matrix4::scale(Vector3(actor->width() * actor->scale_x(),
                                           actor->height() * actor->scale_y(),
                                           1.f));
  // Matrix4 mvp = new_model_view * perspective_;
  Matrix4 mvp = perspective_ * new_model_view;
  gl_->UniformMatrix4fv(tex_color_shader_->MvpLocation(), 1, GL_FALSE,
                        &mvp[0][0]);

  gl_->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void OpenGlesDrawVisitor::VisitContainer(
    TidyInterface::ContainerActor* actor) {
  LOG(INFO) << "Visit container: " << actor->name();
  Matrix4 old_model_view = model_view_;
  model_view_ *= Matrix4::translation(Vector3(actor->x(), actor->y(),
                                              actor->z()));
  model_view_ *= Matrix4::scale(Vector3(actor->width() * actor->scale_x(),
                                        actor->height() * actor->scale_y(),
                                        1.f));

  const float original_opacity = ancestor_opacity_;
  ancestor_opacity_ *= actor->opacity();

  // Back to front rendering
  const TidyInterface::ActorVector children = actor->GetChildren();
  for (TidyInterface::ActorVector::const_reverse_iterator i =
       children.rbegin(); i != children.rend(); ++i) {
    (*i)->Accept(this);
  }

  // Reset opacity.
  ancestor_opacity_ = original_opacity;
  // Pop matrix.
  model_view_ = old_model_view;
}

OpenGlesTextureData::OpenGlesTextureData(Gles2Interface* gl)
    : gl_(gl),
      texture_(0),
      has_alpha_(false) {}

OpenGlesTextureData::~OpenGlesTextureData() {
  gl_->DeleteTextures(1, &texture_);
}

void OpenGlesTextureData::SetTexture(GLuint texture, bool has_alpha) {
  gl_->DeleteTextures(1, &texture_);
  texture_ = texture;
  has_alpha_ = has_alpha;
}

OpenGlesEglImageData::OpenGlesEglImageData(XConnection* x,
                                           Gles2Interface* gl)
    : bound_(false),
      x_(x),
      gl_(gl),
      damage_(XCB_NONE),
      pixmap_(XCB_NONE),
      egl_image_(EGL_NO_IMAGE_KHR) {
}

OpenGlesEglImageData::~OpenGlesEglImageData() {
  if (damage_)
    x_->DestroyDamage(damage_);
  if (egl_image_)
    gl_->EglDestroyImageKHR(gl_->egl_display(), egl_image_);
  if (pixmap_)
    x_->FreePixmap(pixmap_);
}

bool OpenGlesEglImageData::Bind(TidyInterface::TexturePixmapActor* actor,
                                EGLContext egl_context) {
  CHECK(!bound_);

  const XID window = actor->texture_pixmap_window();
  if (!window) {
    // Unmapped window, nothing to bind to
    return false;
  }

  pixmap_ = x_->GetCompositingPixmapForWindow(window);
  if (pixmap_ == XCB_NONE) {
    LOG(INFO) << "GetCompositingPixmapForWindow() returned NONE.";
    return false;
  }

  static const EGLint egl_image_attribs[] = {
    EGL_IMAGE_PRESERVED_KHR, EGL_TRUE,
    EGL_NONE
  };
  // Work around broken eglCreateImageKHR that improperly takes a context
  // TODO: add #define configuration of this workaround, it breaks
  // platforms which follow the spec
  egl_image_ = gl_->EglCreateImageKHR(
      gl_->egl_display(), egl_context, EGL_NATIVE_PIXMAP_KHR,
      reinterpret_cast<EGLClientBuffer>(pixmap_), egl_image_attribs);
  if (egl_image_ == EGL_NO_IMAGE_KHR) {
    LOG(INFO) << "eglCreateImageKHR() returned EGL_NO_IMAGE_KHR.";
    return false;
  }

  damage_ = x_->CreateDamage(window, XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);
  if (damage_ == XCB_NONE) {
    LOG(INFO) << "CreateDamage() returned NONE.";
    return false;
  }

  bound_ = true;
  return true;
}

void OpenGlesEglImageData::BindTexture(OpenGlesTextureData* texture_data) {
  CHECK(bound_);

  GLuint texture;
  gl_->GenTextures(1, &texture);
  CHECK(texture > 0) << "Failed to allocated texture.";
  gl_->BindTexture(GL_TEXTURE_2D, texture);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl_->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl_->EGLImageTargetTexture2DOES(GL_TEXTURE_2D,
                               static_cast<GLeglImageOES>(egl_image_));

  XConnection::WindowGeometry geometry;
  x_->GetWindowGeometry(pixmap_, &geometry);
  bool has_alpha = (geometry.depth == 32);

  texture_data->SetTexture(texture, has_alpha);
}

void OpenGlesEglImageData::Refresh() {
  if (damage_)
    x_->SubtractRegionFromDamage(damage_, XCB_NONE, XCB_NONE);
}

}  // namespace window_manager

