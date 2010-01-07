// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/no_clutter.h"

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

using std::tr1::shared_ptr;

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

NoClutterInterface::AnimationBase::AnimationBase(AnimationTime start_time,
                                                 AnimationTime end_time)
    : start_time_(start_time),
      end_time_(end_time),
      ease_factor_(M_PI / (end_time - start_time)) {
}

NoClutterInterface::FloatAnimation::FloatAnimation(float* field,
                                                   float end_value,
                                                   AnimationTime start_time,
                                                   AnimationTime end_time)
    : NoClutterInterface::AnimationBase(start_time, end_time),
      field_(field),
      start_value_(*field),
      end_value_(end_value) {
}

bool NoClutterInterface::FloatAnimation::Eval(AnimationTime current_time) {
  if (current_time >= end_time_) {
    *field_ = end_value_;
    return true;
  }
  float x = (1.0f - cosf(ease_factor_ * (current_time - start_time_))) / 2.0f;
  *field_ = start_value_ + x * (end_value_ - start_value_);
  return false;
}

NoClutterInterface::IntAnimation::IntAnimation(int* field,
                                               int end_value,
                                               AnimationTime start_time,
                                               AnimationTime end_time)
    : NoClutterInterface::AnimationBase(start_time, end_time),
      field_(field),
      start_value_(*field),
      end_value_(end_value) {
}

bool NoClutterInterface::IntAnimation::Eval(AnimationTime current_time) {
  if (current_time >= end_time_) {
    *field_ = end_value_;
    return true;
  }
  float x = (1.0f - cosf(ease_factor_ * (current_time - start_time_))) / 2.0f;
  *field_ = start_value_ + x * (end_value_ - start_value_);
  return false;
}

NoClutterInterface::Actor::~Actor() {
  if (parent_) {
    parent_->RemoveActor(this);
  }
  interface_->RemoveActor(this);
}

NoClutterInterface::Actor::Actor(NoClutterInterface* interface)
    : interface_(interface),
      parent_(NULL),
      x_(0),
      y_(0),
      width_(0),
      height_(0),
      z_(0.f),
      scale_x_(1.f),
      scale_y_(1.f),
      opacity_(1.f),
      visible_(true) {
  interface_->AddActor(this);
}

NoClutterInterface::Actor* NoClutterInterface::Actor::Clone() {
  // TODO: This doesn't do a "real" clone at all.  Determine if we
  // need it to or not, and either remove it or implement a real
  // clone.
  return new Actor(interface_);
}

void NoClutterInterface::Actor::Move(int x, int y, int duration_ms) {
  MoveX(x, duration_ms);
  MoveY(y, duration_ms);
}

void NoClutterInterface::Actor::MoveX(int x, int duration_ms) {
  AnimateInt(&x_, x, duration_ms);
}

void NoClutterInterface::Actor::MoveY(int y, int duration_ms) {
  AnimateInt(&y_, y, duration_ms);
}

void NoClutterInterface::Actor::Scale(double scale_x, double scale_y,
                                      int duration_ms) {
  AnimateFloat(&scale_x_, static_cast<float>(scale_x), duration_ms);
  AnimateFloat(&scale_y_, static_cast<float>(scale_y), duration_ms);
}

void NoClutterInterface::Actor::SetOpacity(double opacity, int duration_ms) {
  AnimateFloat(&opacity_, static_cast<float>(opacity), duration_ms);
}

void NoClutterInterface::Actor::Raise(ClutterInterface::Actor* other) {
  CHECK(parent_) << "Tried to raise an actor that has no parent.";
  NoClutterInterface::Actor* other_nc =
      dynamic_cast<NoClutterInterface::Actor*>(other);
  CHECK(other_nc) << "Failed to cast to an Actor in Raise";
  parent_->RaiseChild(this, other_nc);
  set_dirty();
}

void NoClutterInterface::Actor::Lower(ClutterInterface::Actor* other) {
  CHECK(parent_) << "Tried to lower an actor that has no parent.";
  NoClutterInterface::Actor* other_nc =
      dynamic_cast<NoClutterInterface::Actor*>(other);
  CHECK(other_nc) << "Failed to cast to an Actor in Raise";
  parent_->LowerChild(this, other_nc);
  set_dirty();
}

void NoClutterInterface::Actor::RaiseToTop() {
  CHECK(parent_) << "Tried to raise an actor to top that has no parent.";
  parent_->RaiseChild(this, NULL);
  set_dirty();
}

void NoClutterInterface::Actor::LowerToBottom() {
  CHECK(parent_) << "Tried to lower an actor to bottom that has no parent.";
  parent_->LowerChild(this, NULL);
  set_dirty();
}

void NoClutterInterface::Actor::Update(float* depth,
                                       AnimationBase::AnimationTime now) {
  set_z(*depth);
  *depth += 1.0f;
  AnimationList::iterator iterator = animations_.begin();
  if (!animations_.empty()) set_dirty();
  while (iterator != animations_.end()) {
    bool done = (*iterator)->Eval(now);
    if (done) {
      iterator = animations_.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

void NoClutterInterface::Actor::AddToDisplayList(
    NoClutterInterface::ActorVector* actors,
    bool opaque) {
  if (!IsVisible())
    return;
  AddToDisplayListImpl(actors, opaque);
}

void NoClutterInterface::Actor::AnimateFloat(float* field, float value,
                                             int duration_ms) {
  if (duration_ms > 0) {
    AnimationBase::AnimationTime now = interface_->GetCurrentTime();
    shared_ptr<AnimationBase> animation(
        new FloatAnimation(field, value, now, now + duration_ms));
    animations_.push_back(animation);
  } else {
    *field = value;
    set_dirty();
  }
}

void NoClutterInterface::Actor::AnimateInt(int* field, int value,
                                           int duration_ms) {
  if (duration_ms > 0) {
    AnimationBase::AnimationTime now = interface_->GetCurrentTime();
    shared_ptr<AnimationBase> animation(
        new IntAnimation(field, value, now, now + duration_ms));
    animations_.push_back(animation);
  } else {
    *field = value;
    set_dirty();
  }
}

void NoClutterInterface::ContainerActor::AddActor(
    ClutterInterface::Actor* actor) {
  NoClutterInterface::Actor* cast_actor = dynamic_cast<Actor*>(actor);
  CHECK(cast_actor) << "Unable to down-cast actor.";
  cast_actor->set_parent(this);
  children_.push_back(cast_actor);
  set_dirty();
}

// Note that the passed-in Actors might be partially destroyed (the
// Actor destructor calls RemoveActor on its parent), so we shouldn't
// rely on the contents of the Actor.
void NoClutterInterface::ContainerActor::RemoveActor(
    ClutterInterface::Actor* actor) {
  ActorVector::iterator iterator = std::find(children_.begin(), children_.end(),
                                           actor);
  if (iterator != children_.end()) {
    children_.erase(iterator);
    set_dirty();
  }
}

void NoClutterInterface::ContainerActor::Update(
    float* depth, AnimationBase::AnimationTime now) {
  for (ActorVector::iterator iterator = children_.begin();
       iterator != children_.end(); ++iterator) {
    (*iterator)->Update(depth, now);
  }
  NoClutterInterface::Actor::Update(depth, now);
}

void NoClutterInterface::ContainerActor::AddToDisplayListImpl(
    ActorVector* actors, bool opaque) {
  for (ActorVector::iterator iterator = children_.begin();
       iterator != children_.end(); ++iterator) {
    (*iterator)->AddToDisplayList(actors, opaque);
  }
}

void NoClutterInterface::ContainerActor::RaiseChild(
    NoClutterInterface::Actor* child, NoClutterInterface::Actor* above) {
  CHECK(child) << "Tried to raise a NULL child.";
  if (child == above) {
    // Do nothing if we're raising a child above itself.
    return;
  }
  ActorVector::iterator iterator =
      std::find(children_.begin(), children_.end(), child);
  if (iterator == children_.end()) {
    LOG(WARNING) << "Attempted to raise a child (" << child
                 << ") that isn't a child of this container (" << this << ")";
    return;
  }
  if (above) {
    // Check and make sure 'above' is an existing child.
    ActorVector::iterator iterator_above =
        std::find(children_.begin(), children_.end(), above);
    if (iterator_above == children_.end()) {
      LOG(WARNING) << "Attempted to raise a child (" << child
                   << ") above a sibling (" << above << ") that isn't "
                   << "a child of this container (" << this << ").";
      return;
    }
    CHECK(iterator_above != iterator);
    if (iterator_above > iterator) {
      children_.erase(iterator);
      // Find the above child again after erasing, because the old
      // iterator is invalid.
      iterator_above = std::find(children_.begin(), children_.end(), above);
    } else {
      children_.erase(iterator);
    }
    // Re-insert the child.
    children_.insert(iterator_above, child);
  } else {  // above is NULL, move child to top.
    children_.erase(iterator);
    children_.insert(children_.begin(), child);
  }
}

void NoClutterInterface::ContainerActor::LowerChild(
    NoClutterInterface::Actor* child, NoClutterInterface::Actor* below) {
  CHECK(child) << "Tried to raise a NULL child.";
  if (child == below) {
    // Do nothing if we're lowering a child below itself,
    // or it's NULL.
    return;
  }
  ActorVector::iterator iterator =
      std::find(children_.begin(), children_.end(), child);
  if (iterator == children_.end()) {
    LOG(WARNING) << "Attempted to lower a child (" << child
                 << ") that isn't a child of this container (" << this << ")";
    return;
  }
  if (below) {
    // Check and make sure 'below' is an existing child.
    ActorVector::iterator iterator_below =
        std::find(children_.begin(), children_.end(), below);
    if (iterator_below == children_.end()) {
      LOG(WARNING) << "Attempted to lower a child (" << child
                   << ") below a sibling (" << below << ") that isn't "
                   << "a child of this container (" << this << ").";
      return;
    }
    CHECK(iterator_below != iterator);
    if (iterator_below > iterator) {
      children_.erase(iterator);
      // Find the below child again after erasing, because the old
      // iterator is invalid.
      iterator_below = std::find(children_.begin(), children_.end(), below);
    } else {
      children_.erase(iterator);
    }
    ++iterator_below;
    // Re-insert the child.
    children_.insert(iterator_below, child);
  } else {  // below is NULL, move child to bottom.
    children_.erase(iterator);
    children_.push_back(child);
  }
}


NoClutterInterface::QuadActor::QuadActor(NoClutterInterface* interface)
    : NoClutterInterface::Actor(interface),
      texture_(0),
      r_(1.f),
      g_(1.f),
      b_(1.f) {
}

void NoClutterInterface::QuadActor::AddToDisplayListImpl(ActorVector* actors,
                                                         bool opaque) {
  if (opaque == is_opaque())
    actors->push_back(this);
}

// TODO: Implement group attribute propagation.  Right now, the
// opacity and transform of the group isn't added to the state
// anywhere.  We should be setting up the group's opacity and
// transform as we traverse (either in AddToDisplayList, or in another
// traversal pass).
void NoClutterInterface::QuadActor::Draw() {
  interface()->gl_interface()->Color4f(r_, g_, b_, opacity());
  if (texture_) {
    interface()->gl_interface()->Enable(GL_TEXTURE_2D);
    interface()->gl_interface()->BindTexture(GL_TEXTURE_2D, texture_);
  } else {
    interface()->gl_interface()->Disable(GL_TEXTURE_2D);
  }
  interface()->gl_interface()->PushMatrix();
  interface()->gl_interface()->Translatef(x(), y(), z());
  interface()->gl_interface()->Scalef(width() * scale_x(),
                                      height() * scale_y(), 1.f);
  interface()->gl_interface()->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  interface()->gl_interface()->PopMatrix();
  CHECK_GL_ERROR();
}

NoClutterInterface::TexturePixmapActor::TexturePixmapActor(
    NoClutterInterface* interface)
    : NoClutterInterface::QuadActor(interface),
      window_(XCB_NONE),
      pixmap_(XCB_NONE),
      glx_pixmap_(XCB_NONE),
      damage_(XCB_NONE) {
}

bool NoClutterInterface::TexturePixmapActor::SetTexturePixmapWindow(
    XWindow xid) {
  Reset();
  window_ = xid;
  interface()->StartMonitoringWindowForChanges(window_, this);
  set_dirty();
  return true;
}

bool NoClutterInterface::TexturePixmapActor::Bind() {
  CHECK(!texture_) << "Missing texture in Bind.";
  CHECK(!pixmap_) << "Missing pixmap in Bind.";
  CHECK(!glx_pixmap_) << "Missing GLX pixmap in Bind.";
  CHECK(window_) << "Missing window in Bind.";

  pixmap_ = interface()->x_conn()->GetCompositingPixmapForWindow(window_);
  if (pixmap_ == XCB_NONE) {
    return false;
  }

  XConnection::WindowGeometry geometry;
  interface()->x_conn()->GetWindowGeometry(pixmap_, &geometry);
  int attribs[] = {
    GLX_TEXTURE_FORMAT_EXT,
    geometry.depth == 32 ?
      GLX_TEXTURE_FORMAT_RGBA_EXT :
      GLX_TEXTURE_FORMAT_RGB_EXT,
    GLX_TEXTURE_TARGET_EXT,
    GLX_TEXTURE_2D_EXT,
    0
  };
  GLXFBConfig config = (geometry.depth == 32) ?
                         interface()->config_32_ :
                         interface()->config_24_;
  glx_pixmap_ = interface()->gl_interface()->CreateGlxPixmap(config, pixmap_,
                                                             attribs);

  interface()->gl_interface()->GenTextures(1, &texture_);
  interface()->gl_interface()->BindTexture(GL_TEXTURE_2D, texture_);
  interface()->gl_interface()->TexParameteri(GL_TEXTURE_2D,
                                             GL_TEXTURE_MIN_FILTER,
                                             GL_NEAREST);
  interface()->gl_interface()->BindGlxTexImage(glx_pixmap_,
                                               GLX_FRONT_LEFT_EXT,
                                               NULL);
  damage_ = interface()->x_conn()->CreateDamage(
      window_, XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);
  set_dirty();
  return true;
}

void NoClutterInterface::TexturePixmapActor::Reset() {
  if (window_) {
    interface()->StopMonitoringWindowForChanges(window_, this);
  }
  if (damage_) {
    interface()->x_conn()->DestroyDamage(damage_);
    damage_ = XCB_NONE;
  }
  if (texture_) {
    interface()->gl_interface()->DeleteTextures(1, &texture_);
    texture_ = XCB_NONE;
  }
  if (glx_pixmap_) {
    interface()->gl_interface()->DestroyGlxPixmap(glx_pixmap_);
    glx_pixmap_ = XCB_NONE;
  }
  if (pixmap_) {
    interface()->x_conn()->FreePixmap(pixmap_);
    pixmap_ = XCB_NONE;
  }
}

void NoClutterInterface::TexturePixmapActor::Refresh() {
  LOG_IF(ERROR, !texture_ || !glx_pixmap_) << "Refreshing with no textures.";
  if (!texture_ || !glx_pixmap_)
    return;

  interface()->gl_interface()->BindTexture(GL_TEXTURE_2D, texture_);
  interface()->gl_interface()->ReleaseGlxTexImage(glx_pixmap_,
                                                  GLX_FRONT_LEFT_EXT);
  interface()->gl_interface()->BindGlxTexImage(glx_pixmap_,
                                               GLX_FRONT_LEFT_EXT, NULL);
  if (damage_) {
    interface()->x_conn()->SubtractRegionFromDamage(damage_, XCB_NONE,
                                                    XCB_NONE);
  }
  set_dirty();
}

void NoClutterInterface::TexturePixmapActor::Draw() {
  if (!texture_ && window_) {
    Bind();
  }
  if (!texture_)
    return;
  NoClutterInterface::QuadActor::Draw();
}

NoClutterInterface::StageActor::StageActor(NoClutterInterface* an_interface,
                                           int width, int height)
  : NoClutterInterface::ContainerActor(an_interface),
    stage_color_("#ffffff") {
  window_ = interface()->x_conn()->CreateSimpleWindow(
      interface()->x_conn()->GetRootWindow(),
      0, 0, width, height);
  interface()->x_conn()->MapWindow(window_);
}

NoClutterInterface::StageActor::~StageActor() {
  interface()->x_conn()->DestroyWindow(window_);
}

void NoClutterInterface::StageActor::SetStageColor(
    const std::string& color_str) {
  stage_color_ = color_str;
}

static bool CompareFrontToBack(NoClutterInterface::Actor* a,
                               NoClutterInterface::Actor* b) {
  return a->z() < b->z();
}

static bool CompareBackToFront(NoClutterInterface::Actor* a,
                               NoClutterInterface::Actor* b) {
  return a->z() > b->z();
}

void NoClutterInterface::StageActor::Draw() {
  interface()->gl_interface()->MatrixMode(GL_PROJECTION);
  interface()->gl_interface()->LoadIdentity();
  interface()->gl_interface()->Ortho(0, width(), height(), 0, -100, 100);
  interface()->gl_interface()->MatrixMode(GL_MODELVIEW);
  interface()->gl_interface()->LoadIdentity();
  ActorVector actors;

  interface()->gl_interface()->Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  interface()->gl_interface()->BindBuffer(GL_ARRAY_BUFFER,
                                          interface()->vertex_buffer());
  interface()->gl_interface()->EnableClientState(GL_VERTEX_ARRAY);
  interface()->gl_interface()->VertexPointer(2, GL_FLOAT, 0, NULL);
  interface()->gl_interface()->EnableClientState(GL_TEXTURE_COORD_ARRAY);
  interface()->gl_interface()->TexCoordPointer(2, GL_FLOAT, 0, NULL);
  CHECK_GL_ERROR();

  AddToDisplayList(&actors, true);
  if (!actors.empty()) {
    interface()->gl_interface()->Disable(GL_BLEND);
    std::sort(actors.begin(), actors.end(), CompareFrontToBack);
    for (ActorVector::iterator iterator = actors.begin();
         iterator != actors.end(); ++iterator) {
      (*iterator)->Draw();
      CHECK_GL_ERROR();
    }
  }

  actors.clear();
  AddToDisplayList(&actors, false);
  if (!actors.empty()) {
    interface()->gl_interface()->DepthMask(GL_FALSE);
    interface()->gl_interface()->Enable(GL_BLEND);
    std::sort(actors.begin(), actors.end(), CompareBackToFront);
    for (ActorVector::iterator iterator = actors.begin();
         iterator != actors.end(); ++iterator) {
      (*iterator)->Draw();
      CHECK_GL_ERROR();
    }
    interface()->gl_interface()->DepthMask(GL_TRUE);
  }
  CHECK_GL_ERROR();
}

void NoClutterInterface::StageActor::SetSizeImpl(int width, int height) {
  // Have to resize the window to match the stage.
  CHECK(window_) << "Missing window in StageActor::SetSizeImpl.";
  interface()->x_conn()->ResizeWindow(window_, width, height);
}

static gboolean DrawInterface(void* data) {
  reinterpret_cast<NoClutterInterface*>(data)->Draw();
  return TRUE;
}

NoClutterInterface::NoClutterInterface(XConnection* xconn,
                                       GLInterface* gl_interface)
    : dirty_(true),
      xconn_(xconn),
      gl_interface_(gl_interface),
      config_32_(0),
      config_24_(0),
      num_frames_drawn_(0),
      vertex_buffer_(0) {
  CHECK(xconn_);
  DCHECK(gl_interface_);
  now_ = GetCurrentRealTime();
  XWindow root = x_conn()->GetRootWindow();
  XConnection::WindowGeometry geometry;
  x_conn()->GetWindowGeometry(root, &geometry);
  default_stage_.reset(new NoClutterInterface::StageActor(this,
                                                          geometry.width,
                                                          geometry.height));
  default_stage_->SetSize(geometry.width, geometry.height);

  XConnection::WindowAttributes attributes;
  x_conn()->GetWindowAttributes(root, &attributes);
  XVisualInfo visual_info_template;
  visual_info_template.visualid = attributes.visual_id;
  int visual_info_count = 0;
  XVisualInfo* visual_info_list =
      x_conn()->GetVisualInfo(VisualIDMask,
                              &visual_info_template,
                              &visual_info_count);
  DCHECK(visual_info_list);
  DCHECK_GT(visual_info_count, 0);
  context_ = 0;
  for (int i = 0; i < visual_info_count; ++i) {
    context_ = gl_interface_->CreateGlxContext(visual_info_list + i);
    if (context_) {
      break;
    }
  }
  x_conn()->Free(visual_info_list);
  CHECK(context_) << "Unable to create a context from the available visuals.";
  gl_interface_->MakeGlxCurrent(default_stage_->GetStageXWindow(), context_);

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

  gl_interface_->GenBuffers(1, &vertex_buffer_);
  gl_interface_->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);

  static float kQuad[] = {
    0.f, 0.f,
    0.f, 1.f,
    1.f, 0.f,
    1.f, 1.f,
  };

  gl_interface_->BufferData(GL_ARRAY_BUFFER,
                            sizeof(kQuad),
                            kQuad,
                            GL_STATIC_DRAW);

  // TODO: Remove this lovely hack, and replace it with something that
  // knows more about keeping a consistent frame rate.
  g_timeout_add(20, window_manager::DrawInterface, this);
}

NoClutterInterface::~NoClutterInterface() {
  gl_interface()->DeleteBuffers(1, &vertex_buffer_);
  gl_interface()->Finish();
  CHECK_GL_ERROR();
  gl_interface()->MakeGlxCurrent(0, 0);
  if (context_) {
    gl_interface()->DestroyGlxContext(context_);
  }
}

NoClutterInterface::ContainerActor* NoClutterInterface::CreateGroup() {
  return new ContainerActor(this);
}

NoClutterInterface::Actor* NoClutterInterface::CreateRectangle(
    const std::string& color_str,
    const std::string& border_color_str,
    int border_width) {
  QuadActor* actor = new QuadActor(this);
  // TODO(piman): parse color
  actor->SetColor(0.f, 1.f, 0.f);
  return actor;
}

NoClutterInterface::Actor* NoClutterInterface::CreateImage(
    const std::string& filename) {
  QuadActor* actor = new QuadActor(this);
  // TODO(piman): load image, create texture set into actor
  actor->SetColor(1.f, 0.f, 0.f);
  return actor;
}

NoClutterInterface::TexturePixmapActor*
NoClutterInterface::CreateTexturePixmap() {
  return new TexturePixmapActor(this);
}

NoClutterInterface::Actor* NoClutterInterface::CreateText(
    const std::string& font_name,
    const std::string& text,
    const std::string& color_str) {
  QuadActor* actor = new QuadActor(this);
  // TODO(piman): create text.
  actor->SetColor(0.f, 0.f, 1.f);
  actor->SetOpacity(.5f, 0);
  return actor;
}

NoClutterInterface::Actor* NoClutterInterface::CloneActor(
    ClutterInterface::Actor* orig) {
  Actor* actor = dynamic_cast<Actor*>(orig);
  CHECK(actor);
  return actor->Clone();
}

void NoClutterInterface::RemoveActor(Actor* actor) {
  ActorVector::iterator iterator = std::find(actors_.begin(), actors_.end(),
                                           actor);
  if (iterator != actors_.end()) {
    actors_.erase(iterator);
  }
}

static GdkFilterReturn FilterEvent(GdkXEvent* xevent,
                                   GdkEvent* event,
                                   gpointer data) {
  NoClutterInterface* interface = static_cast<NoClutterInterface*>(data);
  return interface->HandleEvent(reinterpret_cast<XEvent*>(xevent)) ?
      GDK_FILTER_REMOVE : GDK_FILTER_CONTINUE;
}

#include "chromeos/utility.h"

bool NoClutterInterface::HandleEvent(XEvent* xevent) {
  if (xevent->type != DestroyNotify &&
      xevent->type != x_conn()->damage_event_base() + XDamageNotify) {
    return false;
  }
  XIDToTexturePixmapActorMap::iterator iterator =
      texture_pixmaps_.find(xevent->xany.window);
  if (iterator == texture_pixmaps_.end())
    return false;
  TexturePixmapActor* actor = iterator->second;
  if (!actor)
    return false;
  if (xevent->type == DestroyNotify) {
    actor->Reset();
    return false;  // Let the window manager continue to receive DestroyNotify.
  } else {  // This must be an XDamageNotify event.
    actor->Refresh();
    return true;
  }
}

void NoClutterInterface::StartMonitoringWindowForChanges(
    XWindow xid, TexturePixmapActor* actor) {
  if (texture_pixmaps_.empty()) {
    gdk_window_add_filter(NULL, FilterEvent, this);
  }

  texture_pixmaps_[xid] = actor;

  x_conn()->RedirectWindowForCompositing(xid);
}

void NoClutterInterface::StopMonitoringWindowForChanges(
    XWindow xid, TexturePixmapActor* actor) {
  x_conn()->UnredirectWindowForCompositing(xid);

  texture_pixmaps_.erase(xid);
  if (texture_pixmaps_.empty()) {
    gdk_window_remove_filter(NULL, FilterEvent, this);
  }
}

void NoClutterInterface::DrawNeedle() {
  gl_interface()->BindBuffer(GL_ARRAY_BUFFER, vertex_buffer_);
  gl_interface()->EnableClientState(GL_VERTEX_ARRAY);
  gl_interface()->VertexPointer(2, GL_FLOAT, 0, NULL);
  gl_interface()->DisableClientState(GL_TEXTURE_COORD_ARRAY);
  gl_interface()->Disable(GL_TEXTURE_2D);
  gl_interface()->PushMatrix();
  gl_interface()->Disable(GL_DEPTH_TEST);
  gl_interface()->Translatef(30, 30, 0);
  gl_interface()->Rotatef(num_frames_drawn_, 0.f, 0.f, 1.f);
  gl_interface()->Scalef(30, 3, 1.f);
  gl_interface()->Color4f(1.f, 0.f, 0.f, 1.f);
  gl_interface()->DrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_interface()->Enable(GL_DEPTH_TEST);
  gl_interface()->PopMatrix();
}

void NoClutterInterface::Draw() {
  now_ = GetCurrentRealTime();
  float depth = 1.0f;
  default_stage_->Update(&depth, now_);
  if (dirty_) {
    default_stage_->Draw();
    DrawNeedle();
    gl_interface()->SwapGlxBuffers(default_stage_->GetStageXWindow());
    ++num_frames_drawn_;
    dirty_ = false;
  }
}

NoClutterInterface::AnimationBase::AnimationTime
NoClutterInterface::GetCurrentRealTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000ULL * tv.tv_sec + tv.tv_usec / 1000ULL;
}

}  // namespace window_manager
