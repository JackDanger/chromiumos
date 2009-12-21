// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/scoped_ptr.h"
#include "base/logging.h"
#include "window_manager/clutter_interface.h"
#include "window_manager/shadow.h"
#include "window_manager/test_lib.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

class ShadowTest : public ::testing::Test {
 protected:
  virtual void SetUp() {
    clutter_.reset(new MockClutterInterface);
  }

  scoped_ptr<ClutterInterface> clutter_;
};

TEST_F(ShadowTest, Basic) {
  Shadow shadow(clutter_.get());
  shadow.Move(10, 20, 0);
  shadow.Resize(200, 100, 0);
  shadow.SetOpacity(0.75, 0);
  shadow.Show();
  // TODO: Check that the individual images are positioned correctly.
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
