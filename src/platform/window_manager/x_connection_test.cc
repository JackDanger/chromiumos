// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdlib>
#include <vector>

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "window_manager/test_lib.h"
#include "window_manager/x_connection.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace window_manager {

using std::vector;

class XConnectionTest : public ::testing::Test {};

TEST_F(XConnectionTest, GetKeyCodeState) {
  vector<uint8_t> states;
  states.resize(32);
  memset(states.data(), 0, states.size());
  states[0] = 0x1;    // index 0
  states[12] = 0xf2;  // indices 97, 100-103
  states[31] = 0xff;  // indices 248-255

  for (int i = 0; i < 255; ++i) {
    if (i == 0 || i == 97 || (i >= 100 && i <= 103) ||
        (i >= 248 && i <= 255)) {
      EXPECT_TRUE(XConnection::GetKeyCodeState(states, i)) << "keycode=" << i;
    } else {
      EXPECT_FALSE(XConnection::GetKeyCodeState(states, i)) << "keycode=" << i;
    }
  }
}

}  // namespace window_manager

int main(int argc, char **argv) {
  return window_manager::InitAndRunTests(&argc, argv, &FLAGS_logtostderr);
}
