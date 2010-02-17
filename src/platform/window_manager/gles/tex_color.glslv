// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

uniform highp mat4 mvp;

attribute highp vec4 pos;
attribute highp vec2 tex_in;

varying mediump vec2 tex;

void main() {
  tex = tex_in;
  gl_Position = mvp * pos;
}
