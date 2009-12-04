// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gflags/gflags.h>
#include <gtest/gtest.h>

#include "base/logging.h"
#include "chromeos/string.h"
#include "window_manager/test_lib.h"
#include "window_manager/util.h"

DEFINE_bool(logtostderr, false,
            "Print debugging messages to stderr (suppressed otherwise)");

namespace chromeos {

class UtilTest : public ::testing::Test {
 protected:
  // Helper function for testStacker().
  // 'expected' is a space-separated list of strings in the order in which
  // they should appear in 'actual'.
  void CheckStackerOutput(const std::list<std::string>& actual,
                          const std::string& expected) {
    std::vector<std::string> expected_parts;
    SplitString(expected, &expected_parts);
    ASSERT_EQ(actual.size(), expected_parts.size());

    int i = 0;
    for (std::list<std::string>::const_iterator it = actual.begin();
         it != actual.end(); ++it, ++i) {
      EXPECT_EQ(*it, expected_parts[i]);
    }
  }
};

TEST_F(UtilTest, Stacker) {
  Stacker<std::string> stacker;

  stacker.AddOnTop("b");
  stacker.AddOnBottom("c");
  stacker.AddOnTop("a");
  stacker.AddOnBottom("d");
  CheckStackerOutput(stacker.items(), "a b c d");
  EXPECT_EQ(0, stacker.GetIndex("a"));
  EXPECT_EQ(1, stacker.GetIndex("b"));
  EXPECT_EQ(2, stacker.GetIndex("c"));
  EXPECT_EQ(3, stacker.GetIndex("d"));

  stacker.AddBelow("a2", "a");
  stacker.AddBelow("b2", "b");
  stacker.AddBelow("c2", "c");
  stacker.AddBelow("d2", "d");
  CheckStackerOutput(stacker.items(), "a a2 b b2 c c2 d d2");

  stacker.Remove("a");
  stacker.Remove("c");
  stacker.Remove("d2");
  CheckStackerOutput(stacker.items(), "a2 b b2 c2 d");

  EXPECT_EQ(NULL, stacker.GetUnder("not-present"));
  EXPECT_EQ(NULL, stacker.GetUnder("d"));
  const std::string* str = NULL;
  ASSERT_TRUE((str = stacker.GetUnder("c2")) != NULL);
  EXPECT_EQ("d", *str);
  ASSERT_TRUE((str = stacker.GetUnder("b")) != NULL);
  EXPECT_EQ("b2", *str);
  ASSERT_TRUE((str = stacker.GetUnder("a2")) != NULL);
  EXPECT_EQ("b", *str);

  stacker.AddAbove("a3", "a2");
  stacker.AddAbove("b3", "b2");
  stacker.AddAbove("d3", "d");
  CheckStackerOutput(stacker.items(), "a3 a2 b b3 b2 c2 d3 d");
}

TEST_F(UtilTest, ByteMap) {
  int width = 4, height = 3;
  ByteMap bytemap(width, height);
  EXPECT_EQ(width, bytemap.width());
  EXPECT_EQ(height, bytemap.height());
  EXPECT_PRED_FORMAT3(
      BytesAreEqual,
      reinterpret_cast<unsigned const char*>("\x00\x00\x00\x00"
                                             "\x00\x00\x00\x00"
                                             "\x00\x00\x00\x00"),
      bytemap.bytes(),
      width * height);

  // Set a few rectangles that are bogus or fall entirely outside of the
  // region.
  bytemap.SetRectangle(-width, 0, width, height, 0xff);
  bytemap.SetRectangle(width, 0, width, height, 0xff);
  bytemap.SetRectangle(0, -height, width, height, 0xff);
  bytemap.SetRectangle(0, height, width, height, 0xff);
  bytemap.SetRectangle(0, 0, width, -1, 0xff);
  bytemap.SetRectangle(0, 0, -1, height, 0xff);
  EXPECT_PRED_FORMAT3(
      BytesAreEqual,
      reinterpret_cast<unsigned const char*>("\x00\x00\x00\x00"
                                             "\x00\x00\x00\x00"
                                             "\x00\x00\x00\x00"),
      bytemap.bytes(),
      width * height);

  // Set a few rectangles that partially cover the region and then one
  // that matches its size.
  bytemap.SetRectangle(-2, -3, 3, 4, 0xf0);
  EXPECT_PRED_FORMAT3(
      BytesAreEqual,
      reinterpret_cast<unsigned const char*>("\xf0\x00\x00\x00"
                                             "\x00\x00\x00\x00"
                                             "\x00\x00\x00\x00"),
      bytemap.bytes(),
      width * height);
  bytemap.SetRectangle(width - 3, height - 1, 10, 10, 0xff);
  EXPECT_PRED_FORMAT3(
      BytesAreEqual,
      reinterpret_cast<unsigned const char*>("\xf0\x00\x00\x00"
                                             "\x00\x00\x00\x00"
                                             "\x00\xff\xff\xff"),
      bytemap.bytes(),
      width * height);
  bytemap.SetRectangle(0, 0, width, height, 0xaa);
  EXPECT_PRED_FORMAT3(
      BytesAreEqual,
      reinterpret_cast<unsigned const char*>("\xaa\xaa\xaa\xaa"
                                             "\xaa\xaa\xaa\xaa"
                                             "\xaa\xaa\xaa\xaa"),
      bytemap.bytes(),
      width * height);

  // Now clear the map to a particular value.
  bytemap.Clear(0x01);
  EXPECT_PRED_FORMAT3(
      BytesAreEqual,
      reinterpret_cast<unsigned const char*>("\x01\x01\x01\x01"
                                             "\x01\x01\x01\x01"
                                             "\x01\x01\x01\x01"),
      bytemap.bytes(),
      width * height);
}

}  // namespace chromeos

int main(int argc, char **argv) {
  return chromeos::InitAndRunTests(&argc, argv, FLAGS_logtostderr);
}
