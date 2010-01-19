// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdarg>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/no_clutter.h"
#include "window_manager/mock_gl_interface.h"
#include "window_manager/mock_x_connection.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class TestInterface : virtual public NoClutterInterface {
 public:
  TestInterface(XConnection* xconnection, GLInterface* gl_interface)
      : NoClutterInterface(xconnection, gl_interface) {}
};

class NoClutterTest : public ::testing::Test {
 public:
  NoClutterTest() : interface_(NULL), gl_interface_(new MockGLInterface),
                    x_connection_(new MockXConnection) {
    interface_ = new TestInterface(x_connection_, gl_interface_);
  }
  ~NoClutterTest() {
    delete interface_;
    delete gl_interface_;
    delete x_connection_;
  }
  NoClutterInterface* interface() { return interface_; }
 private:
  NoClutterInterface* interface_;
  GLInterface* gl_interface_;
  XConnection* x_connection_;
};

TEST_F(NoClutterTest, LayerDepth) {
  // Create an actor tree to test.
  ClutterInterface::StageActor* stage = interface()->GetDefaultStage();
  ClutterInterface::ContainerActor* group1 = interface()->CreateGroup();
  ClutterInterface::ContainerActor* group2 = interface()->CreateGroup();
  ClutterInterface::ContainerActor* group3 = interface()->CreateGroup();
  ClutterInterface::Actor* rect1 =
      interface()->CreateRectangle(ClutterInterface::Color(),
                                   ClutterInterface::Color(), 0);
  ClutterInterface::Actor* rect2 =
      interface()->CreateRectangle(ClutterInterface::Color(),
                                   ClutterInterface::Color(), 0);
  ClutterInterface::Actor* rect3 =
      interface()->CreateRectangle(ClutterInterface::Color(),
                                   ClutterInterface::Color(), 0);
  stage->AddActor(group1);
  group1->AddActor(group2);
  group2->AddActor(rect1);
  group2->AddActor(group3);
  group3->AddActor(rect2);
  group3->AddActor(rect3);

  // Test lower-level layer-setting routines
  int count = 0;
  dynamic_cast<NoClutterInterface::StageActor*>(stage)->Update(&count, 0LL);
  EXPECT_EQ(7, count);
  NoClutterInterface::ActorVector actors;
  // Start at a default depth range of -100 to 100.  Layers are
  // disributed evenly within that range.
  float depth = 100.0f;
  float thickness = -(100.0f - -100.0f)/count;
  dynamic_cast<NoClutterInterface::StageActor*>(stage)->ComputeDepth(
      &depth, thickness);
  EXPECT_FLOAT_EQ(-100.0f, depth);
  depth = 100.0f;  // Reset depth.
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::QuadActor*>(rect1)->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::QuadActor*>(rect2)->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::QuadActor*>(rect3)->z());

  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::ContainerActor*>(
                      group3)->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::ContainerActor*>(
                      group2)->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::ContainerActor*>(
                      group1)->z());

  // Test higher-level results.
  interface()->Draw();
  EXPECT_EQ(7, interface()->actor_count());

  // Code uses a depth range of -1 to 1.  Layers are
  // disributed evenly within that range.
  depth = 2047.0f;
  thickness = -1.0f;

  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::QuadActor*>(rect1)->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::QuadActor*>(rect2)->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(depth,
                  dynamic_cast<NoClutterInterface::QuadActor*>(rect3)->z());
}

TEST_F(NoClutterTest, FloatAnimation) {
  float value = -10.0f;
  NoClutterInterface::FloatAnimation anim(&value, 10.0f, 0, 20);
  EXPECT_FALSE(anim.Eval(0));
  EXPECT_FLOAT_EQ(-10.0f, value);
  EXPECT_FALSE(anim.Eval(5));
  EXPECT_FLOAT_EQ(-sqrt(50.0f), value);
  EXPECT_FALSE(anim.Eval(10));

  // The standard epsilon is just a little too small here..
  EXPECT_NEAR(0.0f, value, 1.0e-6);

  EXPECT_FALSE(anim.Eval(15));
  EXPECT_FLOAT_EQ(sqrt(50.0f), value);
  EXPECT_TRUE(anim.Eval(20));
  EXPECT_FLOAT_EQ(10.0f, value);
}

TEST_F(NoClutterTest, IntAnimation) {
  int value = -10;
  NoClutterInterface::IntAnimation anim(&value, 10, 0, 20);
  EXPECT_FALSE(anim.Eval(0));
  EXPECT_EQ(-10, value);
  EXPECT_FALSE(anim.Eval(5));
  EXPECT_EQ(-7, value);
  EXPECT_FALSE(anim.Eval(10));
  EXPECT_EQ(0, value);
  EXPECT_FALSE(anim.Eval(15));
  EXPECT_EQ(7, value);
  EXPECT_TRUE(anim.Eval(20));
  EXPECT_EQ(10, value);
}

}  // end namespace window_manager

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);
  CommandLine::Init(argc, argv);
  logging::InitLogging(NULL,
                       FLAGS_logtostderr ?
                         logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG :
                         logging::LOG_NONE,
                       logging::DONT_LOCK_LOG_FILE,
                       logging::APPEND_TO_OLD_LOG_FILE);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
