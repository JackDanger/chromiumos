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


TEST_F(NoClutterTest, FloatAnimation) {
  float value = -10.0f;
  NoClutterInterface::FloatAnimation anim(&value, 10.0f, 0, 20);
  EXPECT_FALSE(anim.Eval(0));
  EXPECT_FLOAT_EQ(-10.0f, value);
  EXPECT_FALSE(anim.Eval(5));
  EXPECT_FLOAT_EQ(-sqrt(50.0f), value);
  EXPECT_FALSE(anim.Eval(10));
  EXPECT_FLOAT_EQ(0.0f, value);
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
