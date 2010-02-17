// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/clutter_interface.h"

#include "base/logging.h"
#include "chromeos/obsolete_logging.h"
#include "window_manager/util.h"
#include "window_manager/x_connection.h"

namespace window_manager {

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


bool MockClutterInterface::TexturePixmapActor::SetTexturePixmapWindow(
    XWindow xid) {
  xid_ = xid;
  return xconn_->RedirectWindowForCompositing(xid);
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
