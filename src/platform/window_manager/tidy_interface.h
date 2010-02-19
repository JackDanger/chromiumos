// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_TIDY_INTERFACE_H_
#define WINDOW_MANAGER_TIDY_INTERFACE_H_

#include <math.h>

#include <list>
#include <string>
#include <tr1/memory>
#include <vector>

#include <gtest/gtest_prod.h>  // for FRIEND_TEST() macro

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/x_connection.h"
#include "window_manager/gl_interface_base.h"

#if !(defined(TIDY_OPENGL) || defined(TIDY_OPENGLES))
#error TIDY_OPENGL or TIDY_OPENGLES must be defined
#endif

namespace window_manager {

class OpenGlDrawVisitor;
class OpenGlesDrawVisitor;

class TidyInterface : public ClutterInterface {
 public:
  class Actor;
  class AnimationBase;
  class ContainerActor;
  class DrawingData;
  class QuadActor;
  class StageActor;
  class TexturePixmapActor;

  typedef std::vector<Actor*> ActorVector;
  typedef std::list<std::tr1::shared_ptr<AnimationBase> > AnimationList;
  typedef std::tr1::shared_ptr<DrawingData> DrawingDataPtr;
  typedef std::map<int32, DrawingDataPtr> DrawingDataMap;

  // Base class for memento storage on the actors.
  class DrawingData {
   public:
    DrawingData() {}
    virtual ~DrawingData() {}
  };

  class AnimationBase {
   public:
    // This is in milliseconds.
    typedef int64 AnimationTime;
    AnimationBase(AnimationTime start_time, AnimationTime end_time);
    virtual ~AnimationBase() {}
    // Evaluate the animation at the passed-in time and update the
    // field associated with it.  It returns true when the animation
    // is finished.
    virtual bool Eval(AnimationTime current_time) = 0;

   protected:
    AnimationTime start_time_;
    AnimationTime end_time_;
    float ease_factor_;

   private:
    DISALLOW_COPY_AND_ASSIGN(AnimationBase);
  };

  class FloatAnimation : public AnimationBase {
   public:
    FloatAnimation(float* field, float end_value,
                   AnimationTime start_time, AnimationTime end_time);
    bool Eval(AnimationTime current_time);

   private:
    float* field_;
    float start_value_;
    float end_value_;

    DISALLOW_COPY_AND_ASSIGN(FloatAnimation);
  };

  class IntAnimation : public AnimationBase {
   public:
    IntAnimation(int* field, int end_value,
                 AnimationTime start_time, AnimationTime end_time);
    bool Eval(AnimationTime current_time);

   private:
    int* field_;
    int start_value_;
    int end_value_;

    DISALLOW_COPY_AND_ASSIGN(IntAnimation);
  };

  class ActorVisitor {
   public:
    ActorVisitor() {}
    virtual ~ActorVisitor() {}
    virtual void VisitActor(Actor* actor) = 0;

    // Default implementation visits container as an Actor, and then
    // calls Visit on all the container's children.
    virtual void VisitContainer(ContainerActor* actor);
    virtual void VisitStage(StageActor* actor) {
      VisitContainer(actor);
    }
    virtual void VisitTexturePixmap(TexturePixmapActor* actor) {
      VisitActor(actor);
    }
    virtual void VisitQuad(QuadActor* actor) {
      VisitActor(actor);
    }
   private:
    DISALLOW_COPY_AND_ASSIGN(ActorVisitor);
  };

  class VisitorDestination {
   public:
    VisitorDestination() {}
    virtual ~VisitorDestination() {}

    // This function accepts a visitor into the destination to be
    // visited.
    virtual void Accept(ActorVisitor* visitor) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(VisitorDestination);
  };

  class LayerVisitor
      : virtual public TidyInterface::ActorVisitor {
   public:
    static const float kMinDepth;
    static const float kMaxDepth;

    LayerVisitor(int32 count)
        : depth_(0.0f),
          layer_thickness_(0.0f),
          count_(count) {}
    virtual ~LayerVisitor() {}

    virtual void VisitActor(TidyInterface::Actor* actor);
    virtual void VisitStage(TidyInterface::StageActor* actor);
    virtual void VisitContainer(TidyInterface::ContainerActor* actor);
    virtual void VisitQuad(TidyInterface::QuadActor* actor);
    virtual void VisitTexturePixmap(TidyInterface::TexturePixmapActor* actor);

   private:
    float depth_;
    float layer_thickness_;
    int32 count_;

    DISALLOW_COPY_AND_ASSIGN(LayerVisitor);
  };

  class Actor : virtual public ClutterInterface::Actor,
                public TidyInterface::VisitorDestination {
   public:
    explicit Actor(TidyInterface* interface);
    virtual ~Actor();

    // Begin ClutterInterface::VisitorDestination methods
    virtual void Accept(ActorVisitor* visitor) {
      DCHECK(visitor);
      visitor->VisitActor(this);
    }
    // End ClutterInterface::VisitorDestination methods

    // Begin ClutterInterface::Actor methods
    virtual Actor* Clone();
    int GetWidth() { return width_; }
    int GetHeight() { return height_; }
    int GetX() { return x_; }
    int GetY() { return y_; }
    int GetXScale() { return scale_x_; }
    int GetYScale() { return scale_y_; }
    void SetVisibility(bool visible) {
      visible_ = visible;
      set_dirty();
    }
    void SetSize(int width, int height) {
      width_ = width;
      height_ = height;
      SetSizeImpl(&width_, &height_);
      set_dirty();
    }
    void SetName(const std::string& name) { name_ = name; }
    const std::string& name() const { return name_; }

    void Move(int x, int y, int duration_ms);
    void MoveX(int x, int duration_ms);
    void MoveY(int y, int duration_ms);
    void Scale(double scale_x, double scale_y, int duration_ms);
    void SetOpacity(double opacity, int duration_ms);
    void SetClip(int x, int y, int width, int height) { NOTIMPLEMENTED(); }

    void Raise(ClutterInterface::Actor* other);
    void Lower(ClutterInterface::Actor* other);
    void RaiseToTop();
    void LowerToBottom();
    // End ClutterInterface::Actor methods

    // Updates the actor in response to time passing, and counts the
    // number of actors as it goes.
    virtual void Update(int32* count, AnimationBase::AnimationTime now);

    // Regular actors have no children, but we want to be able to
    // avoid a virtual function call to determine this while
    // traversing.
    bool has_children() const { return has_children_; }

    virtual ActorVector GetChildren() {
      return ActorVector();
    }

    void set_parent(ContainerActor* parent) { parent_ = parent; }
    ContainerActor* parent() const { return parent_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int x() const { return x_; }
    int y() const { return y_; }
    void set_z(float z) { z_ = z; }
    float z() const { return z_; }

    // Note that is_opaque isn't valid until after a LayerVisitor has
    // been run over the tree -- that's what calculates the opacity
    // flag.
    bool is_opaque() const { return is_opaque_; }

    bool IsVisible() const { return visible_ && opacity_ > 0.001; }
    float opacity() const { return opacity_; }
    float scale_x() const { return scale_x_; }
    float scale_y() const { return scale_y_; }
    void set_dirty() { interface_->dirty_ = true; }

    // Sets the drawing data of the given type on this object.
    void SetDrawingData(int32 id, DrawingDataPtr data) {
      drawing_data_[id] = data;
    }

    // Gets the drawing data of the given type.
    DrawingDataPtr GetDrawingData(int32 id) const;

    // Erases the drawing data of the given type.
    void EraseDrawingData(int32 id) { drawing_data_.erase(id); }

   protected:
    // So it can update the opacity flag.
    friend class TidyInterface::LayerVisitor;

    TidyInterface* interface() { return interface_; }

    void CloneImpl(Actor* clone);
    virtual void SetSizeImpl(int* width, int* height) {}

    void AnimateFloat(float* field, float value, int duration_ms);
    void AnimateInt(int* field, int value, int duration_ms);

    void set_has_children(bool has_children) { has_children_ = has_children; }
    void set_is_opaque(bool opaque) { is_opaque_ = opaque; }

   private:
    TidyInterface* interface_;

    // This points to the parent that has this actor as a child.
    ContainerActor* parent_;

    // This is the x position on the screen.
    int x_;

    // This is the y position on the screen.
    int y_;

    // This is the width and height of the actor's bounding box.
    int width_;
    int height_;

    // This is the z depth of this actor (which is set according to
    // the layer this actor is on.
    float z_;

    // This is the x and y scale of the actor.
    float scale_x_;
    float scale_y_;

    // This is the opacity of the actor (0 = transparent, 1 = opaque)
    float opacity_;

    // Calculated during the layer visitor pass, and used to determine
    // if this object is opaque for traversal purposes.
    bool is_opaque_;

    // This indicates if this actor has any children (false for all
    // but containers).  This is here so we can avoid a virtual
    // function call to determine this during the drawing traversal.
    bool has_children_;

    // This says whether or not to show this actor.
    bool visible_;

    // This is a name used for identifying the actor (most useful for
    // debugging).
    std::string name_;

    // This is a list of animations that are active on this actor.
    AnimationList animations_;

    // This keeps a mapping of int32 id to drawing data pointer.
    // The id space is maintained by the visitor implementation.
    DrawingDataMap drawing_data_;

    DISALLOW_COPY_AND_ASSIGN(Actor);
  };

  class ContainerActor : public TidyInterface::Actor,
                         virtual public ClutterInterface::ContainerActor {
   public:
    explicit ContainerActor(TidyInterface* interface)
        : TidyInterface::Actor(interface) {
    }

    // Implement VisitorDestination for visitor.
    void Accept(ActorVisitor* visitor) {
      CHECK(visitor);
      visitor->VisitContainer(this);
    }

    virtual Actor* Clone() { NOTIMPLEMENTED(); return NULL; }
    virtual ActorVector GetChildren() { return children_; }

    void AddActor(ClutterInterface::Actor* actor);
    void RemoveActor(ClutterInterface::Actor* actor);
    virtual void Update(int32* count, AnimationBase::AnimationTime now);

    // Raise one child over another.  Raise to top if "above" is NULL.
    void RaiseChild(TidyInterface::Actor* child,
                    TidyInterface::Actor* above);
    // Lower one child under another.  Lower to bottom if "below" is NULL.
    void LowerChild(TidyInterface::Actor* child,
                    TidyInterface::Actor* below);

   protected:
    virtual void SetSizeImpl(int* width, int* height) {
      // For containers, the size is always 1x1.
      // TODO: Implement a more complete story for setting sizes of containers.
      *width = 1;
      *height = 1;
    }
   private:
    // The list of this container's children.
    ActorVector children_;
    DISALLOW_COPY_AND_ASSIGN(ContainerActor);
  };

  // This class contains an actor that is a quadralateral.
  class QuadActor : public TidyInterface::Actor {
   public:
    explicit QuadActor(TidyInterface* interface);

    void SetColor(const ClutterInterface::Color& color) {
      color_ = color;
    }
    const ClutterInterface::Color& color() const { return color_; }

    // Implement VisitorDestination for visitor.
    void Accept(ActorVisitor* visitor) {
      CHECK(visitor);
      visitor->VisitQuad(this);
    }

    virtual Actor* Clone();
   protected:
    void CloneImpl(QuadActor* clone);
   private:
    ClutterInterface::Color color_;

    DISALLOW_COPY_AND_ASSIGN(QuadActor);
  };

  class TexturePixmapActor : public TidyInterface::QuadActor,
                             public ClutterInterface::TexturePixmapActor {
   public:
    explicit TexturePixmapActor(TidyInterface* interface);
    virtual ~TexturePixmapActor() { Reset(); }

    // Implement VisitorDestination for visitor.
    void Accept(ActorVisitor* visitor) {
      CHECK(visitor);
      visitor->VisitTexturePixmap(this);
    }

    bool SetTexturePixmapWindow(XWindow xid);
    XWindow texture_pixmap_window() const { return window_; }
    bool IsUsingTexturePixmapExtension() { return true; }
    bool SetAlphaMask(const unsigned char* bytes, int width, int height) {
      NOTIMPLEMENTED();
      return true;
    }
    void ClearAlphaMask() { NOTIMPLEMENTED(); }

    // Refresh the current pixmap.
    void RefreshPixmap();

    // Stop monitoring the current window, if any, for changes and destroy
    // the current pixmap.
    void Reset();

    // Throw out the current pixmap.  A new one will be created
    // automatically when needed.
    void DestroyPixmap();

    virtual Actor* Clone();

   protected:
    void CloneImpl(TexturePixmapActor* clone);

   private:
    FRIEND_TEST(TidyTest, HandleXEvents);

    // Is there currently any pixmap drawing data?  Tests use this to
    // check that old pixmaps get thrown away when needed.
    bool HasPixmapDrawingData();

    // This is the XWindow that this actor is associated with.
    XWindow window_;

    DISALLOW_COPY_AND_ASSIGN(TexturePixmapActor);
  };

  class StageActor : public TidyInterface::ContainerActor,
                     public ClutterInterface::StageActor {
   public:
    StageActor(TidyInterface* interface, int width, int height);
    virtual ~StageActor();

    // Implement VisitorDestination for visitor.
    void Accept(ActorVisitor* visitor) {
      CHECK(visitor);
      visitor->VisitStage(this);
    }

    virtual Actor* Clone() { NOTIMPLEMENTED(); return NULL; }

    XWindow GetStageXWindow() { return window_; }
    void SetStageColor(const ClutterInterface::Color& color);
    const ClutterInterface::Color& stage_color() const {
      return stage_color_;
    }
    virtual std::string GetDebugString() {
      NOTIMPLEMENTED();
      return "";
    }

   protected:
    virtual void SetSizeImpl(int* width, int* height);

   private:
    // This is the XWindow associated with the stage.  Owned by this class.
    XWindow window_;

    ClutterInterface::Color stage_color_;
    DISALLOW_COPY_AND_ASSIGN(StageActor);
  };

  TidyInterface(XConnection* x_connection,
                GLInterfaceBase* gl_interface);
  ~TidyInterface();

  // Begin ClutterInterface methods
  ContainerActor* CreateGroup();
  Actor* CreateRectangle(const ClutterInterface::Color& color,
                         const ClutterInterface::Color& border_color,
                         int border_width);
  Actor* CreateImage(const std::string& filename);
  TexturePixmapActor* CreateTexturePixmap();
  Actor* CreateText(const std::string& font_name,
                    const std::string& text,
                    const ClutterInterface::Color& color);
  Actor* CloneActor(ClutterInterface::Actor* orig);
  StageActor* GetDefaultStage() { return default_stage_.get(); }
  // End ClutterInterface methods

  void AddActor(Actor* actor) { actors_.push_back(actor); }
  void RemoveActor(Actor* actor);

  AnimationBase::AnimationTime GetCurrentTime() { return now_; }
  bool HandleEvent(XEvent* event);
  int actor_count() { return actor_count_; }
  bool dirty() const { return dirty_; }

  void Draw();

  XConnection* x_conn() const { return xconn_; }

 private:
  FRIEND_TEST(OpenGlVisitorTestTree, LayerDepth);  // sets actor count

  // Used by tests.
  void set_actor_count(int count) { actor_count_ = count; }

  // Returns the real current time, for updating animation time.
  AnimationBase::AnimationTime GetCurrentRealTime();

  // This is called when we start monitoring for changes, and sets up
  // redirection for the supplied window.
  void StartMonitoringWindowForChanges(XWindow xid, TexturePixmapActor* actor);

  // This is called when we stop monitoring for changes, and removes
  // the redirection for the supplied window.
  void StopMonitoringWindowForChanges(XWindow xid, TexturePixmapActor* actor);

  // This indicates if the interface is dirty and needs to be redrawn.
  bool dirty_;

  // This is the X connection to use, and is not owned.
  XConnection* xconn_;

  // This is the list of actors to display.
  ActorVector actors_;

  // This is the default stage where the actors are placed.
  scoped_ptr<StageActor> default_stage_;

  // This is the current time used to evaluate the currently active animations.
  AnimationBase::AnimationTime now_;

  typedef base::hash_map<XWindow, TexturePixmapActor*>
  XIDToTexturePixmapActorMap;

  // This is a map that allows us to look up the texture associated
  // with an XWindow.
  XIDToTexturePixmapActorMap texture_pixmaps_;

  // This is the count of actors in the tree as of the last time
  // Update was called.  It is used to compute the depth delta for
  // layer depth calculations.
  int32 actor_count_;

#ifdef TIDY_OPENGL
  OpenGlDrawVisitor* draw_visitor_;
#elif defined(TIDY_OPENGLES)
  OpenGlesDrawVisitor* draw_visitor_;
#endif

  DISALLOW_COPY_AND_ASSIGN(TidyInterface);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_TIDY_INTERFACE_H_
