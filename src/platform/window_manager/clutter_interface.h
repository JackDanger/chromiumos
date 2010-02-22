// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_CLUTTER_INTERFACE_H_
#define WINDOW_MANAGER_CLUTTER_INTERFACE_H_

#include <list>
#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "window_manager/x_types.h"

namespace window_manager {

template<class T> class Stacker;  // from util.h
class XConnection;

// A wrapper around Clutter's C API.
//
// TODO: We'll almost certainly need something more flexible here.  For
// example, it'd be nice to still have control over which alpha function
// is used, or to specify multiple parameters to be animated at once (as
// can be done with implicit animations), or to be able to chain sequential
// animations together.
class ClutterInterface {
 public:
  struct Color {
    Color() : red(0.f), green(0.f), blue(0.f) {}
    Color(float r, float g, float b) : red(r), green(g), blue(b) {}
    float red;
    float green;
    float blue;
  };

  // Abstract base class for actors, inherited from by both:
  // - more-specific abstract classes within ClutterInterface that add
  //   additional virtual methods
  // - concrete Actor classes inside of implementations of ClutterInterface
  //   that implement this class's methods
  class Actor {
   public:
    Actor() {}
    virtual ~Actor() {}

    virtual void SetName(const std::string& name) = 0;
    virtual int GetWidth() = 0;
    virtual int GetHeight() = 0;
    virtual int GetX() = 0;
    virtual int GetY() = 0;
    virtual int GetXScale() = 0;
    virtual int GetYScale() = 0;

    virtual void SetVisibility(bool visible) = 0;
    virtual void SetSize(int width, int height) = 0;
    virtual void Move(int x, int y, int anim_ms) = 0;
    virtual void MoveX(int x, int anim_ms) = 0;
    virtual void MoveY(int y, int anim_ms) = 0;
    virtual void Scale(double scale_x, double scale_y, int anim_ms) = 0;
    virtual void SetOpacity(double opacity, int anim_ms) = 0;
    virtual void SetClip(int x, int y, int width, int height) = 0;

    // Move an actor directly above or below another actor in the stacking
    // order, or to the top or bottom of all of its siblings.
    virtual void Raise(Actor* other) = 0;
    virtual void Lower(Actor* other) = 0;
    virtual void RaiseToTop() = 0;
    virtual void LowerToBottom() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(Actor);
  };

  class ContainerActor : virtual public Actor {
   public:
    ContainerActor() {}
    virtual ~ContainerActor() {}
    virtual void AddActor(Actor* actor) = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(ContainerActor);
  };

  class StageActor : virtual public ContainerActor {
   public:
    StageActor() {}
    virtual ~StageActor() {}
    virtual XWindow GetStageXWindow() = 0;
    virtual void SetStageColor(const Color &color) = 0;
    virtual std::string GetDebugString() = 0;
   private:
    DISALLOW_COPY_AND_ASSIGN(StageActor);
  };

  class TexturePixmapActor : virtual public Actor {
   public:
    TexturePixmapActor() {}
    virtual ~TexturePixmapActor() {}
    virtual bool SetTexturePixmapWindow(XWindow xid) = 0;
    virtual bool IsUsingTexturePixmapExtension() = 0;

    // Add an additional texture to mask out parts of the actor.
    // 'bytes' must be of size 'width' * 'height'.
    virtual bool SetAlphaMask(
        const unsigned char* bytes, int width, int height) = 0;

    // Clear the previously-applied alpha mask.
    virtual void ClearAlphaMask() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(TexturePixmapActor);
  };

  ClutterInterface() {}
  virtual ~ClutterInterface() {}

  // These methods create new Actor objects.  The caller is responsible for
  // deleting them, even (unlike Clutter) after they have been added to a
  // container.  See RealClutterInterface::Actor for more details.
  virtual ContainerActor* CreateGroup() = 0;
  virtual Actor* CreateRectangle(const Color& color, const Color& border_color,
                                 int border_width) = 0;
  virtual Actor* CreateImage(const std::string& filename) = 0;
  virtual TexturePixmapActor* CreateTexturePixmap() = 0;
  virtual Actor* CreateText(const std::string& font_name,
                            const std::string& text,
                            const Color& color) = 0;
  virtual Actor* CloneActor(Actor* orig) = 0;

  // Get the default stage object.  Ownership of the StageActor remains
  // with ClutterInterface -- the caller should not delete it.
  virtual StageActor* GetDefaultStage() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ClutterInterface);
};

// Mock implementation of ClutterInterface that can be used without calling
// clutter_init().
class MockClutterInterface : public ClutterInterface {
 public:
  class ContainerActor;

  class Actor : virtual public ClutterInterface::Actor {
   public:
    Actor()
        : x_(-1),
          y_(-1),
          width_(-1),
          height_(-1),
          scale_x_(1.0),
          scale_y_(1.0),
          opacity_(1.0),
          visible_(true),
          parent_(NULL) {
    }
    virtual ~Actor();

    int x() const { return x_; }
    int y() const { return y_; }
    double scale_x() const { return scale_x_; }
    double scale_y() const { return scale_y_; }
    double opacity() const { return opacity_; }
    bool visible() const { return visible_; }

    MockClutterInterface::ContainerActor* parent() { return parent_; }
    void set_parent(MockClutterInterface::ContainerActor* new_parent) {
      parent_ = new_parent;
    }

    // Begin ClutterInterface::Actor methods
    void SetName(const std::string& name) {}
    int GetWidth() { return width_; }
    int GetHeight() { return height_; };
    int GetX() { return x_; }
    int GetY() { return y_; };
    int GetXScale() { return scale_x_; }
    int GetYScale() { return scale_y_; }
    void SetVisibility(bool visible) { visible_ = visible; }
    void SetSize(int width, int height) {
      width_ = width;
      height_ = height;
    }
    void Move(int x, int y, int anim_ms) {
      x_ = x;
      y_ = y;
    }
    void MoveX(int x, int anim_ms) { Move(x, y_, anim_ms); }
    void MoveY(int y, int anim_ms) { Move(x_, y, anim_ms); }
    void Scale(double scale_x, double scale_y, int anim_ms) {
      scale_x_ = scale_x;
      scale_y_ = scale_y;
    }
    void SetOpacity(double opacity, int anim_ms) { opacity_ = opacity; }
    void SetClip(int x, int y, int width, int height) {}
    void Raise(ClutterInterface::Actor* other);
    void Lower(ClutterInterface::Actor* other);
    void RaiseToTop();
    void LowerToBottom();
    // End ClutterInterface::Actor methods

   protected:
    int x_, y_;
    int width_, height_;
    double scale_x_, scale_y_;
    double opacity_;
    bool visible_;

    MockClutterInterface::ContainerActor* parent_;  // not owned

    DISALLOW_COPY_AND_ASSIGN(Actor);
  };

  class ContainerActor : public MockClutterInterface::Actor,
                         virtual public ClutterInterface::ContainerActor {
   public:
    ContainerActor();
    virtual ~ContainerActor();
    void AddActor(ClutterInterface::Actor* actor);

    Stacker<Actor*>* stacked_children() { return stacked_children_.get(); }

    // Get an index representing an actor's stacking position inside of
    // this container.  Objects stacked higher have lower indexes.
    // Convenient for testing.
    int GetStackingIndex(ClutterInterface::Actor* actor);

   private:
    scoped_ptr<Stacker<Actor*> > stacked_children_;

    DISALLOW_COPY_AND_ASSIGN(ContainerActor);
  };

  class StageActor : public MockClutterInterface::ContainerActor,
                     public ClutterInterface::StageActor {
   public:
    StageActor() {}
    virtual ~StageActor() {}
    XWindow GetStageXWindow() { return 0; }
    void SetStageColor(const ClutterInterface::Color& color) {}
    std::string GetDebugString() { return ""; }
   private:
    DISALLOW_COPY_AND_ASSIGN(StageActor);
  };

  class TexturePixmapActor : public MockClutterInterface::Actor,
                             public ClutterInterface::TexturePixmapActor {
   public:
    TexturePixmapActor(XConnection* xconn)
        : xconn_(xconn),
          alpha_mask_bytes_(NULL),
          xid_(0) {}
    virtual ~TexturePixmapActor() {
      ClearAlphaMask();
    }
    const unsigned char* alpha_mask_bytes() const { return alpha_mask_bytes_; }
    const XWindow xid() const { return xid_; }

    bool SetTexturePixmapWindow(XWindow xid);
    bool IsUsingTexturePixmapExtension() { return false; }
    bool SetAlphaMask(const unsigned char* bytes, int width, int height);
    void ClearAlphaMask();

   private:
    XConnection* xconn_;  // not owned

    // Shape as set by SetAlphaMask(), or NULL if the actor is unshaped.
    unsigned char* alpha_mask_bytes_;

    // Redirected window that we're displaying.
    XWindow xid_;

    DISALLOW_COPY_AND_ASSIGN(TexturePixmapActor);
  };

  MockClutterInterface(XConnection* xconn) : xconn_(xconn) {}
  ~MockClutterInterface() {}

  // Begin ClutterInterface methods
  ContainerActor* CreateGroup() { return new ContainerActor; }
  Actor* CreateRectangle(const ClutterInterface::Color& color,
                         const ClutterInterface::Color& border_color,
                         int border_width) {
    return new Actor;
  }
  Actor* CreateImage(const std::string& filename) { return new Actor; }
  TexturePixmapActor* CreateTexturePixmap() {
    return new TexturePixmapActor(xconn_);
  }
  Actor* CreateText(const std::string& font_name,
                    const std::string& text,
                    const ClutterInterface::Color& color) {
    return new Actor;
  }
  Actor* CloneActor(ClutterInterface::Actor* orig) { return new Actor; }
  StageActor* GetDefaultStage() { return &default_stage_; }
  // End ClutterInterface methods

 private:
  XConnection* xconn_;  // not owned
  StageActor default_stage_;

  DISALLOW_COPY_AND_ASSIGN(MockClutterInterface);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_CLUTTER_INTERFACE_H_
