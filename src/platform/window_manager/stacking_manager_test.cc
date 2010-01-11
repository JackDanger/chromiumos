// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"
#include "window_manager/shadow.h"
#include "window_manager/stacking_manager.h"
#include "window_manager/window.h"
#include "window_manager/window_manager.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class StackingManagerTest : public BasicWindowManagerTest {
 protected:
  virtual void SetUp() {
    BasicWindowManagerTest::SetUp();
    stacking_manager_ = wm_->stacking_manager();
  }

  StackingManager* stacking_manager_;  // points at wm_'s object
};

TEST_F(StackingManagerTest, StackXidAtTopOfLayer) {
  // Create two windows.
  XWindow xid = CreateSimpleWindow();
  XWindow xid2 = CreateSimpleWindow();

  // Tell the stacking manager to stack them in different layers and then
  // make sure that they were restacked correctly.
  EXPECT_TRUE(
      stacking_manager_->StackXidAtTopOfLayer(
          xid, StackingManager::LAYER_TOPLEVEL_WINDOW));
  EXPECT_TRUE(
      stacking_manager_->StackXidAtTopOfLayer(
          xid2, StackingManager::LAYER_EXPANDED_PANEL));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(xid2),
            xconn_->stacked_xids().GetIndex(xid));

  // Now move the lower window to the top of the other window's layer.
  EXPECT_TRUE(
      stacking_manager_->StackXidAtTopOfLayer(
          xid, StackingManager::LAYER_EXPANDED_PANEL));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(xid),
            xconn_->stacked_xids().GetIndex(xid2));
}

TEST_F(StackingManagerTest, StackActorAtTopOfLayer) {
  // Create two actors and add them to the stage.
  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  scoped_ptr<MockClutterInterface::Actor> actor(clutter_->CreateGroup());
  stage->AddActor(actor.get());
  scoped_ptr<MockClutterInterface::Actor> actor2(clutter_->CreateGroup());
  stage->AddActor(actor2.get());

  // Check that the actors get stacked correctly.
  stacking_manager_->StackActorAtTopOfLayer(
      actor.get(), StackingManager::LAYER_BACKGROUND);
  stacking_manager_->StackActorAtTopOfLayer(
      actor2.get(), StackingManager::LAYER_TOPLEVEL_WINDOW);
  EXPECT_LT(stage->GetStackingIndex(actor2.get()),
            stage->GetStackingIndex(actor.get()));

  // Now restack them.
  stacking_manager_->StackActorAtTopOfLayer(
      actor.get(), StackingManager::LAYER_TOPLEVEL_WINDOW);
  EXPECT_LT(stage->GetStackingIndex(actor.get()),
            stage->GetStackingIndex(actor2.get()));
}

TEST_F(StackingManagerTest, StackWindowAtTopOfLayer) {
  // Create two windows.
  XWindow xid = CreateSimpleWindow();
  Window win(wm_.get(), xid, false);
  XWindow xid2 = CreateSimpleWindow();
  Window win2(wm_.get(), xid2, false);

  // Stack both of the windows in the same layer and make sure that their
  // relative positions are correct.
  EXPECT_TRUE(stacking_manager_->StackWindowAtTopOfLayer(
      &win, StackingManager::LAYER_TOPLEVEL_WINDOW));
  EXPECT_TRUE(stacking_manager_->StackWindowAtTopOfLayer(
      &win2, StackingManager::LAYER_TOPLEVEL_WINDOW));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(xid2),
            xconn_->stacked_xids().GetIndex(xid));

  // Their actors should've been restacked as well, and the shadows should
  // be stacked at the bottom of the layer, beneath both windows.
  MockClutterInterface::StageActor* stage = clutter_->GetDefaultStage();
  EXPECT_LT(stage->GetStackingIndex(win2.actor()),
            stage->GetStackingIndex(win.actor()));
  EXPECT_LT(stage->GetStackingIndex(win.actor()),
            stage->GetStackingIndex(win2.shadow()->group()));
  EXPECT_LT(stage->GetStackingIndex(win2.actor()),
            stage->GetStackingIndex(win.shadow()->group()));

  // Now stack the first window on a higher layer.  Their client windows
  // should be restacked as expected, and the first window's shadow should
  // be stacked above the second window.
  EXPECT_TRUE(stacking_manager_->StackWindowAtTopOfLayer(
      &win, StackingManager::LAYER_COLLAPSED_PANEL));
  EXPECT_LT(xconn_->stacked_xids().GetIndex(xid),
            xconn_->stacked_xids().GetIndex(xid2));
  EXPECT_LT(stage->GetStackingIndex(win.actor()),
            stage->GetStackingIndex(win2.actor()));
  EXPECT_LT(stage->GetStackingIndex(win.actor()),
            stage->GetStackingIndex(win.shadow()->group()));
  EXPECT_LT(stage->GetStackingIndex(win.shadow()->group()),
            stage->GetStackingIndex(win2.actor()));
  EXPECT_LT(stage->GetStackingIndex(win2.actor()),
            stage->GetStackingIndex(win2.shadow()->group()));
}

}  // namespace window_manager

int main(int argc, char** argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
