// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/shadow.h"

#include <cmath>
#include <gflags/gflags.h>

#include "chromeos/obsolete_logging.h"

DEFINE_string(shadow_image_dir, "../assets/images",
              "Path to directory containing shadow images");

namespace chromeos {

// Static members.
ClutterInterface::Actor* Shadow::top_texture_    = NULL;
ClutterInterface::Actor* Shadow::bottom_texture_ = NULL;
ClutterInterface::Actor* Shadow::left_texture_   = NULL;
ClutterInterface::Actor* Shadow::right_texture_  = NULL;
ClutterInterface::Actor* Shadow::tl_texture_     = NULL;
ClutterInterface::Actor* Shadow::tr_texture_     = NULL;
ClutterInterface::Actor* Shadow::bl_texture_     = NULL;
ClutterInterface::Actor* Shadow::br_texture_     = NULL;

int Shadow::kInset = 0;
int Shadow::kTopHeight = 0;
int Shadow::kBottomHeight = 0;
int Shadow::kLeftWidth = 0;
int Shadow::kRightWidth = 0;

Shadow::Shadow(ClutterInterface* clutter)
    : clutter_(clutter),
      is_shown_(false),
      opacity_(1.0) {
  CHECK(clutter_);
  group_.reset(clutter_->CreateGroup());
  group_->SetName("shadow group");

  // Load the images the first time we get called.
  // TODO: Get a proper singleton class and use that instead.
  if (!top_texture_)
    Init();

  top_actor_.reset(clutter_->CloneActor(top_texture_));
  bottom_actor_.reset(clutter_->CloneActor(bottom_texture_));
  left_actor_.reset(clutter_->CloneActor(left_texture_));
  right_actor_.reset(clutter_->CloneActor(right_texture_));
  tl_actor_.reset(clutter_->CloneActor(tl_texture_));
  tr_actor_.reset(clutter_->CloneActor(tr_texture_));
  bl_actor_.reset(clutter_->CloneActor(bl_texture_));
  br_actor_.reset(clutter_->CloneActor(br_texture_));

  top_actor_->SetName("shadow top");
  bottom_actor_->SetName("shadow bottom");
  left_actor_->SetName("shadow left");
  right_actor_->SetName("shadow right");
  tl_actor_->SetName("shadow tl");
  tr_actor_->SetName("shadow tr");
  bl_actor_->SetName("shadow bl");
  br_actor_->SetName("shadow br");

  // Resize the shadow arbitrarily to initialize the positions of the actors.
  Resize(10, 10, 0);
  SetOpacity(1.0, 0);
  group_->AddActor(top_actor_.get());
  group_->AddActor(bottom_actor_.get());
  group_->AddActor(left_actor_.get());
  group_->AddActor(right_actor_.get());
  group_->AddActor(tl_actor_.get());
  group_->AddActor(tr_actor_.get());
  group_->AddActor(bl_actor_.get());
  group_->AddActor(br_actor_.get());
  Hide();
}

void Shadow::Show() {
  is_shown_ = true;
  group_->SetVisibility(true);
}

void Shadow::Hide() {
  is_shown_ = false;
  group_->SetVisibility(false);
}

void Shadow::Move(int x, int y, int anim_ms) {
  group_->Move(x, y, anim_ms);
}

void Shadow::MoveX(int x, int anim_ms) {
  group_->MoveX(x, anim_ms);
}

void Shadow::MoveY(int y, int anim_ms) {
  group_->MoveY(y, anim_ms);
}

void Shadow::Resize(int width, int height, int anim_ms) {
  top_actor_->Move(kInset, -kTopHeight, anim_ms);
  bottom_actor_->Move(kInset, height, anim_ms);
  left_actor_->Move(-kLeftWidth, kInset, anim_ms);
  right_actor_->Move(width, kInset, anim_ms);

  tl_actor_->Move(-kLeftWidth, -kTopHeight, anim_ms);
  tr_actor_->Move(width - kInset, -kTopHeight, anim_ms);
  bl_actor_->Move(-kLeftWidth, height - kInset, anim_ms);
  br_actor_->Move(width - kInset, height - kInset, anim_ms);

  // TODO: Figure out what to do for windows that are too small for these
  // images -- currently, we get a Clutter error as we try to scale them to
  // negative values.
  top_actor_->Scale(width - 2 * kInset, 1.0, anim_ms);
  bottom_actor_->Scale(width - 2 * kInset, 1.0, anim_ms);
  left_actor_->Scale(1.0, height - 2 * kInset, anim_ms);
  right_actor_->Scale(1.0, height - 2 * kInset, anim_ms);
}

void Shadow::SetOpacity(double opacity, int anim_ms) {
  opacity_ = opacity;
  group_->SetOpacity(opacity, anim_ms);
}

void Shadow::Init() {
  CHECK(!top_texture_) << "Was Init() already called?";
  top_texture_    = InitTexture("shadow_top.png");
  bottom_texture_ = InitTexture("shadow_bottom.png");
  left_texture_   = InitTexture("shadow_left.png");
  right_texture_  = InitTexture("shadow_right.png");
  tl_texture_     = InitTexture("shadow_tl.png");
  tr_texture_     = InitTexture("shadow_tr.png");
  bl_texture_     = InitTexture("shadow_bl.png");
  br_texture_     = InitTexture("shadow_br.png");

  kTopHeight    = top_texture_->GetHeight();
  kBottomHeight = bottom_texture_->GetHeight();
  kLeftWidth    = left_texture_->GetWidth();
  kRightWidth   = right_texture_->GetWidth();

  kInset = tl_texture_->GetHeight() - kTopHeight;
  CHECK_EQ(br_texture_->GetHeight() - kBottomHeight, kInset);
  CHECK_EQ(tl_texture_->GetWidth() - kLeftWidth, kInset);
  CHECK_EQ(tr_texture_->GetWidth() - kRightWidth, kInset);
}

ClutterInterface::Actor* Shadow::InitTexture(const std::string& filename) {
  ClutterInterface::Actor* actor = clutter_->CreateImage(
      FLAGS_shadow_image_dir + "/" + filename);
  actor->SetName(filename);
  // Even though we don't actually want to display it, we need to add the
  // actor to the default stage; otherwise Clutter complains that actors
  // that are cloned from it are unmappable.
  actor->SetVisibility(false);
  clutter_->GetDefaultStage()->AddActor(actor);
  return actor;
}

}  // namespace chromeos
