// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/clutter_interface.h"

#include <cmath>

#include "base/string_util.h"
#include "chromeos/obsolete_logging.h"
#include "window_manager/util.h"

namespace window_manager {

const int RealClutterInterface::TexturePixmapActor::kAlphaMaskLayerIndex = 1;


RealClutterInterface::Actor::Actor(ClutterActor* clutter_actor)
    : clutter_actor_(clutter_actor) {
  CHECK(clutter_actor_);
  // Tell GLib to set 'clutter_actor_' to NULL when the ClutterActor object
  // is destroyed.
  g_object_add_weak_pointer(
      G_OBJECT(clutter_actor), reinterpret_cast<gpointer*>(&clutter_actor_));
}

RealClutterInterface::Actor::~Actor() {
  if (clutter_actor_) {
    clutter_actor_destroy(clutter_actor_);
    clutter_actor_ = NULL;
  }
}

void RealClutterInterface::Actor::SetName(const std::string& name) {
  CHECK(clutter_actor_);
  clutter_actor_set_name(clutter_actor_, name.c_str());
}

int RealClutterInterface::Actor::GetWidth() {
  CHECK(clutter_actor_);
  return clutter_actor_get_width(clutter_actor_);
}

int RealClutterInterface::Actor::GetHeight() {
  CHECK(clutter_actor_);
  return clutter_actor_get_height(clutter_actor_);
}

void RealClutterInterface::Actor::SetVisibility(bool visible) {
  CHECK(clutter_actor_);
  if (visible) {
    clutter_actor_show_all(clutter_actor_);
  } else {
    clutter_actor_hide_all(clutter_actor_);
  }
}

void RealClutterInterface::Actor::SetSize(int width, int height) {
  CHECK(clutter_actor_);
  clutter_actor_set_size(clutter_actor_, width, height);
}

void RealClutterInterface::Actor::Move(int x, int y, int anim_ms) {
  CHECK(clutter_actor_);
  if (anim_ms <= 0) {
    // Clutter doesn't like getting 0-ms animation durations.
    clutter_actor_set_position(clutter_actor_, x, y);
  } else {
    clutter_actor_animate(clutter_actor_, CLUTTER_EASE_IN_OUT_SINE, anim_ms,
#ifdef CLUTTER_0_9_2
                          "x", x,
                          "y", y,
#else
                          "x", static_cast<double>(x),
                          "y", static_cast<double>(y),
#endif
                          NULL);
  }
}

void RealClutterInterface::Actor::MoveX(int x, int anim_ms) {
  CHECK(clutter_actor_);
  if (anim_ms <= 0) {
    // Clutter doesn't like getting 0-ms animation durations.
    clutter_actor_set_x(clutter_actor_, x);
  } else {
    clutter_actor_animate(clutter_actor_, CLUTTER_EASE_IN_OUT_SINE, anim_ms,
#ifdef CLUTTER_0_9_2
                          "x", x,
#else
                          "x", static_cast<double>(x),
#endif
                          NULL);
  }
}

void RealClutterInterface::Actor::MoveY(int y, int anim_ms) {
  CHECK(clutter_actor_);
  if (anim_ms <= 0) {
    // Clutter doesn't like getting 0-ms animation durations.
    clutter_actor_set_y(clutter_actor_, y);
  } else {
    clutter_actor_animate(clutter_actor_, CLUTTER_EASE_IN_OUT_SINE, anim_ms,
#ifdef CLUTTER_0_9_2
                          "y", y,
#else
                          "y", static_cast<double>(y),
#endif
                          NULL);
  }
}

void RealClutterInterface::Actor::Scale(
    double scale_x, double scale_y, int anim_ms) {
  CHECK(clutter_actor_);
  if (anim_ms <= 0) {
    clutter_actor_set_scale(clutter_actor_, scale_x, scale_y);
  } else {
    clutter_actor_animate(clutter_actor_, CLUTTER_EASE_IN_OUT_SINE, anim_ms,
                          "scale-x", scale_x,
                          "scale-y", scale_y,
                          NULL);
  }
}

void RealClutterInterface::Actor::SetOpacity(double opacity, int anim_ms) {
  CHECK(clutter_actor_);
  const int clutter_opacity = static_cast<int>(opacity * 255);
  if (anim_ms <= 0) {
    clutter_actor_set_opacity(clutter_actor_, clutter_opacity);
  } else {
    clutter_actor_animate(clutter_actor_, CLUTTER_EASE_IN_OUT_SINE, anim_ms,
                          "opacity", clutter_opacity,
                          NULL);
  }
}

void RealClutterInterface::Actor::SetClip(int x, int y, int width, int height) {
  CHECK(clutter_actor_);
  clutter_actor_set_clip(clutter_actor_, x, y, width, height);
}

void RealClutterInterface::Actor::Raise(ClutterInterface::Actor* other) {
  CHECK(clutter_actor_);
  CHECK(other);
  RealClutterInterface::Actor* cast_other =
      dynamic_cast<RealClutterInterface::Actor*>(other);
  CHECK(cast_other);
  clutter_actor_raise(clutter_actor_, cast_other->clutter_actor_);
}

void RealClutterInterface::Actor::Lower(ClutterInterface::Actor* other) {
  CHECK(clutter_actor_);
  CHECK(other);
  RealClutterInterface::Actor* cast_other =
      dynamic_cast<RealClutterInterface::Actor*>(other);
  CHECK(cast_other);
  clutter_actor_lower(clutter_actor_, cast_other->clutter_actor_);
}

void RealClutterInterface::Actor::RaiseToTop() {
  CHECK(clutter_actor_);
  clutter_actor_raise_top(clutter_actor_);
}

void RealClutterInterface::Actor::LowerToBottom() {
  CHECK(clutter_actor_);
  clutter_actor_lower_bottom(clutter_actor_);
}


void RealClutterInterface::ContainerActor::AddActor(
    ClutterInterface::Actor* actor) {
  CHECK(clutter_actor_);
  CHECK(actor);
  RealClutterInterface::Actor* cast_actor =
      dynamic_cast<RealClutterInterface::Actor*>(actor);
  CHECK(cast_actor);
  clutter_container_add_actor(
      CLUTTER_CONTAINER(clutter_actor_), cast_actor->clutter_actor());
}


XWindow RealClutterInterface::StageActor::GetStageXWindow() {
  CHECK(clutter_actor_);
  return clutter_x11_get_stage_window(CLUTTER_STAGE(clutter_actor_));
}

void RealClutterInterface::StageActor::SetStageColor(
    const ClutterInterface::Color &color) {
  CHECK(clutter_actor_);
  ClutterColor clutter_color = ConvertColor(color);
  clutter_stage_set_color(CLUTTER_STAGE(clutter_actor_), &clutter_color);
}

std::string RealClutterInterface::StageActor::GetDebugString() {
  return GetDebugStringInternal(clutter_actor_, 0);
}

// static
std::string RealClutterInterface::StageActor::GetDebugStringInternal(
    ClutterActor* actor, int indent_level) {
  std::string out;
  for (int i = 0; i < indent_level; ++i)
    out += "  ";

  GType type = G_TYPE_FROM_INSTANCE(G_OBJECT(actor));
  const char* name = clutter_actor_get_name(actor);
  const char* type_name = g_type_name(type);
  gdouble scale_x = 0, scale_y = 0;
  clutter_actor_get_scale(actor, &scale_x, &scale_y);
  out += StringPrintf("\"%s\" %p (%s%s) (%d, %d) %dx%d "
                        "scale=(%.2f, %.2f) %.f%%\n",
                      name ? name : "",
                      actor,
                      (CLUTTER_ACTOR_IS_VISIBLE(actor) ? "" : "inv "),
                      type_name,
                      static_cast<int>(clutter_actor_get_x(actor)),
                      static_cast<int>(clutter_actor_get_y(actor)),
                      static_cast<int>(clutter_actor_get_width(actor)),
                      static_cast<int>(clutter_actor_get_height(actor)),
                      scale_x, scale_y,
                      round((clutter_actor_get_opacity(actor) / 255.0) * 100));

  if (g_type_is_a(type, CLUTTER_TYPE_CONTAINER)) {
    GList* children = clutter_container_get_children(CLUTTER_CONTAINER(actor));
    for (GList* node = g_list_first(children);
         node != NULL;
         node = g_list_next(node)) {
      out += GetDebugStringInternal(CLUTTER_ACTOR(node->data),
                                    indent_level + 1);
    }
  }

  return out;
}


RealClutterInterface::TexturePixmapActor::~TexturePixmapActor() {
  ClearAlphaMask();
}

bool RealClutterInterface::TexturePixmapActor::SetTexturePixmapWindow(
    XWindow xid) {
  CHECK(clutter_actor_);
  CHECK_NE(xid, None);

  // Trap errors in case the X window goes away underneath us.
  clutter_x11_trap_x_errors();

  // The final 'automatic' param here is unrelated to the 'automatic' in
  // the next statement -- this one corresponds to
  // XCompositeRedirectWindow()'s 'update' param (and I think it's
  // effectively a no-op, since we're already redirecting the window
  // ourselves).
  clutter_x11_texture_pixmap_set_window(
      CLUTTER_X11_TEXTURE_PIXMAP(clutter_actor_), xid, FALSE);

  // Automatically update texture from pixmap when damage events are
  // received.
  clutter_x11_texture_pixmap_set_automatic(
      CLUTTER_X11_TEXTURE_PIXMAP(clutter_actor_), TRUE);

  if (clutter_x11_untrap_x_errors()) {
    LOG(WARNING) << "Got X error while making texture pixmap use window "
                 << XidStr(xid);
    return false;
  }

  return true;
}

bool RealClutterInterface::TexturePixmapActor::IsUsingTexturePixmapExtension() {
  CHECK(clutter_actor_);
#if __arm__
  return clutter_eglx_egl_image_using_extension(
      CLUTTER_EGLX_EGL_IMAGE(clutter_actor_));
#else  // x86
  return clutter_glx_texture_pixmap_using_extension(
      CLUTTER_GLX_TEXTURE_PIXMAP(clutter_actor_));
#endif
}

bool RealClutterInterface::TexturePixmapActor::SetAlphaMask(
    const unsigned char* bytes, int width, int height) {
  CHECK(clutter_actor_);

  ClearAlphaMask();

  alpha_mask_texture_ = cogl_texture_new_from_data(
      width, height,
#ifdef CLUTTER_0_9_2
      0,                      // max_waste
#endif
      COGL_TEXTURE_NONE,      // flags
      COGL_PIXEL_FORMAT_A_8,  // format in memory
      COGL_PIXEL_FORMAT_A_8,  // format in GPU
      width,                  // rowstride
      bytes);
  if (alpha_mask_texture_ == COGL_INVALID_HANDLE) {
    LOG(WARNING) << "Unable to create COGL texture";
    return false;
  }

  CoglHandle material = clutter_texture_get_cogl_material(
      CLUTTER_TEXTURE(clutter_actor_));
  CHECK(material != COGL_INVALID_HANDLE);

#ifndef CLUTTER_0_9_2  // more-recent versions
  GError* error = NULL;
  cogl_material_set_layer_combine(
      material,
      kAlphaMaskLayerIndex,
      "RGB = MODULATE(PREVIOUS, TEXTURE[A]) A = MODULATE(PREVIOUS, TEXTURE)",
      &error);
  if (error) {
    LOG(ERROR) << "Got error when adding alpha mask layer to material: "
               << error->message;
    g_error_free(error);
    cogl_handle_unref(alpha_mask_texture_);
    alpha_mask_texture_ = COGL_INVALID_HANDLE;
    return false;
  }
#else  // CLUTTER_0_9_2
  cogl_material_set_layer_combine_function(
      material, kAlphaMaskLayerIndex,
      COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA,
      COGL_MATERIAL_LAYER_COMBINE_FUNC_MODULATE);
  cogl_material_set_layer_combine_arg_src(
      material, kAlphaMaskLayerIndex, 0,
      COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA,
      COGL_MATERIAL_LAYER_COMBINE_SRC_PREVIOUS);
  cogl_material_set_layer_combine_arg_op(
      material, kAlphaMaskLayerIndex, 0,
      COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGB,
      COGL_MATERIAL_LAYER_COMBINE_OP_SRC_COLOR);
  cogl_material_set_layer_combine_arg_op(
      material, kAlphaMaskLayerIndex, 0,
      COGL_MATERIAL_LAYER_COMBINE_CHANNELS_ALPHA,
      COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA);
  cogl_material_set_layer_combine_arg_src(
      material, kAlphaMaskLayerIndex, 1,
      COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA,
      COGL_MATERIAL_LAYER_COMBINE_SRC_TEXTURE);
  cogl_material_set_layer_combine_arg_op(
      material, kAlphaMaskLayerIndex, 1,
      COGL_MATERIAL_LAYER_COMBINE_CHANNELS_RGBA,
      COGL_MATERIAL_LAYER_COMBINE_OP_SRC_ALPHA);
#endif

  cogl_material_set_layer(material,
                          kAlphaMaskLayerIndex,
                          alpha_mask_texture_);
  return true;
}

void RealClutterInterface::TexturePixmapActor::ClearAlphaMask() {
  if (alpha_mask_texture_ == COGL_INVALID_HANDLE)
    return;

  CoglHandle material = clutter_texture_get_cogl_material(
      CLUTTER_TEXTURE(clutter_actor_));
  CHECK(material != COGL_INVALID_HANDLE);

  cogl_material_remove_layer(material, kAlphaMaskLayerIndex);
  cogl_handle_unref(alpha_mask_texture_);
  alpha_mask_texture_ = COGL_INVALID_HANDLE;
}


RealClutterInterface::RealClutterInterface()
    : default_stage_(new StageActor(clutter_stage_get_default())) {
}

RealClutterInterface::~RealClutterInterface() {
  default_stage_.reset();
}

RealClutterInterface::ContainerActor* RealClutterInterface::CreateGroup() {
  return new ContainerActor(clutter_group_new());
}

RealClutterInterface::Actor* RealClutterInterface::CreateRectangle(
    const ClutterInterface::Color& color,
    const ClutterInterface::Color& border_color,
    int border_width) {
  ClutterColor clutter_color = ConvertColor(color);
  ClutterColor clutter_border_color = ConvertColor(border_color);

  ClutterActor* clutter_actor =
      clutter_rectangle_new_with_color(&clutter_color);
  clutter_rectangle_set_border_color(
      CLUTTER_RECTANGLE(clutter_actor), &clutter_border_color);
  clutter_rectangle_set_border_width(
      CLUTTER_RECTANGLE(clutter_actor), border_width);
  return new Actor(clutter_actor);
}

RealClutterInterface::Actor* RealClutterInterface::CreateImage(
    const std::string& filename) {
  GError* error = NULL;
  ClutterActor* clutter_actor =
      clutter_texture_new_from_file(filename.c_str(), &error);
  CHECK(!error) << "Got error when creating texture from " << filename << ": "
                << error->message;
  CHECK(clutter_actor);
  return new Actor(clutter_actor);
}

RealClutterInterface::TexturePixmapActor*
RealClutterInterface::CreateTexturePixmap() {
  ClutterActor* clutter_actor = NULL;
#if __arm__
  clutter_actor = clutter_eglx_egl_image_new();
#else  // x86
  clutter_actor = clutter_glx_texture_pixmap_new();
#endif
  return new TexturePixmapActor(clutter_actor);
}

RealClutterInterface::Actor* RealClutterInterface::CreateText(
    const std::string& font_name,
    const std::string& text,
    const ClutterInterface::Color& color) {
  ClutterColor clutter_color = ConvertColor(color);
  return new Actor(
      clutter_text_new_full(font_name.c_str(), text.c_str(), &clutter_color));
}

RealClutterInterface::Actor* RealClutterInterface::CloneActor(
    ClutterInterface::Actor* orig) {
  CHECK(orig);
  RealClutterInterface::Actor* cast_orig =
      dynamic_cast<RealClutterInterface::Actor*>(orig);
  CHECK(cast_orig);
  CHECK(cast_orig->clutter_actor());
  ClutterActor* clutter_actor = clutter_clone_new(cast_orig->clutter_actor());
  return new Actor(clutter_actor);
}

// static
ClutterColor RealClutterInterface::ConvertColor(
    const ClutterInterface::Color& color) {
  ClutterColor clutter_color;
  clutter_color.red = static_cast<guint8>(color.red * 255.0f + 0.5f);
  clutter_color.green = static_cast<guint8>(color.green * 255.0f + 0.5f);
  clutter_color.blue = static_cast<guint8>(color.blue * 255.0f + 0.5f);
  clutter_color.alpha = 0xff;
  return clutter_color;
}


MockClutterInterface::Actor::~Actor() {
  if (parent_) {
    parent_->stacked_children()->Remove(this);
    parent_ = NULL;
  }
}

void MockClutterInterface::Actor::Raise(ClutterInterface::Actor* other) {
  CHECK(parent_);
  CHECK(other);
  MockClutterInterface::Actor* cast_other =
      dynamic_cast<MockClutterInterface::Actor*>(other);
  CHECK(cast_other);
  CHECK(parent_->stacked_children()->Contains(this));
  CHECK(parent_->stacked_children()->Contains(cast_other));
  parent_->stacked_children()->Remove(this);
  parent_->stacked_children()->AddAbove(this, cast_other);
}

void MockClutterInterface::Actor::Lower(ClutterInterface::Actor* other) {
  CHECK(parent_);
  CHECK(other);
  MockClutterInterface::Actor* cast_other =
      dynamic_cast<MockClutterInterface::Actor*>(other);
  CHECK(cast_other);
  CHECK(parent_->stacked_children()->Contains(this));
  CHECK(parent_->stacked_children()->Contains(cast_other));
  parent_->stacked_children()->Remove(this);
  parent_->stacked_children()->AddBelow(this, cast_other);
}

void MockClutterInterface::Actor::RaiseToTop() {
  CHECK(parent_);
  CHECK(parent_->stacked_children()->Contains(this));
  parent_->stacked_children()->Remove(this);
  parent_->stacked_children()->AddOnTop(this);
}

void MockClutterInterface::Actor::LowerToBottom() {
  CHECK(parent_);
  CHECK(parent_->stacked_children()->Contains(this));
  parent_->stacked_children()->Remove(this);
  parent_->stacked_children()->AddOnBottom(this);
}


MockClutterInterface::ContainerActor::ContainerActor()
    : stacked_children_(new Stacker<Actor*>) {
}

MockClutterInterface::ContainerActor::~ContainerActor() {
  typedef std::list<Actor*>::const_iterator iterator;

  for (iterator it = stacked_children_->items().begin();
       it != stacked_children_->items().end(); ++it) {
    (*it)->set_parent(NULL);
  }
}

void MockClutterInterface::ContainerActor::AddActor(
    ClutterInterface::Actor* actor) {
  MockClutterInterface::Actor* cast_actor =
      dynamic_cast<MockClutterInterface::Actor*>(actor);
  CHECK(cast_actor);
  CHECK_EQ(cast_actor->parent(), static_cast<ContainerActor*>(NULL));
  cast_actor->set_parent(this);
  CHECK(!stacked_children_->Contains(cast_actor));
  stacked_children_->AddOnBottom(cast_actor);
}

int MockClutterInterface::ContainerActor::GetStackingIndex(
    ClutterInterface::Actor* actor) {
  CHECK(actor);
  MockClutterInterface::Actor* cast_actor =
      dynamic_cast<MockClutterInterface::Actor*>(actor);
  CHECK(cast_actor);
  return stacked_children_->GetIndex(cast_actor);
}


bool MockClutterInterface::TexturePixmapActor::SetAlphaMask(
    const unsigned char* bytes, int width, int height) {
  ClearAlphaMask();
  size_t size = width * height;
  alpha_mask_bytes_ = new unsigned char[size];
  memcpy(alpha_mask_bytes_, bytes, size);
  return true;
}

void MockClutterInterface::TexturePixmapActor::ClearAlphaMask() {
  delete[] alpha_mask_bytes_;
  alpha_mask_bytes_ = NULL;
}

}  // namespace window_manager
