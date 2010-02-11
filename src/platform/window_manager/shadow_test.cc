// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/shadow.h"
#include "window_manager/test_lib.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class ShadowTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    xconn_.reset(new MockXConnection);
    clutter_.reset(new MockClutterInterface(xconn_.get()));
  }

  scoped_ptr<MockXConnection> xconn_;
  scoped_ptr<MockClutterInterface> clutter_;
};

TEST_F(ShadowTest, Basic) {
  Shadow shadow(clutter_.get());
  int x = 10;
  int y = 20;
  int w = 200;
  int h = 100;

  shadow.Move(x, y, 0);
  shadow.Resize(w, h, 0);
  shadow.SetOpacity(0.75, 0);
  shadow.Show();

  // Check the group transform
  EXPECT_EQ(10, shadow.group_->GetX());
  EXPECT_EQ(20, shadow.group_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.group_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.group_->GetYScale());

  // Check the sides.
  EXPECT_EQ(0, shadow.top_actor_->GetX());
  EXPECT_EQ(1, shadow.top_actor_->GetY());
  EXPECT_FLOAT_EQ(200.0f, shadow.top_actor_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.top_actor_->GetYScale());

  EXPECT_EQ(0, shadow.bottom_actor_->GetX());
  EXPECT_EQ(100, shadow.bottom_actor_->GetY());
  EXPECT_FLOAT_EQ(200.0f, shadow.bottom_actor_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.bottom_actor_->GetYScale());

  EXPECT_EQ(1, shadow.left_actor_->GetX());
  EXPECT_EQ(0, shadow.left_actor_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.left_actor_->GetXScale());
  EXPECT_FLOAT_EQ(100.0f, shadow.left_actor_->GetYScale());

  EXPECT_EQ(200, shadow.right_actor_->GetX());
  EXPECT_EQ(0, shadow.right_actor_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.right_actor_->GetXScale());
  EXPECT_FLOAT_EQ(100.0f, shadow.right_actor_->GetYScale());

  // Check the corners.
  EXPECT_EQ(1, shadow.tl_actor_->GetX());
  EXPECT_EQ(1, shadow.tl_actor_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.tl_actor_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.tl_actor_->GetYScale());

  EXPECT_EQ(200, shadow.tr_actor_->GetX());
  EXPECT_EQ(1, shadow.tr_actor_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.tr_actor_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.tr_actor_->GetYScale());

  EXPECT_EQ(200, shadow.br_actor_->GetX());
  EXPECT_EQ(100, shadow.br_actor_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.br_actor_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.br_actor_->GetYScale());

  EXPECT_EQ(1, shadow.bl_actor_->GetX());
  EXPECT_EQ(100, shadow.bl_actor_->GetY());
  EXPECT_FLOAT_EQ(1.0f, shadow.bl_actor_->GetXScale());
  EXPECT_FLOAT_EQ(1.0f, shadow.bl_actor_->GetYScale());
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
