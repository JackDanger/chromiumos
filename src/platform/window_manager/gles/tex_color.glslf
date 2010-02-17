// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

varying mediump vec2 tex;

uniform lowp vec4 color;
uniform lowp sampler2D sampler;

void main() {
  gl_FragColor = color * texture2D(sampler, tex);
}
