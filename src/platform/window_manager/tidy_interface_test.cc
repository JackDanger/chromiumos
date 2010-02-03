// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <cstdarg>
#include <string>
#include <vector>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/command_line.h"
#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/tidy_interface.h"
#include "window_manager/mock_gl_interface.h"
#include "window_manager/mock_x_connection.h"
#include "window_manager/util.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

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

class TidyTest : public ::testing::Test {
 public:
  TidyTest() : interface_(NULL), gl_interface_(new MockGLInterface),
                    x_connection_(new MockXConnection) {
    interface_.reset(new TestInterface(x_connection_.get(),
                                       gl_interface_.get()));
  }
  virtual ~TidyTest() {
    interface_.reset(NULL);  // Must explicitly delete so that we get
                             // the order right.
  }
  TidyInterface* interface() { return interface_.get(); }
 private:
  scoped_ptr<TidyInterface> interface_;
  scoped_ptr<GLInterface> gl_interface_;
  scoped_ptr<XConnection> x_connection_;
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

    //     stage
    //     |   |
    // group1  group3
    //   |         |
    // group2    group4
    //   |       |    |
    // rect1  rect2 rect3

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

TEST_F(TidyTestTree, ActorCollectorBasic) {
  vector<string> expected;
  expected.push_back("stage");
  expected.push_back("group3");
  expected.push_back("group4");
  expected.push_back("rect3");
  expected.push_back("rect2");
  expected.push_back("group1");
  expected.push_back("group2");
  expected.push_back("rect1");

  TidyInterface::ActorCollector collector;
  stage_->Accept(&collector);
  TidyInterface::ActorVector results = collector.results();
  ASSERT_EQ(8, results.size());
  EXPECT_STREQ(expected[0].c_str(), results[0]->name().c_str());
  EXPECT_STREQ(expected[1].c_str(), results[1]->name().c_str());
  EXPECT_STREQ(expected[2].c_str(), results[2]->name().c_str());
  EXPECT_STREQ(expected[3].c_str(), results[3]->name().c_str());
  EXPECT_STREQ(expected[4].c_str(), results[4]->name().c_str());
  EXPECT_STREQ(expected[5].c_str(), results[5]->name().c_str());
  EXPECT_STREQ(expected[6].c_str(), results[6]->name().c_str());
  EXPECT_STREQ(expected[7].c_str(), results[7]->name().c_str());
}

TEST_F(TidyTestTree, ActorCollectorBranches) {
  vector<string> expected;
  expected.push_back("stage");
  expected.push_back("group3");
  expected.push_back("group4");
  expected.push_back("group1");
  expected.push_back("group2");

  TidyInterface::ActorCollector collector;
  collector.CollectLeaves(false);
  collector.CollectBranches(true);
  stage_->Accept(&collector);

  TidyInterface::ActorVector results = collector.results();
  ASSERT_EQ(5, results.size());
  EXPECT_STREQ(expected[0].c_str(), results[0]->name().c_str());
  EXPECT_STREQ(expected[1].c_str(), results[1]->name().c_str());
  EXPECT_STREQ(expected[2].c_str(), results[2]->name().c_str());
  EXPECT_STREQ(expected[3].c_str(), results[3]->name().c_str());
  EXPECT_STREQ(expected[4].c_str(), results[4]->name().c_str());
}

TEST_F(TidyTestTree, ActorCollectorLeaves) {
  vector<string> expected;
  expected.push_back("rect3");
  expected.push_back("rect2");
  expected.push_back("rect1");

  TidyInterface::ActorCollector collector;
  collector.CollectLeaves(true);
  collector.CollectBranches(false);
  stage_->Accept(&collector);

  TidyInterface::ActorVector results = collector.results();
  ASSERT_EQ(3, results.size());
  EXPECT_STREQ(expected[0].c_str(), results[0]->name().c_str());
  EXPECT_STREQ(expected[1].c_str(), results[1]->name().c_str());
  EXPECT_STREQ(expected[2].c_str(), results[2]->name().c_str());
}

TEST_F(TidyTestTree, ActorCollectorVisible) {
  vector<string> expected;
  expected.push_back("stage");
  expected.push_back("group1");
  expected.push_back("group2");
  expected.push_back("rect1");

  TidyInterface::ActorCollector collector;
  collector.CollectLeaves(true);
  collector.CollectBranches(true);
  collector.CollectVisible(TidyInterface::ActorCollector::VALUE_TRUE);
  group3_->SetVisibility(false);
  stage_->Accept(&collector);

  TidyInterface::ActorVector results = collector.results();
  ASSERT_EQ(4, results.size());
  EXPECT_STREQ(expected[0].c_str(), results[0]->name().c_str());
  EXPECT_STREQ(expected[1].c_str(), results[1]->name().c_str());
  EXPECT_STREQ(expected[2].c_str(), results[2]->name().c_str());
  EXPECT_STREQ(expected[3].c_str(), results[3]->name().c_str());
}

TEST_F(TidyTestTree, ActorCollectorOpaque) {
  vector<string> expected;
  expected.push_back("group1");
  expected.push_back("group2");
  expected.push_back("rect1");

  TidyInterface::ActorCollector collector;
  collector.CollectLeaves(true);
  collector.CollectBranches(true);
  collector.CollectOpaque(TidyInterface::ActorCollector::VALUE_FALSE);
  group1_->SetOpacity(0.5f, 0);
  group2_->SetOpacity(0.5f, 0);
  rect1_->SetOpacity(0.5f, 0);
  stage_->Accept(&collector);

  TidyInterface::ActorVector results = collector.results();
  ASSERT_EQ(3, results.size());
  EXPECT_STREQ(expected[0].c_str(), results[0]->name().c_str());
  EXPECT_STREQ(expected[1].c_str(), results[1]->name().c_str());
  EXPECT_STREQ(expected[2].c_str(), results[2]->name().c_str());
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
