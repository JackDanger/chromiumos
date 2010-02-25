// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/gles/shader_base.h"

#include "base/logging.h"
#include "base/scoped_ptr.h"

namespace window_manager {

// TODO: make this use the Gles2Interface

Shader::Shader(const char* vertex_shader, const char* fragment_shader) {
  program_ = glCreateProgram();
  CHECK(program_) << "Unable to allocate shader program.";

  AttachShader(vertex_shader, GL_VERTEX_SHADER);
  AttachShader(fragment_shader, GL_FRAGMENT_SHADER);
  glLinkProgram(program_);

  GLint link_status = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &link_status);
  if (!link_status) {
    GLsizei log_size = 0;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_size);
    // Some GLES drivers have a bug where INFO_LOG_LENGTH returns 0.
    if (!log_size)
      log_size = 4096;
    scoped_array<char> log(new char[log_size]);
    glGetShaderInfoLog(program_, log_size, NULL, log.get());
    CHECK(0) << "Shader program link failed: \n" << log.get();
  }
}

Shader::~Shader() {
  glDeleteProgram(program_);
}

void Shader::AttachShader(const char* source, GLenum type) {
  GLint shader = glCreateShader(type);
  CHECK(shader) << "Unable to allocate shader object.";

  glShaderSource(shader, 1, &source, NULL);
  glCompileShader(shader);
  GLint shader_status = 0;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &shader_status);

  if (!shader_status) {
    GLsizei log_size = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_size);
    // Some GLES drivers have a bug where INFO_LOG_LENGTH returns 0.
    if (!log_size)
      log_size = 4096;
    scoped_array<char> log(new char[log_size]);
    glGetShaderInfoLog(shader, log_size, NULL, log.get());
    CHECK(0) << "Shader compile failed: \n" << log.get();
  }

  glAttachShader(program_, shader);
  glDeleteShader(shader);
}

}  // namespace window_manager

