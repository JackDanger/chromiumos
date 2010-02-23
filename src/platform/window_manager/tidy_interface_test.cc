// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <set>
#include <string>
#include <vector>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/compositor_event_source.h"
#include "window_manager/mock_gl_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/test_lib.h"
#include "window_manager/tidy_interface.h"
#include "window_manager/util.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

using std::set;
using std::string;
using std::vector;

namespace window_manager {

class TestInterface : virtual public TidyInterface {
 public:
  TestInterface(XConnection* xconnection, GLInterface* gl_interface)
      : TidyInterface(xconnection, gl_interface) {}
 private:
  DISALLOW_COPY_AND_ASSIGN(TestInterface);
};

class NameCheckVisitor : virtual public TidyInterface::ActorVisitor {
 public:
  NameCheckVisitor() {}
  virtual ~NameCheckVisitor() {}
  virtual void VisitActor(TidyInterface::Actor* actor) {
    results_.push_back(actor->name());
  }
  const vector<string>& results() { return results_; }
 private:
  vector<string> results_;
  DISALLOW_COPY_AND_ASSIGN(NameCheckVisitor);
};

struct TestCompositorEventSource : public CompositorEventSource {
  void StartSendingEventsForWindowToCompositor(XWindow xid) {
    tracked_xids.insert(xid);
  }
  void StopSendingEventsForWindowToCompositor(XWindow xid) {
    tracked_xids.erase(xid);
  }
  set<XWindow> tracked_xids;
};

class TidyTest : public ::testing::Test {
 public:
  TidyTest()
      : interface_(NULL),
        gl_interface_(new MockGLInterface),
        x_connection_(new MockXConnection),
        event_source_(new TestCompositorEventSource) {
    interface_.reset(new TestInterface(x_connection_.get(),
                                       gl_interface_.get()));
    interface_->SetEventSource(event_source_.get());
  }
  virtual ~TidyTest() {
    interface_.reset(NULL);  // Must explicitly delete so that we get
                             // the order right.
  }

  TidyInterface* interface() { return interface_.get(); }
  MockXConnection* x_connection() { return x_connection_.get(); }
  TestCompositorEventSource* event_source() { return event_source_.get(); }

 private:
  scoped_ptr<TidyInterface> interface_;
  scoped_ptr<GLInterface> gl_interface_;
  scoped_ptr<MockXConnection> x_connection_;
  scoped_ptr<TestCompositorEventSource> event_source_;
};

class TidyTestTree : public TidyTest {
 public:
  TidyTestTree() {}
  virtual ~TidyTestTree() {}
  void SetUp() {
    // Create an actor tree to test.
    stage_ = interface()->GetDefaultStage();
    group1_.reset(interface()->CreateGroup());
    group2_.reset(interface()->CreateGroup());
    group3_.reset(interface()->CreateGroup());
    group4_.reset(interface()->CreateGroup());
    rect1_.reset(interface()->CreateRectangle(ClutterInterface::Color(),
                                              ClutterInterface::Color(), 0));
    rect2_.reset(interface()->CreateRectangle(ClutterInterface::Color(),
                                              ClutterInterface::Color(), 0));
    rect3_.reset(interface()->CreateRectangle(ClutterInterface::Color(),
                                              ClutterInterface::Color(), 0));
    stage_->SetName("stage");
    group1_->SetName("group1");
    group2_->SetName("group2");
    group3_->SetName("group3");
    group4_->SetName("group4");
    rect1_->SetName("rect1");
    rect2_->SetName("rect2");
    rect3_->SetName("rect3");

    //     stage (0)
    //     |          |
    // group1(256)  group3(1024)
    //    |            |
    // group2(512)    group4(1280)
    //   |              |      |
    // rect1(768)  rect2(1536) rect3(1792)

    // depth order (furthest to nearest) should be:
    // rect3 = 1792
    // rect2 = 1536
    // group4 = 1280
    // group3 = 1024
    // rect1 = 768
    // group2 = 512
    // group1 = 256
    // stage = 0

    stage_->AddActor(group1_.get());
    stage_->AddActor(group3_.get());
    group1_->AddActor(group2_.get());
    group2_->AddActor(rect1_.get());
    group3_->AddActor(group4_.get());
    group4_->AddActor(rect2_.get());
    group4_->AddActor(rect3_.get());
  }

  void TearDown() {
    // This is in reverse order of creation on purpose...
    rect3_.reset(NULL);
    rect2_.reset(NULL);
    group4_.reset(NULL);
    rect1_.reset(NULL);
    group2_.reset(NULL);
    group3_.reset(NULL);
    group1_.reset(NULL);
    stage_ = NULL;
  }
 protected:
  TidyInterface::StageActor* stage_;
  scoped_ptr<TidyInterface::ContainerActor> group1_;
  scoped_ptr<TidyInterface::ContainerActor> group2_;
  scoped_ptr<TidyInterface::ContainerActor> group3_;
  scoped_ptr<TidyInterface::ContainerActor> group4_;
  scoped_ptr<TidyInterface::Actor> rect1_;
  scoped_ptr<TidyInterface::Actor> rect2_;
  scoped_ptr<TidyInterface::Actor> rect3_;
};

TEST_F(TidyTestTree, LayerDepth) {
  // Test lower-level layer-setting routines
  int32 count = 0;
  stage_->Update(&count, 0LL);
  EXPECT_EQ(8, count);
  TidyInterface::ActorVector actors;

  // Code uses a depth range of kMinDepth to kMaxDepth.  Layers are
  // disributed evenly within that range, except we don't use the
  // frontmost or backmost values in that range.
  uint32 max_count = NextPowerOfTwo(static_cast<uint32>(count + 2));
  float thickness = -(TidyInterface::LayerVisitor::kMaxDepth -
                TidyInterface::LayerVisitor::kMinDepth) / max_count;
  float depth = TidyInterface::LayerVisitor::kMaxDepth + thickness;

  // First we test the layer visitor directly.
  TidyInterface::LayerVisitor layer_visitor(count);
  stage_->Accept(&layer_visitor);

  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group4_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect1_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group1_.get())->z());

  // Now we test higher-level layer depth results.
  depth = TidyInterface::LayerVisitor::kMaxDepth + thickness;
  interface()->Draw();
  EXPECT_EQ(8, interface()->actor_count());

  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group4_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect1_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group1_.get())->z());
}

TEST_F(TidyTestTree, LayerDepthWithOpacity) {
  rect2_->SetOpacity(0.5f, 0);

  // Test lower-level layer-setting routines
  int32 count = 0;
  stage_->Update(&count, 0LL);
  EXPECT_EQ(8, count);
  TidyInterface::ActorVector actors;

  // Code uses a depth range of kMinDepth to kMaxDepth.  Layers are
  // disributed evenly within that range, except we don't use the
  // frontmost or backmost values in that range.
  uint32 max_count = NextPowerOfTwo(static_cast<uint32>(count + 2));
  float thickness = -(TidyInterface::LayerVisitor::kMaxDepth -
                TidyInterface::LayerVisitor::kMinDepth) / max_count;
  float depth = TidyInterface::LayerVisitor::kMaxDepth + thickness;

  // First we test the layer visitor directly.
  TidyInterface::LayerVisitor layer_visitor(count);
  stage_->Accept(&layer_visitor);

  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group4_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect1_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group1_.get())->z());

  // Now we test higher-level layer depth results.
  depth = TidyInterface::LayerVisitor::kMaxDepth + thickness;
  interface()->Draw();
  EXPECT_EQ(8, interface()->actor_count());

  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group4_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group3_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::QuadActor*>(rect1_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group2_.get())->z());
  depth += thickness;
  EXPECT_FLOAT_EQ(
      depth,
      dynamic_cast<TidyInterface::ContainerActor*>(group1_.get())->z());
}

TEST_F(TidyTestTree, ActorVisitor) {
  NameCheckVisitor visitor;
  stage_->Accept(&visitor);

  vector<string> expected;
  expected.push_back("stage");
  expected.push_back("group3");
  expected.push_back("group4");
  expected.push_back("rect3");
  expected.push_back("rect2");
  expected.push_back("group1");
  expected.push_back("group2");
  expected.push_back("rect1");

  const vector<string>& results = visitor.results();
  EXPECT_EQ(expected.size(), results.size());
  // Yes, this could be a loop, but then it gets harder to know which
  // one failed.  And there's only eight of them.
  EXPECT_EQ(expected[0], results[0]);
  EXPECT_EQ(expected[1], results[1]);
  EXPECT_EQ(expected[2], results[2]);
  EXPECT_EQ(expected[3], results[3]);
  EXPECT_EQ(expected[4], results[4]);
  EXPECT_EQ(expected[5], results[5]);
  EXPECT_EQ(expected[6], results[6]);
  EXPECT_EQ(expected[7], results[7]);
}

TEST_F(TidyTestTree, ActorAttributes) {
  TidyInterface::LayerVisitor layer_visitor(interface()->actor_count());
  stage_->Accept(&layer_visitor);

  // Make sure width and height set the right parameters.
  rect1_->SetSize(12, 13);
  EXPECT_EQ(12, rect1_->width());
  EXPECT_EQ(13, rect1_->height());

  // Make sure scale is independent of width and height.
  rect1_->Scale(2.0f, 3.0f, 0);
  EXPECT_EQ(2.0f, rect1_->scale_x());
  EXPECT_EQ(3.0f, rect1_->scale_y());
  EXPECT_EQ(12, rect1_->width());
  EXPECT_EQ(13, rect1_->height());

  // Make sure Move isn't relative, and works on both axes.
  rect1_->MoveX(2, 0);
  rect1_->MoveX(2, 0);
  rect1_->MoveY(2, 0);
  rect1_->MoveY(2, 0);
  EXPECT_EQ(2, rect1_->x());
  EXPECT_EQ(2, rect1_->y());
  EXPECT_EQ(12, rect1_->width());
  EXPECT_EQ(13, rect1_->height());
  rect1_->Move(4, 4, 0);
  rect1_->Move(4, 4, 0);
  EXPECT_EQ(4, rect1_->x());
  EXPECT_EQ(4, rect1_->y());

  // Test depth setting.
  rect1_->set_z(14.0f);
  EXPECT_EQ(14.0f, rect1_->z());

  // Test opacity setting.
  rect1_->SetOpacity(0.6f, 0);
  // Have to traverse the tree to update is_opaque.
  stage_->Accept(&layer_visitor);
  EXPECT_EQ(0.6f, rect1_->opacity());
  EXPECT_FALSE(rect1_->is_opaque());
  rect1_->SetOpacity(1.0f, 0);
  stage_->Accept(&layer_visitor);
  EXPECT_EQ(1.0f, rect1_->opacity());
  EXPECT_TRUE(rect1_->is_opaque());

  // Test visibility setting.
  rect1_->SetVisibility(true);
  stage_->Accept(&layer_visitor);
  EXPECT_TRUE(rect1_->IsVisible());
  EXPECT_TRUE(rect1_->is_opaque());
  rect1_->SetVisibility(false);
  stage_->Accept(&layer_visitor);
  EXPECT_FALSE(rect1_->IsVisible());
  rect1_->SetVisibility(true);
  rect1_->SetOpacity(0.00001f, 0);
  stage_->Accept(&layer_visitor);
  EXPECT_FALSE(rect1_->IsVisible());
  EXPECT_FALSE(rect1_->is_opaque());
}

TEST_F(TidyTestTree, ContainerActorAttributes) {
  TidyInterface::LayerVisitor layer_visitor(interface()->actor_count());
  stage_->Accept(&layer_visitor);
  rect1_->SetSize(10, 5);
  // Make sure width and height set the right parameters.
  group1_->SetSize(12, 13);
  // Groups ignore SetSize.
  EXPECT_EQ(1, group1_->width());
  EXPECT_EQ(1, group1_->height());
  EXPECT_EQ(10, rect1_->width());
  EXPECT_EQ(5, rect1_->height());

  // Make sure scale is independent of width and height.
  group1_->Scale(2.0f, 3.0f, 0);
  EXPECT_EQ(2.0f, group1_->scale_x());
  EXPECT_EQ(3.0f, group1_->scale_y());
  EXPECT_EQ(1, group1_->width());
  EXPECT_EQ(1, group1_->height());
  EXPECT_EQ(10, rect1_->width());
  EXPECT_EQ(5, rect1_->height());
  EXPECT_EQ(1.0f, rect1_->scale_x());
  EXPECT_EQ(1.0f, rect1_->scale_y());

  // Make sure Move isn't relative, and works on both axes.
  group1_->MoveX(2, 0);
  group1_->MoveX(2, 0);
  group1_->MoveY(2, 0);
  group1_->MoveY(2, 0);
  EXPECT_EQ(2, group1_->x());
  EXPECT_EQ(2, group1_->y());
  group1_->Move(4, 4, 0);
  group1_->Move(4, 4, 0);
  EXPECT_EQ(4, group1_->x());
  EXPECT_EQ(4, group1_->y());

  // Test depth setting.
  group1_->set_z(14.0f);
  EXPECT_EQ(14.0f, group1_->z());

  // Test opacity setting.
  group1_->SetOpacity(0.6f, 0);
  stage_->Accept(&layer_visitor);
  EXPECT_EQ(0.6f, group1_->opacity());
  EXPECT_FALSE(group1_->is_opaque());
  group1_->SetOpacity(1.0f, 0);
  stage_->Accept(&layer_visitor);
  EXPECT_EQ(1.0f, group1_->opacity());
  EXPECT_TRUE(group1_->is_opaque());

  // Test visibility setting.
  group1_->SetVisibility(true);
  stage_->Accept(&layer_visitor);
  EXPECT_TRUE(group1_->IsVisible());
  EXPECT_TRUE(group1_->is_opaque());
  EXPECT_TRUE(rect1_->is_opaque());
  group1_->SetVisibility(false);
  stage_->Accept(&layer_visitor);
  EXPECT_FALSE(group1_->IsVisible());
  EXPECT_TRUE(rect1_->IsVisible());
  group1_->SetVisibility(true);
  group1_->SetOpacity(0.00001f, 0);
  stage_->Accept(&layer_visitor);
  EXPECT_FALSE(group1_->IsVisible());
  EXPECT_FALSE(group1_->is_opaque());
  EXPECT_TRUE(rect1_->IsVisible());
}

TEST_F(TidyTest, FloatAnimation) {
  float value = -10.0f;
  TidyInterface::FloatAnimation anim(&value, 10.0f, 0, 20);
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

TEST_F(TidyTest, IntAnimation) {
  int value = -10;
  TidyInterface::IntAnimation anim(&value, 10, 0, 20);
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

TEST_F(TidyTestTree, CloneTest) {
  rect1_->Move(10, 20, 0);
  rect1_->SetSize(100, 200);
  TidyInterface::Actor* clone = rect1_->Clone();
  EXPECT_EQ(10, clone->x());
  EXPECT_EQ(20, clone->y());
  EXPECT_EQ(100, clone->width());
  EXPECT_EQ(200, clone->height());
}

// Test TidyInterface's handling of X events concerning composited windows.
TEST_F(TidyTest, HandleXEvents) {
  // The interface shouldn't be asking for events about any windows at first.
  EXPECT_TRUE(event_source()->tracked_xids.empty());

  // Draw once initially to make sure that the interface isn't dirty.
  interface()->Draw();
  EXPECT_FALSE(interface()->dirty());

  // Now create an texture pixmap actor and add it to the stage.
  scoped_ptr<ClutterInterface::TexturePixmapActor> actor(
      interface()->CreateTexturePixmap());

  TidyInterface::TexturePixmapActor* cast_actor =
      dynamic_cast<TidyInterface::TexturePixmapActor*>(actor.get());
  CHECK(cast_actor);
  EXPECT_FALSE(cast_actor->HasPixmapDrawingData());
  actor->SetVisibility(true);
  interface()->GetDefaultStage()->AddActor(actor.get());
  EXPECT_TRUE(interface()->dirty());
  interface()->Draw();
  EXPECT_FALSE(interface()->dirty());

  XWindow xid = x_connection()->CreateWindow(
      x_connection()->GetRootWindow(),  // parent
      0, 0,      // x, y
      400, 300,  // width, height
      false,     // override_redirect=false
      false,     // input_only=false
      0);        // event_mask
  MockXConnection::WindowInfo* info =
      x_connection()->GetWindowInfoOrDie(xid);
  info->compositing_pixmap = 123;  // arbitrary

  // After we bind the actor to our window, the window should be
  // redirected and the interface should be marked dirty.
  EXPECT_TRUE(actor->SetTexturePixmapWindow(xid));
  EXPECT_TRUE(info->redirected);
  EXPECT_TRUE(interface()->dirty());
  EXPECT_EQ(static_cast<size_t>(1), event_source()->tracked_xids.size());
  EXPECT_TRUE(event_source()->tracked_xids.count(xid));

  // We should pick up the window's pixmap the next time we draw.
  interface()->Draw();
  EXPECT_TRUE(cast_actor->HasPixmapDrawingData());
  EXPECT_FALSE(interface()->dirty());

  // Now resize the window.  The pixmap should get thrown away.
  info->width = 640;
  info->height = 480;
  interface()->HandleWindowConfigured(xid);
  EXPECT_TRUE(interface()->dirty());
  EXPECT_FALSE(cast_actor->HasPixmapDrawingData());

  // A new pixmap should be loaded the next time we draw.
  interface()->Draw();
  EXPECT_TRUE(cast_actor->HasPixmapDrawingData());
  EXPECT_FALSE(interface()->dirty());

  // TODO: Test that we refresh textures when we see damage events.

  // We should throw away the pixmap and un-redirect the window after
  // seeing the window get destroyed.
  interface()->HandleWindowDestroyed(xid);
  EXPECT_FALSE(info->redirected);
  EXPECT_EQ(None, cast_actor->texture_pixmap_window());
  EXPECT_FALSE(cast_actor->HasPixmapDrawingData());
  EXPECT_TRUE(interface()->dirty());
  EXPECT_TRUE(event_source()->tracked_xids.empty());
}

}  // end namespace window_manager

int main(int argc, char **argv) {
  window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
