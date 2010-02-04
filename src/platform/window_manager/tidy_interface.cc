// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/tidy_interface.h"

#include <gdk/gdkx.h>
#include <sys/time.h>
#include <time.h>

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "window_manager/image_container.h"
#ifdef TIDY_OPENGL
#include "window_manager/opengl_visitor.h"
#endif

using std::tr1::shared_ptr;

// Turn this on if you want to debug the visitor traversal.
#undef EXTRA_LOGGING

namespace window_manager {

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
  ActorVector::const_iterator iterator = actor->children().begin();
  while (iterator != actor->children().end()) {
    if (*iterator) {
      (*iterator)->Accept(this);
    }
    ++iterator;
  }
}

TidyInterface::ActorCollector::ActorCollector()
    : branches_(true),
      leaves_(true),
      opaque_(TidyInterface::ActorCollector::VALUE_EITHER),
      visible_(TidyInterface::ActorCollector::VALUE_EITHER) {
}

TidyInterface::ActorCollector::~ActorCollector() {
}

void TidyInterface::ActorCollector::VisitActor(Actor* actor) {
  CHECK(actor);
  bool is_container = dynamic_cast<ContainerActor*>(actor) != NULL;
#ifdef EXTRA_LOGGING
  LOG(INFO) << "Looking for "
            << (leaves_ ? "leaves, " : "")
            << (branches_ ? "branches, " : "")
            << (visible_ == VALUE_FALSE ? "not visible, " : "")
            << (visible_ == VALUE_TRUE ? "visible, " : "")
            << (visible_ == VALUE_EITHER ? "either visible, " : "")
            << (opaque_ == VALUE_FALSE ? "not opaque" : "")
            << (opaque_ == VALUE_TRUE ? "opaque" : "")
            << (opaque_ == VALUE_EITHER ? "either opaque" : "");
#endif  // EXTRA_LOGGING
  if ((!is_container && leaves_) || (is_container && branches_)) {
    TriValue is_visible = actor->IsVisible() ? VALUE_TRUE : VALUE_FALSE;
    TriValue is_opaque = actor->is_opaque() ? VALUE_TRUE : VALUE_FALSE;
    if (is_visible == visible_ || visible_ == VALUE_EITHER ) {
      if (is_opaque == opaque_ || opaque_ == VALUE_EITHER) {
#ifdef EXTRA_LOGGING
        LOG(INFO) << "Visiting an actor (" << actor->name() << ") that "
                  << (is_container ? "is" : "is not") << " a container, "
                  << (actor->IsVisible() ? "is" : "is not") << " visible and "
                  << (actor->is_opaque() ? "is" : "is not") << " opaque.";
#endif  // EXTRA_LOGGING
        results_.push_back(actor);
      }
    }
  }
}

void TidyInterface::ActorCollector::VisitContainer(
    TidyInterface::ContainerActor* actor) {
  CHECK(actor);
  this->VisitActor(actor);
  ActorVector::const_iterator iterator = actor->children().begin();
  while (iterator != actor->children().end()) {
    if (*iterator) {
      // Don't traverse actors that don't match visibility filter.
      TriValue is_visible = actor->IsVisible() ? VALUE_TRUE : VALUE_FALSE;
      if (is_visible == visible_ || visible_ == VALUE_EITHER ) {
        (*iterator)->Accept(this);
      }
    }
    ++iterator;
  }
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
      width_(0),
      height_(0),
      z_(0.f),
      scale_x_(1.f),
      scale_y_(1.f),
      opacity_(1.f),
      visible_(true) {
  interface_->AddActor(this);
}

TidyInterface::Actor* TidyInterface::Actor::Clone() {
  // TODO: This doesn't do a "real" clone at all.  Determine if we
  // need it to or not, and either remove it or implement a real
  // clone.
  return new Actor(interface_);
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
  if (window_) {
    interface()->StopMonitoringWindowForChanges(window_, this);
  }

#ifdef TIDY_OPENGL
  EraseDrawingData(OpenGlDrawVisitor::PIXMAP_DATA);
#endif
}

void TidyInterface::TexturePixmapActor::Refresh() {
#ifdef TIDY_OPENGL
  OpenGlPixmapData* data  = dynamic_cast<OpenGlPixmapData*>(
      GetDrawingData(OpenGlDrawVisitor::PIXMAP_DATA).get());
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

void TidyInterface::StageActor::SetSizeImpl(int width, int height) {
  // Have to resize the window to match the stage.
  CHECK(window_) << "Missing window in StageActor::SetSizeImpl.";
  interface()->x_conn()->ResizeWindow(window_, width, height);
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
#endif

  // TODO: Remove this lovely hack, and replace it with something that
  // knows more about keeping a consistent frame rate.
  g_timeout_add(20, window_manager::DrawInterface, this);
}

TidyInterface::~TidyInterface() {
#ifdef TIDY_OPENGL
  delete draw_visitor_;
#endif
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
#ifdef TIDY_OPENGL
    draw_visitor_->BindImage(container.get(), actor);
#endif
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
  Actor* actor = dynamic_cast<Actor*>(orig);
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
#ifdef TIDY_OPENGL
    default_stage_->Accept(draw_visitor_);
#endif
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
