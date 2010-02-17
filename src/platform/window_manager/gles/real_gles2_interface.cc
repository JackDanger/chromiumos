// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/gles/real_gles2_interface.h"

#define EGLAPI
#define EGLAPIENTRY
#include <EGL/egl.h>
#include "base/logging.h"
#include "window_manager/real_x_connection.h"

#ifdef GL_ERROR_DEBUGGING
#define GLES2_DCHECK_ERROR() \
  do { \
    GLenum error = GetError(); \
    LOG_IF(ERROR, error != GL_NO_ERROR) << "GLES2 Error:" << error; \
  } while (0)
#else
#define GLES2_DCHECK_ERROR() void(0)
#endif

namespace window_manager {

RealGles2Interface::RealGles2Interface(RealXConnection* x)
    : extensions_(),
      egl_display_(EGL_NO_DISPLAY),
      egl_create_image_khr_(NULL),
      egl_destroy_image_khr_(NULL),
      gl_egl_image_target_texture_2d_oes_(NULL),
      gl_egl_image_target_renderbuffer_storage_oes_(NULL) {
  egl_display_ = eglGetDisplay(reinterpret_cast<EGLNativeDisplayType>(
      x->GetDisplay()));
  CHECK(egl_display_ != EGL_NO_DISPLAY) << "Failed to get the EGL display";
  CHECK(eglInitialize(egl_display_, NULL, NULL) == EGL_TRUE)
      << "Failed to initialize EGL";
}

RealGles2Interface::~RealGles2Interface() {
  LOG_IF(ERROR, eglTerminate(egl_display_) != EGL_TRUE)
      << "eglTerminate() failed:" << eglGetError();
}

bool RealGles2Interface::InitExtensions() {
  ParseExtensionString(&extensions_,
      reinterpret_cast<const char*>(GetString(GL_EXTENSIONS)));
  ParseExtensionString(&extensions_,
                       EglQueryString(egl_display_, EGL_EXTENSIONS));

  if (!HasExtension(extensions_, "EGL_KHR_image")) {
    LOG(ERROR) << "EGL extension EGL_KHR_image unavailable.";
    return false;
  }
  if (!HasExtension(extensions_, "GL_OES_EGL_image")) {
    LOG(ERROR) << "OpenGL-ES 2.0 extension GL_OES_EGL_image unavailable.";
    return false;
  }

  egl_create_image_khr_ =
      reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
      eglGetProcAddress("eglCreateImageKHR"));
  if (!egl_create_image_khr_)
    return false;
  egl_destroy_image_khr_ = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
      eglGetProcAddress("eglDestroyImageKHR"));
  if (!egl_destroy_image_khr_)
    return false;
  gl_egl_image_target_texture_2d_oes_ =
      reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
      eglGetProcAddress("glEGLImageTargetTexture2DOES"));
  if (!gl_egl_image_target_texture_2d_oes_)
    return false;
  gl_egl_image_target_renderbuffer_storage_oes_ =
      reinterpret_cast<PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC>(
      eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES"));
  if (!gl_egl_image_target_renderbuffer_storage_oes_)
    return false;

  return true;
}

// EGL Functions
EGLBoolean RealGles2Interface::EglChooseConfig(EGLDisplay dpy,
                                               const EGLint *attrib_list,
                                               EGLConfig *configs,
                                               EGLint config_size,
                                               EGLint *num_config) {
  return eglChooseConfig(dpy, attrib_list, configs, config_size, num_config);
}

EGLContext RealGles2Interface::EglCreateContext(EGLDisplay dpy,
                                                EGLConfig config,
                                                EGLContext share_context,
                                                const EGLint *attrib_list) {
  return eglCreateContext(dpy, config, share_context, attrib_list);
}

EGLSurface RealGles2Interface::EglCreateWindowSurface(
    EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win,
    const EGLint *attrib_list) {
  return eglCreateWindowSurface(dpy, config, win, attrib_list);
}

EGLBoolean RealGles2Interface::EglDestroyContext(EGLDisplay dpy,
                                                 EGLContext ctx) {
  return eglDestroyContext(dpy, ctx);
}

EGLBoolean RealGles2Interface::EglDestroySurface(EGLDisplay dpy,
                                                 EGLSurface surface) {
  return eglDestroySurface(dpy, surface);
}

EGLDisplay RealGles2Interface::EglGetDisplay(EGLNativeDisplayType display_id) {
  return eglGetDisplay(display_id);
}

EGLint RealGles2Interface::EglGetError() {
  return eglGetError();
}


EGLBoolean RealGles2Interface::EglInitialize(EGLDisplay dpy, EGLint *major,
                                             EGLint *minor) {
  return eglInitialize(dpy, major, minor);
}

EGLBoolean RealGles2Interface::EglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                                              EGLSurface read,
                                              EGLContext ctx) {
  return eglMakeCurrent(dpy, draw, read, ctx);
}

const char* RealGles2Interface::EglQueryString(EGLDisplay dpy, EGLint name) {
  return eglQueryString(dpy, name);
}

EGLBoolean RealGles2Interface::EglSwapBuffers(EGLDisplay dpy,
                                              EGLSurface surface) {
  return eglSwapBuffers(dpy, surface);
}

EGLBoolean RealGles2Interface::EglTerminate(EGLDisplay dpy) {
  return eglTerminate(dpy);
}

// EGL_KHR_image
EGLImageKHR RealGles2Interface::EglCreateImageKHR(EGLDisplay dpy,
                                                  EGLContext ctx,
                                                  EGLenum target,
                                                  EGLClientBuffer buffer,
                                                  const EGLint* attrib_list) {
  DCHECK(egl_create_image_khr_);
  // Work around broken EGL/eglext.h headers that have attrib_list defined as
  // non-const
  return egl_create_image_khr_(dpy, ctx, target, buffer,
                               const_cast<EGLint*>(attrib_list));
}

EGLBoolean RealGles2Interface::EglDestroyImageKHR(EGLDisplay dpy,
                                                  EGLImageKHR image) {
  DCHECK(egl_destroy_image_khr_);
  return egl_destroy_image_khr_(dpy, image);
}

// GLES2 Functions
void RealGles2Interface::ActiveTexture(GLenum texture) {
  glActiveTexture(texture);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::AttachShader(GLuint program, GLuint shader) {
  glAttachShader(program, shader);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::BindBuffer(GLenum target, GLuint buffer) {
  glBindBuffer(target, buffer);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::BindTexture(GLenum target, GLuint texture) {
  glBindTexture(target, texture);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::BufferData(GLenum target, GLsizeiptr size,
                                    const void* data, GLenum usage) {
  glBufferData(target, size, data, usage);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Clear(GLbitfield mask) {
  glClear(mask);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::ClearColor(GLclampf red, GLclampf green,
                                    GLclampf blue, GLclampf alpha) {
  glClearColor(red, green, blue, alpha);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::CompileShader(GLuint shader) {
  glCompileShader(shader);
  GLES2_DCHECK_ERROR();
}

GLuint RealGles2Interface::CreateProgram() {
  GLuint retval = glCreateProgram();
  GLES2_DCHECK_ERROR();
  return retval;
}

GLuint RealGles2Interface::CreateShader(GLenum type) {
  GLuint retval = glCreateShader(type);
  GLES2_DCHECK_ERROR();
  return retval;
}

void RealGles2Interface::DeleteBuffers(GLsizei n, const GLuint* buffers) {
  glDeleteBuffers(n, buffers);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::DeleteProgram(GLuint program) {
  glDeleteProgram(program);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::DeleteShader(GLuint shader) {
  glDeleteShader(shader);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::DeleteTextures(GLsizei n, const GLuint* textures) {
  glDeleteTextures(n, textures);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Disable(GLenum cap) {
  glDisable(cap);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::DisableVertexAttribArray(GLuint index) {
  glDisableVertexAttribArray(index);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::DrawArrays(GLenum mode, GLint first, GLsizei count) {
  glDrawArrays(mode, first, count);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Enable(GLenum cap) {
  glEnable(cap);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::EnableVertexAttribArray(GLuint index) {
  glEnableVertexAttribArray(index);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::GenBuffers(GLsizei n, GLuint* buffers) {
  glGenBuffers(n, buffers);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::GenTextures(GLsizei n, GLuint* textures) {
  glGenTextures(n, textures);
  GLES2_DCHECK_ERROR();
}

int RealGles2Interface::GetAttribLocation(GLuint program, const char* name) {
  int retval = glGetAttribLocation(program, name);
  GLES2_DCHECK_ERROR();
  return retval;
}

GLenum RealGles2Interface::GetError() {
  GLenum retval = glGetError();
  GLES2_DCHECK_ERROR();
  return retval;
}

void RealGles2Interface::GetIntegerv(GLenum pname, GLint* params) {
  glGetIntegerv(pname, params);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::GetProgramiv(GLuint program, GLenum pname,
                                      GLint* params) {
  glGetProgramiv(program, pname, params);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::GetProgramInfoLog(GLuint program, GLsizei bufsize,
                                           GLsizei* length, char* infolog) {
  glGetProgramInfoLog(program, bufsize, length, infolog);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::GetShaderiv(GLuint shader, GLenum pname,
                                     GLint* params) {
  glGetShaderiv(shader, pname, params);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::GetShaderInfoLog(GLuint shader, GLsizei bufsize,
                                          GLsizei* length, char* infolog) {
  glGetShaderInfoLog(shader, bufsize, length, infolog);
  GLES2_DCHECK_ERROR();
}

const GLubyte* RealGles2Interface::GetString(GLenum name) {
  const GLubyte* retval = glGetString(name);
  GLES2_DCHECK_ERROR();
  return retval;
}
int RealGles2Interface::GetUniformLocation(GLuint program, const char* name) {
  int retval = glGetUniformLocation(program, name);
  GLES2_DCHECK_ERROR();
  return retval;
}

void RealGles2Interface::LinkProgram(GLuint program) {
  glLinkProgram(program);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::ReadPixels(GLint x, GLint y, GLsizei width,
                                    GLsizei height, GLenum format, GLenum type,
                                    void* pixels) {
  glReadPixels(x, y, width, height, format, type, pixels);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::ReleaseShaderCompiler() {
  glReleaseShaderCompiler();
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::ShaderSource(GLuint shader, GLsizei count,
                                      const char** string,
                                      const GLint* length) {
  glShaderSource(shader, count, string, length);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::TexImage2D(GLenum target, GLint level,
                                    GLenum internalformat, GLsizei width,
                                    GLsizei height, GLint border,
                                    GLenum format, GLenum type,
                                    const void* pixels) {
  glTexImage2D(target, level, internalformat, width, height, border, format,
               type, pixels);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::TexParameteri(GLenum target, GLenum pname,
                                       GLint param) {
  glTexParameteri(target, pname, param);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform1f(GLint location, GLfloat x) {
  glUniform1f(location, x);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform1fv(GLint location, GLsizei count,
                                    const GLfloat* v) {
  glUniform1fv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform1i(GLint location, GLint x) {
  glUniform1i(location, x);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform1iv(GLint location, GLsizei count,
                                    const GLint* v) {
  glUniform1iv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform2f(GLint location, GLfloat x, GLfloat y) {
  glUniform2f(location, x, y);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform2fv(GLint location, GLsizei count,
                                    const GLfloat* v) {
  glUniform2fv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform2i(GLint location, GLint x, GLint y) {
  glUniform2i(location, x, y);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform2iv(GLint location, GLsizei count,
                                    const GLint* v) {
  glUniform2iv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform3f(GLint location, GLfloat x, GLfloat y,
                                   GLfloat z) {
  glUniform3f(location, x, y, z);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform3fv(GLint location, GLsizei count,
                                    const GLfloat* v) {
  glUniform3fv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform3i(GLint location, GLint x, GLint y,
                                   GLint z) {
  glUniform3i(location, x, y, z);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform3iv(GLint location, GLsizei count,
                                    const GLint* v) {
  glUniform3iv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform4f(GLint location, GLfloat x, GLfloat y,
                                   GLfloat z, GLfloat w) {
  glUniform4f(location, x, y, z, w);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform4fv(GLint location, GLsizei count,
                                    const GLfloat* v) {
  glUniform4fv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform4i(GLint location, GLint x, GLint y, GLint z,
                                   GLint w) {
  glUniform4i(location, x, y, z, w);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::Uniform4iv(GLint location, GLsizei count,
                                    const GLint* v) {
  glUniform4iv(location, count, v);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::UniformMatrix2fv(GLint location, GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  glUniformMatrix2fv(location, count, transpose, value);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::UniformMatrix3fv(GLint location, GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  glUniformMatrix3fv(location, count, transpose, value);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::UniformMatrix4fv(GLint location, GLsizei count,
                                          GLboolean transpose,
                                          const GLfloat* value) {
  glUniformMatrix4fv(location, count, transpose, value);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::UseProgram(GLuint program) {
  glUseProgram(program);
  GLES2_DCHECK_ERROR();
}
void RealGles2Interface::VertexAttribPointer(GLuint indx, GLint size,
                                             GLenum type, GLboolean normalized,
                                             GLsizei stride,
                                             const void* ptr) {
  glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::EGLImageTargetTexture2DOES(GLenum target,
                                                    GLeglImageOES image) {
  DCHECK(gl_egl_image_target_texture_2d_oes_);
  gl_egl_image_target_texture_2d_oes_(target, image);
  GLES2_DCHECK_ERROR();
}

void RealGles2Interface::EGLImageTargetRenderbufferStorageOES(
    GLenum target, GLeglImageOES image) {
  DCHECK(gl_egl_image_target_renderbuffer_storage_oes_);
  gl_egl_image_target_renderbuffer_storage_oes_(target, image);
  GLES2_DCHECK_ERROR();
}

}  // namespace window_manager

