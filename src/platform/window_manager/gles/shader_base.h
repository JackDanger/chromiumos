// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_GLES_SHADER_BASE_H_
#define WINDOW_MANAGER_GLES_SHADER_BASE_H_

#include <GLES2/gl2.h>

#include "base/basictypes.h"

namespace window_manager {

class Shader {
 public:
  ~Shader();

  int program() const { return program_; }

 protected:
  Shader(const char* vertex_shader, const char* fragment_shader);

 private:
  GLint program_;

  void AttachShader(const char* source, GLenum type);

  DISALLOW_COPY_AND_ASSIGN(Shader);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_GLES_SHADER_BASE_H_

