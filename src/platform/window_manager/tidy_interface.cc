// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/tidy_interface.h"

extern "C" {
#include <X11/extensions/Xdamage.h>
}

#include <gdk/gdkx.h>
#include <gflags/gflags.h>
#include <sys/time.h>
#include <time.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "window_manager/gl_interface_base.h"
#include "window_manager/image_container.h"
#ifdef TIDY_OPENGL
#include "window_manager/opengl_visitor.h"
#elif defined(TIDY_OPENGLES)
#include "window_manager/gles/opengles_visitor.h"
#endif
#include "window_manager/util.h"
#include "window_manager/x_connection.h"

using std::tr1::shared_ptr;

DEFINE_bool(tidy_display_debug_needle, false,
            "Specify this to turn on a debugging aid for seeing when "
            "frames are being drawn.");

// Turn this on if you want to debug the visitor traversal.
#undef EXTRA_LOGGING

namespace window_manager {

const float TidyInterface::LayerVisitor::kMinDepth = -2048.0f;
const float TidyInterface::LayerVisitor::kMaxDepth = 2048.0f;

TidyInterface::AnimationBase::AnimationBase(AnimationTime start_time,
                                            AnimationTime end_time)
    : start_time_(start_time),
      end_time_(end_time),
      ease_factor_(M_PI / (end_time - start_time)) {
}

TidyInterface::FloatAnimation::FloatAnimation(float* field,
                                              float end_value,
                                              AnimationTime start_time,
                                              AnimationTime end_time)
    : TidyInterface::AnimationBase(start_time, end_time),
      field_(field),
      start_value_(*field),
      end_value_(end_value) {
}

bool TidyInterface::FloatAnimation::Eval(AnimationTime current_time) {
  if (current_time >= end_time_) {
    *field_ = end_value_;
    return true;
  }
  float x = (1.0f - cosf(ease_factor_ * (current_time - start_time_))) / 2.0f;
  *field_ = start_value_ + x * (end_value_ - start_value_);
  return false;
}

TidyInterface::IntAnimation::IntAnimation(int* field,
                                          int end_value,
                                          AnimationTime start_time,
                                          AnimationTime end_time)
    : TidyInterface::AnimationBase(start_time, end_time),
      field_(field),
      start_value_(*field),
      end_value_(end_value) {
}

bool TidyInterface::IntAnimation::Eval(AnimationTime current_time) {
  if (current_time >= end_time_) {
    *field_ = end_value_;
    return true;
  }
  float x = (1.0f - cosf(ease_factor_ * (current_time - start_time_))) / 2.0f;
  *field_ = start_value_ + x * (end_value_ - start_value_);
  return false;
}

void TidyInterface::ActorVisitor::VisitContainer(ContainerActor* actor) {
  CHECK(actor);
  this->VisitActor(actor);
  ActorVector children = actor->GetChildren();
  ActorVector::const_iterator iterator = children.begin();
  while (iterator != children.end()) {
    if (*iterator) {
      (*iterator)->Accept(this);
    }
    ++iterator;
  }
}

void TidyInterface::LayerVisitor::VisitQuad(TidyInterface::QuadActor* actor) {
  // Do all the regular actor stuff.
  this->VisitActor(actor);

#ifdef TIDY_OPENGL
  OpenGlTextureData* data  = static_cast<OpenGlTextureData*>(
      actor->GetDrawingData(OpenGlDrawVisitor::TEXTURE_DATA).get());
  if (data) {
    actor->set_is_opaque(actor->is_opaque() && !data->has_alpha());
  }
#endif
}

void TidyInterface::LayerVisitor::VisitTexturePixmap(
    TidyInterface::TexturePixmapActor* actor) {
  // Do all the regular Quad stuff.
  this->VisitQuad(actor);

#ifdef TIDY_OPENGL
  OpenGlPixmapData* data  = static_cast<OpenGlPixmapData*>(
      actor->GetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA).get());
  if (data) {
    actor->set_is_opaque(actor->is_opaque() && !data->has_alpha());
  }
#endif
}

void TidyInterface::LayerVisitor::VisitActor(TidyInterface::Actor* actor) {
  actor->set_z(depth_);
  depth_ += layer_thickness_;
  actor->set_is_opaque(actor->opacity() > 0.999f);
}

void TidyInterface::LayerVisitor::VisitContainer(
    TidyInterface::ContainerActor* actor) {
  CHECK(actor);
  ActorVector children = actor->GetChildren();
  TidyInterface::ActorVector::const_iterator iterator = children.begin();
  while (iterator != children.end()) {
    if (*iterator) {
      (*iterator)->Accept(this);
    }
    ++iterator;
  }

  // The containers should be "closer" than all their children.
  this->VisitActor(actor);
}

void TidyInterface::LayerVisitor::VisitStage(TidyInterface::StageActor* actor) {
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

TidyInterface::Actor::~Actor() {
  if (parent_) {
    parent_->RemoveActor(this);
  }
  interface_->RemoveActor(this);
}

TidyInterface::Actor::Actor(TidyInterface* interface)
    : interface_(interface),
      parent_(NULL),
      x_(0),
      y_(0),
      width_(1),
      height_(1),
      z_(0.f),
      scale_x_(1.f),
      scale_y_(1.f),
      opacity_(1.f),
      is_opaque_(true),
      has_children_(false),
      visible_(true) {
  interface_->AddActor(this);
}

TidyInterface::Actor* TidyInterface::Actor::Clone() {
  Actor* new_instance = new Actor(interface_);
  CloneImpl(new_instance);
  return new_instance;
}

void TidyInterface::Actor::CloneImpl(TidyInterface::Actor* clone) {
  clone->x_ = x_;
  clone->y_ = y_;
  clone->width_ = width_;
  clone->height_ = height_;
  clone->parent_ = NULL;
  clone->z_ = 0.0f;
  clone->scale_x_ = scale_x_;
  clone->scale_y_ = scale_y_;
  clone->opacity_ = opacity_;
  clone->is_opaque_ = is_opaque_;
  clone->has_children_ = has_children_;
  clone->visible_ = visible_;
  clone->name_ = name_;

  // This copies all the drawing data, but they're all tr1::shared_ptr's,
  // so it all works out great.
  clone->drawing_data_ = drawing_data_;
}

void TidyInterface::Actor::Move(int x, int y, int duration_ms) {
  MoveX(x, duration_ms);
  MoveY(y, duration_ms);
}

void TidyInterface::Actor::MoveX(int x, int duration_ms) {
  AnimateInt(&x_, x, duration_ms);
}

void TidyInterface::Actor::MoveY(int y, int duration_ms) {
  AnimateInt(&y_, y, duration_ms);
}

void TidyInterface::Actor::Scale(double scale_x, double scale_y,
                                 int duration_ms) {
  AnimateFloat(&scale_x_, static_cast<float>(scale_x), duration_ms);
  AnimateFloat(&scale_y_, static_cast<float>(scale_y), duration_ms);
}

void TidyInterface::Actor::SetOpacity(double opacity, int duration_ms) {
  AnimateFloat(&opacity_, static_cast<float>(opacity), duration_ms);
}

void TidyInterface::Actor::Raise(ClutterInterface::Actor* other) {
  CHECK(parent_) << "Tried to raise an actor that has no parent.";
  TidyInterface::Actor* other_nc =
      dynamic_cast<TidyInterface::Actor*>(other);
  CHECK(other_nc) << "Failed to cast to an Actor in Raise";
  parent_->RaiseChild(this, other_nc);
  set_dirty();
}

void TidyInterface::Actor::Lower(ClutterInterface::Actor* other) {
  CHECK(parent_) << "Tried to lower an actor that has no parent.";
  TidyInterface::Actor* other_nc =
      dynamic_cast<TidyInterface::Actor*>(other);
  CHECK(other_nc) << "Failed to cast to an Actor in Lower";
  parent_->LowerChild(this, other_nc);
  set_dirty();
}

void TidyInterface::Actor::RaiseToTop() {
  CHECK(parent_) << "Tried to raise an actor to top that has no parent.";
  parent_->RaiseChild(this, NULL);
  set_dirty();
}

void TidyInterface::Actor::LowerToBottom() {
  CHECK(parent_) << "Tried to lower an actor to bottom that has no parent.";
  parent_->LowerChild(this, NULL);
  set_dirty();
}

TidyInterface::DrawingDataPtr TidyInterface::Actor::GetDrawingData(
    int32 id) const {
  DrawingDataMap::const_iterator iterator = drawing_data_.find(id);
  if (iterator != drawing_data_.end()) {
    return (*iterator).second;
  }
  return DrawingDataPtr();
}

void TidyInterface::Actor::Update(int* count,
                                  AnimationBase::AnimationTime now) {
  (*count)++;
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

void TidyInterface::Actor::AnimateFloat(float* field, float value,
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

void TidyInterface::Actor::AnimateInt(int* field, int value,
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

void TidyInterface::ContainerActor::AddActor(
    ClutterInterface::Actor* actor) {
  TidyInterface::Actor* cast_actor = dynamic_cast<Actor*>(actor);
  CHECK(cast_actor) << "Unable to down-cast actor.";
  cast_actor->set_parent(this);
  children_.insert(children_.begin(), cast_actor);
  set_has_children(true);
  set_dirty();
}

// Note that the passed-in Actors might be partially destroyed (the
// Actor destructor calls RemoveActor on its parent), so we shouldn't
// rely on the contents of the Actor.
void TidyInterface::ContainerActor::RemoveActor(
    ClutterInterface::Actor* actor) {
  ActorVector::iterator iterator = std::find(children_.begin(), children_.end(),
                                             actor);
  if (iterator != children_.end()) {
    children_.erase(iterator);
    set_has_children(!children_.empty());
    set_dirty();
  }
}

void TidyInterface::ContainerActor::Update(
    int* count, AnimationBase::AnimationTime now) {
  for (ActorVector::iterator iterator = children_.begin();
       iterator != children_.end(); ++iterator) {
    dynamic_cast<TidyInterface::Actor*>(*iterator)->Update(count, now);
  }
  TidyInterface::Actor::Update(count, now);
}

void TidyInterface::ContainerActor::RaiseChild(
    TidyInterface::Actor* child, TidyInterface::Actor* above) {
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

void TidyInterface::ContainerActor::LowerChild(
    TidyInterface::Actor* child, TidyInterface::Actor* below) {
  CHECK(child) << "Tried to lower a NULL child.";
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

TidyInterface::QuadActor::QuadActor(TidyInterface* interface)
    : TidyInterface::Actor(interface),
      color_(1.f, 1.f, 1.f) {
}

TidyInterface::Actor* TidyInterface::QuadActor::Clone() {
  QuadActor* new_instance = new QuadActor(interface());
  CloneImpl(new_instance);
  return static_cast<Actor*>(new_instance);
}

void TidyInterface::QuadActor::CloneImpl(QuadActor* clone) {
  Actor::CloneImpl(static_cast<TidyInterface::Actor*>(clone));
  clone->color_ = color_;
}

TidyInterface::TexturePixmapActor::TexturePixmapActor(
    TidyInterface* interface)
    : TidyInterface::QuadActor(interface),
      window_(0) {
}

bool TidyInterface::TexturePixmapActor::SetTexturePixmapWindow(
    XWindow xid) {
  Reset();
  window_ = xid;
  interface()->StartMonitoringWindowForChanges(window_, this);
  set_dirty();
  return true;
}

void TidyInterface::TexturePixmapActor::Reset() {
  if (window_)
    interface()->StopMonitoringWindowForChanges(window_, this);
  window_ = None;
  DestroyPixmap();
  set_dirty();
}

void TidyInterface::TexturePixmapActor::DestroyPixmap() {
#ifdef TIDY_OPENGL
  EraseDrawingData(OpenGlDrawVisitor::PIXMAP_DATA);
#endif
#ifdef TIDY_OPENGLES
  EraseDrawingData(OpenGlesDrawVisitor::kEglImageData);
#endif
}

TidyInterface::Actor* TidyInterface::TexturePixmapActor::Clone() {
  TexturePixmapActor* new_instance = new TexturePixmapActor(interface());
  CloneImpl(new_instance);
  return static_cast<Actor*>(new_instance);
}

void TidyInterface::TexturePixmapActor::CloneImpl(TexturePixmapActor* clone) {
  QuadActor::CloneImpl(static_cast<TidyInterface::QuadActor*>(clone));
  clone->window_ = window_;
}

bool TidyInterface::TexturePixmapActor::HasPixmapDrawingData() {
#ifdef TIDY_OPENGL
  return GetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA) != NULL;
#endif
#ifdef TIDY_OPENGLES
  return GetDrawingData(OpenGlesDrawVisitor::kEglImageData) != NULL;
#endif
}

void TidyInterface::TexturePixmapActor::RefreshPixmap() {
#ifdef TIDY_OPENGL
  OpenGlPixmapData* data  = dynamic_cast<OpenGlPixmapData*>(
      GetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA).get());
  if (data)
    data->Refresh();
#endif
  // TODO: Lift common damage and pixmap creation code to TidyInterface
#ifdef TIDY_OPENGLES
  OpenGlesEglImageData* data = dynamic_cast<OpenGlesEglImageData*>(
      GetDrawingData(OpenGlesDrawVisitor::kEglImageData).get());
  if (data)
    data->Refresh();
#endif
  set_dirty();
}

TidyInterface::StageActor::StageActor(TidyInterface* an_interface,
                                      int width, int height)
    : TidyInterface::ContainerActor(an_interface),
      stage_color_(1.f, 1.f, 1.f) {
  window_ = interface()->x_conn()->CreateSimpleWindow(
      interface()->x_conn()->GetRootWindow(),
      0, 0, width, height);
  interface()->x_conn()->MapWindow(window_);
}

TidyInterface::StageActor::~StageActor() {
  interface()->x_conn()->DestroyWindow(window_);
}

void TidyInterface::StageActor::SetStageColor(
    const ClutterInterface::Color& color) {
  stage_color_ = color;
}

void TidyInterface::StageActor::SetSizeImpl(int* width, int* height) {
  // Have to resize the window to match the stage.
  CHECK(window_) << "Missing window in StageActor::SetSizeImpl.";
  interface()->x_conn()->ResizeWindow(window_, *width, *height);
}

static gboolean DrawInterface(void* data) {
  reinterpret_cast<TidyInterface*>(data)->Draw();
  return TRUE;
}

TidyInterface::TidyInterface(XConnection* xconn,
                             GLInterfaceBase* gl_interface)
    : dirty_(true),
      xconn_(xconn),
      actor_count_(0) {
  CHECK(xconn_);
  now_ = GetCurrentRealTime();
  XWindow root = x_conn()->GetRootWindow();
  XConnection::WindowGeometry geometry;
  x_conn()->GetWindowGeometry(root, &geometry);
  default_stage_.reset(new TidyInterface::StageActor(this,
                                                     geometry.width,
                                                     geometry.height));
  default_stage_->SetSize(geometry.width, geometry.height);

#ifdef TIDY_OPENGL
  draw_visitor_ = new OpenGlDrawVisitor(gl_interface,
                                        this,
                                        default_stage_.get());
#elif defined(TIDY_OPENGLES)
  draw_visitor_ = new OpenGlesDrawVisitor(gl_interface,
                                          this,
                                          default_stage_.get());
#endif

  // TODO: Remove this lovely hack, and replace it with something that
  // knows more about keeping a consistent frame rate.
  g_timeout_add(20, window_manager::DrawInterface, this);
}

TidyInterface::~TidyInterface() {
  delete draw_visitor_;
}

TidyInterface::ContainerActor* TidyInterface::CreateGroup() {
  return new ContainerActor(this);
}

TidyInterface::Actor* TidyInterface::CreateRectangle(
    const ClutterInterface::Color& color,
    const ClutterInterface::Color& border_color,
    int border_width) {
  QuadActor* actor = new QuadActor(this);
  // TODO: Handle border color/width properly.
  actor->SetColor(color);
  return actor;
}

TidyInterface::Actor* TidyInterface::CreateImage(
    const std::string& filename) {
  QuadActor* actor = new QuadActor(this);
  scoped_ptr<ImageContainer> container(
      ImageContainer::CreateContainer(filename));
  if (container.get() &&
      container->LoadImage() == ImageContainer::IMAGE_LOAD_SUCCESS) {
    draw_visitor_->BindImage(container.get(), actor);
    actor->SetSize(container->width(), container->height());
  } else {
    actor->SetColor(ClutterInterface::Color(1.f, 0.f, 1.f));
  }

  return actor;
}

TidyInterface::TexturePixmapActor*
TidyInterface::CreateTexturePixmap() {
  return new TexturePixmapActor(this);
}

TidyInterface::Actor* TidyInterface::CreateText(
    const std::string& font_name,
    const std::string& text,
    const ClutterInterface::Color& color) {
  QuadActor* actor = new QuadActor(this);
  // TODO: Actually create the text.
  actor->SetColor(color);
  actor->SetOpacity(.5f, 0);
  return actor;
}

TidyInterface::Actor* TidyInterface::CloneActor(
    ClutterInterface::Actor* orig) {
  TidyInterface::Actor* actor = dynamic_cast<TidyInterface::Actor*>(orig);
  CHECK(actor);
  return actor->Clone();
}

void TidyInterface::RemoveActor(Actor* actor) {
  ActorVector::iterator iterator = std::find(actors_.begin(), actors_.end(),
                                             actor);
  if (iterator != actors_.end()) {
    actors_.erase(iterator);
  }
}

static GdkFilterReturn FilterEvent(GdkXEvent* xevent,
                                   GdkEvent* event,
                                   gpointer data) {
  TidyInterface* interface = static_cast<TidyInterface*>(data);
  return interface->HandleEvent(reinterpret_cast<XEvent*>(xevent)) ?
      GDK_FILTER_REMOVE : GDK_FILTER_CONTINUE;
}

bool TidyInterface::HandleEvent(XEvent* xevent) {
  static int damage_notify = x_conn()->damage_event_base() + XDamageNotify;

  if (xevent->type == ConfigureNotify) {
    TexturePixmapActor* actor =
        FindWithDefault(texture_pixmaps_,
                        xevent->xconfigure.window,
                        static_cast<TexturePixmapActor*>(NULL));
    // Get a new pixmap with a new size.
    if (actor) {
      actor->DestroyPixmap();
      actor->set_dirty();
    }
    return false;
  } else if (xevent->type == DestroyNotify) {
    TexturePixmapActor* actor =
        FindWithDefault(texture_pixmaps_,
                        xevent->xdestroywindow.window,
                        static_cast<TexturePixmapActor*>(NULL));
    if (actor)
      actor->Reset();
    return false;
  } else if (xevent->type == damage_notify) {
    TexturePixmapActor* actor =
        FindWithDefault(texture_pixmaps_,
                        reinterpret_cast<XDamageNotifyEvent*>(xevent)->drawable,
                        static_cast<TexturePixmapActor*>(NULL));
    if (actor)
      actor->RefreshPixmap();
    return (actor != NULL);;
  } else {
    return false;
  }
}

void TidyInterface::StartMonitoringWindowForChanges(
    XWindow xid, TexturePixmapActor* actor) {
  if (texture_pixmaps_.empty()) {
    gdk_window_add_filter(NULL, FilterEvent, this);
  }

  texture_pixmaps_[xid] = actor;

  x_conn()->RedirectWindowForCompositing(xid);
}

void TidyInterface::StopMonitoringWindowForChanges(
    XWindow xid, TexturePixmapActor* actor) {
  x_conn()->UnredirectWindowForCompositing(xid);

  texture_pixmaps_.erase(xid);
  if (texture_pixmaps_.empty()) {
    gdk_window_remove_filter(NULL, FilterEvent, this);
  }
}

void TidyInterface::Draw() {
  now_ = GetCurrentRealTime();
  actor_count_ = 0;
  default_stage_->Update(&actor_count_, now_);
  if (dirty_) {
    default_stage_->Accept(draw_visitor_);
    dirty_ = false;
  }
}

TidyInterface::AnimationBase::AnimationTime
TidyInterface::GetCurrentRealTime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return 1000ULL * tv.tv_sec + tv.tv_usec / 1000ULL;
}

}  // namespace window_manager
