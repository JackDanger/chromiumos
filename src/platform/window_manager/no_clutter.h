// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_NO_CLUTTER_H_
#define WINDOW_MANAGER_NO_CLUTTER_H_

#include <GL/glx.h>
#include <math.h>

#include <list>
#include <string>
#include <tr1/memory>
#include <vector>

#include "base/hash_tables.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/gl_interface.h"
#include "window_manager/x_connection.h"

namespace window_manager {

class NoClutterInterface : public ClutterInterface {
 public:
  class Actor;
  class AnimationBase;
  class ContainerActor;

  typedef std::vector<Actor*> ActorVector;
  typedef std::list<std::tr1::shared_ptr<AnimationBase> > AnimationList;

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

  class Actor : virtual public ClutterInterface::Actor {
   public:
    explicit Actor(NoClutterInterface* interface);
    virtual ~Actor();

    // Begin ClutterInterface::Actor methods
    virtual Actor* Clone();
    int GetWidth() { return width_; }
    int GetHeight() { return height_; }
    void SetVisibility(bool visible) {
      visible_ = visible;
      set_dirty();
    }
    void SetSize(int width, int height) {
      SetSizeImpl(width, height);
      width_ = width;
      height_ = height;
      set_dirty();
    }
    void SetName(const std::string& name) { name_ = name; }

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

    // Updates the actor in response to time passing.  The actor is
    // set to depth 'depth', and updates the parameter to contain the
    // depth that should be used for the next actor.
    virtual void Update(float* depth, AnimationBase::AnimationTime now);

    // Traverse the scene and add actors to the given display list.
    // When opaque is true, only opaque actors are added, when opaque
    // is false, only non-opaque actors are added.
    void AddToDisplayList(NoClutterInterface::ActorVector* actors, bool opaque);

    virtual void Draw() { }
    void set_parent(ContainerActor* parent) { parent_ = parent; }
    ContainerActor* parent() const { return parent_; }
    float z() const { return z_; }
    void set_z(float z) { z_ = z; }

   protected:
    NoClutterInterface* interface() { return interface_; }

    virtual void SetSizeImpl(int width, int height) {}
    virtual void AddToDisplayListImpl(NoClutterInterface::ActorVector* actors,
                                      bool opaque) {}

    void AnimateFloat(float* field, float value, int duration_ms);
    void AnimateInt(int* field, int value, int duration_ms);

    int width() { return width_; }
    int height() { return height_; }
    int x() { return x_; }
    int y() { return y_; }
    bool is_opaque() { return opacity_ > 0.999f; }
    bool IsVisible() { return visible_ && opacity_ > 0.001; }
    float opacity() { return opacity_; }
    float scale_x() { return scale_x_; }
    float scale_y() { return scale_y_; }
    void set_dirty() { interface_->dirty_ = true; }

   private:
    NoClutterInterface* interface_;
    ContainerActor* parent_;
    int x_;
    int y_;
    int width_;
    int height_;
    float z_;
    float scale_x_;
    float scale_y_;
    float opacity_;
    bool visible_;
    std::string name_;
    AnimationList animations_;
    DISALLOW_COPY_AND_ASSIGN(Actor);
  };

  class ContainerActor : public NoClutterInterface::Actor,
                         virtual public ClutterInterface::ContainerActor {
   public:
    explicit ContainerActor(NoClutterInterface* interface)
        : NoClutterInterface::Actor(interface) {
    }
    void AddActor(ClutterInterface::Actor* actor);
    void RemoveActor(ClutterInterface::Actor* actor);
    virtual void Update(float* depth, AnimationBase::AnimationTime now);
    virtual void AddToDisplayListImpl(NoClutterInterface::ActorVector* actors,
                                      bool opaque);
    // Raise one child over another.  Raise to top if "above" is NULL.
    void RaiseChild(NoClutterInterface::Actor* child,
                    NoClutterInterface::Actor* above);
    // Lower one child under another.  Lower to bottom if "below" is NULL.
    void LowerChild(NoClutterInterface::Actor* child,
                    NoClutterInterface::Actor* below);
   private:
    ActorVector children_;
    DISALLOW_COPY_AND_ASSIGN(ContainerActor);
  };

  class QuadActor : public NoClutterInterface::Actor {
   public:
    explicit QuadActor(NoClutterInterface* interface);
    void SetColor(const ClutterInterface::Color& color) {
      color_ = color;
    }

    virtual void AddToDisplayListImpl(NoClutterInterface::ActorVector* actors,
                                      bool opaque);
    virtual void Draw();

   protected:
    GLuint texture_;
    ClutterInterface::Color color_;

   private:
    DISALLOW_COPY_AND_ASSIGN(QuadActor);
  };

  class TexturePixmapActor : public NoClutterInterface::QuadActor,
                             public ClutterInterface::TexturePixmapActor {
   public:
    explicit TexturePixmapActor(NoClutterInterface* interface);
    virtual ~TexturePixmapActor() { Reset(); }
    bool SetTexturePixmapWindow(XWindow xid);
    bool IsUsingTexturePixmapExtension() { return true; }
    bool SetAlphaMask(const unsigned char* bytes, int width, int height) {
      NOTIMPLEMENTED();
      return true;
    }
    void ClearAlphaMask() { NOTIMPLEMENTED(); }
    void Refresh();
    void Reset();

    virtual void Draw();

   private:
    // Binds the window, the pixmap, the texture and the glx pixmap
    // together.
    bool Bind();

    // This is the XWindow that this actor is associated with.
    XWindow window_;

    // This is the compositing pixmap associated with the window.
    Pixmap pixmap_;

    // This is the GLX pixmap we draw into, created from the pixmap above.
    GLXPixmap glx_pixmap_;

    // This is the id of the damage region.
    XID damage_;

    DISALLOW_COPY_AND_ASSIGN(TexturePixmapActor);
  };

  class StageActor : public NoClutterInterface::ContainerActor,
                     public ClutterInterface::StageActor {
   public:
    StageActor(NoClutterInterface* interface, int width, int height);
    virtual ~StageActor();
    XWindow GetStageXWindow() { return window_;}
    void SetStageColor(const ClutterInterface::Color& color);
    virtual void Draw();
    virtual std::string GetDebugString() {
      NOTIMPLEMENTED();
      return "";
    }

   protected:
    virtual void SetSizeImpl(int width, int height);

   private:
    // This is the XWindow associated with the stage.  Owned by this class.
    XWindow window_;

    ClutterInterface::Color stage_color_;
    DISALLOW_COPY_AND_ASSIGN(StageActor);
  };

  NoClutterInterface(XConnection* x_connection, GLInterface* gl_interface);
  ~NoClutterInterface();

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
  GLuint vertex_buffer() { return vertex_buffer_; }

  void Draw();

 private:
  // These are friends so that they can get at the X connection and GL
  // interface.
  friend class StageActor;
  friend class TexturePixmapActor;

  XConnection* x_conn() const {
    return xconn_;
  }
  GLInterface* gl_interface() const {
    return gl_interface_;
  }

  // Returns the real current time, for updating animation time.
  AnimationBase::AnimationTime GetCurrentRealTime();

  // This draws a debugging "needle" in the upper left corner.
  void DrawNeedle();

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

  // This is the GL interface to use, and it is not owned.
  GLInterface* gl_interface_;

  // This is the 32-bit depth config that was found in the list of
  // visuals (if any).
  GLXFBConfig config_32_;

  // This is the 24-bit depth config that was found in the list of
  // visuals (if any).
  GLXFBConfig config_24_;

  // This is the current GLX context used for GL rendering.
  GLXContext context_;

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

  // This keeps track of the number of frames drawn so we can draw the
  // debugging needle.
  int num_frames_drawn_;

  // This is the vertex buffer that holds the rect we use for rendering stages.
  GLuint vertex_buffer_;

  DISALLOW_COPY_AND_ASSIGN(NoClutterInterface);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_NO_CLUTTER_H_
