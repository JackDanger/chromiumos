// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THEME_SCOPED_PATTERN_H_
#define THEME_SCOPED_PATTERN_H_

#include <cairo.h>

namespace theme {

// ScopedPattern manages a cairo_pattern_t for you, taking care of deleting the
// the pattern when the ScopedPattern is deleted.
class ScopedPattern {
 public:
  ScopedPattern() : pattern_(NULL) {}
  explicit ScopedPattern(cairo_pattern_t* pattern) : pattern_(pattern) {}

  ~ScopedPattern() {
    if (pattern_)
      cairo_pattern_destroy(pattern_);
  }

  void reset(cairo_pattern_t* pattern) {
    if (pattern == pattern_)
      return;
    if (pattern_)
      cairo_pattern_destroy(pattern_);
    pattern_ = pattern;
  }

  cairo_pattern_t* get() { return pattern_; }

 private:
  cairo_pattern_t* pattern_;

  // DISALLOW_COPY_AND_ASSIGN
  void operator=(const ScopedPattern& cr);
  ScopedPattern(const ScopedPattern& cr);
};

}  // namespace theme

#endif  // THEME_SCOPED_PATTERN_H_
