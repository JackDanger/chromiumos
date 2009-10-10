// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/util.h"

#include <cstdarg>
#include <cstdlib>
#include <ctime>
#include <glog/logging.h>
#include <string.h>

#include <iomanip>
#include <sstream>

namespace chromeos {

ByteMap::ByteMap(int width, int height)
    : width_(width),
      height_(height) {
  CHECK_GT(width, 0);
  CHECK_GT(height, 0);
  bytes_ = new unsigned char[width * height];
  Clear(0);
}

ByteMap::~ByteMap() {
  delete[] bytes_;
  bytes_ = NULL;
}

void ByteMap::Copy(const ByteMap& other) {
  CHECK_EQ(width_, other.width_);
  CHECK_EQ(height_, other.height_);
  memcpy(bytes_, other.bytes_, width_ * height_);
}

void ByteMap::Clear(unsigned char value) {
  memset(bytes_, value, width_ * height_);
}

void ByteMap::SetRectangle(int rect_x, int rect_y,
                           int rect_width, int rect_height,
                           unsigned char value) {
  const int limit_x = min(rect_x + rect_width, width_);
  const int limit_y = min(rect_y + rect_height, height_);
  rect_x = max(rect_x, 0);
  rect_y = max(rect_y, 0);

  if (rect_x >= limit_x)
    return;

  for (int y = rect_y; y < limit_y; ++y)
    memset(bytes_ + y * width_ + rect_x, value, limit_x - rect_x);
}


double GetCurrentTime() {
  struct timeval tv;
  CHECK_EQ(gettimeofday(&tv, NULL), 0);
  return tv.tv_sec + (tv.tv_usec / 1000000.0);
}


void FillTimeval(double time, struct timeval* tv) {
  CHECK(tv);
  tv->tv_sec = static_cast<__time_t>(time);
  tv->tv_usec =
      static_cast<__suseconds_t>(1000000 * (time - static_cast<int>(time)));
}

}  // namespace chromeos
