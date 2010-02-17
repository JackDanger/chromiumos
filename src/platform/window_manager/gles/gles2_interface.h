// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_GLES2_INTERFACE_H_
#define WINDOW_MANAGER_GLES2_INTERFACE_H_

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "base/basictypes.h"
#include "window_manager/gl_interface_base.h"

namespace window_manager {

// This is an abstract base class representing a GLES2 interface.
class Gles2Interface : virtual public GLInterfaceBase {
 public:
  Gles2Interface() {}
  virtual ~Gles2Interface() {}

  virtual bool InitExtensions() = 0;

  virtual EGLDisplay egl_display() = 0;

  // EGL Core
  virtual EGLBoolean EglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                                     EGLConfig *configs, EGLint config_size,
                                     EGLint *num_config) = 0;
  virtual EGLContext EglCreateContext(EGLDisplay dpy, EGLConfig config,
                                      EGLContext share_context,
                                      const EGLint *attrib_list) = 0;
  virtual EGLSurface EglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                            EGLNativeWindowType win,
                                            const EGLint *attrib_list) = 0;
  virtual EGLBoolean EglDestroyContext(EGLDisplay dpy, EGLContext ctx) = 0;
  virtual EGLBoolean EglDestroySurface(EGLDisplay dpy,
                                       EGLSurface surface) = 0;
  virtual EGLDisplay EglGetDisplay(EGLNativeDisplayType display_id) = 0;
  virtual EGLint EglGetError() = 0;
  virtual EGLBoolean EglInitialize(EGLDisplay dpy, EGLint *major,
                                   EGLint *minor) = 0;
  virtual EGLBoolean EglMakeCurrent(EGLDisplay dpy, EGLSurface draw,
                                    EGLSurface read, EGLContext ctx) = 0;
  virtual const char* EglQueryString(EGLDisplay dpy, EGLint name) = 0;
  virtual EGLBoolean EglSwapBuffers(EGLDisplay dpy, EGLSurface surface) = 0;
  virtual EGLBoolean EglTerminate(EGLDisplay dpy) = 0;

  // Functions from the EGL_KHR_image extension
  virtual EGLImageKHR EglCreateImageKHR(EGLDisplay dpy, EGLContext ctx,
                                        EGLenum target, EGLClientBuffer buffer,
                                        const EGLint* attrib_list) = 0;
  virtual EGLBoolean EglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image) = 0;

  // OpenGLES 2 Core
  virtual void ActiveTexture(GLenum texture) = 0;
  virtual void AttachShader(GLuint program, GLuint shader) = 0;
  virtual void BindBuffer(GLenum target, GLuint buffer) = 0;
  virtual void BindTexture(GLenum target, GLuint texture) = 0;
  virtual void BufferData(GLenum target, GLsizeiptr size, const void* data,
                          GLenum usage) = 0;
  virtual void Clear(GLbitfield mask) = 0;
  virtual void ClearColor(GLclampf red, GLclampf green, GLclampf blue,
                          GLclampf alpha) = 0;
  virtual void CompileShader(GLuint shader) = 0;
  virtual GLuint CreateProgram() = 0;
  virtual GLuint CreateShader(GLenum type) = 0;
  virtual void DeleteBuffers(GLsizei n, const GLuint* buffers) = 0;
  virtual void DeleteProgram(GLuint program) = 0;
  virtual void DeleteShader(GLuint shader) = 0;
  virtual void DeleteTextures(GLsizei n, const GLuint* textures) = 0;
  virtual void Disable(GLenum cap) = 0;
  virtual void DisableVertexAttribArray(GLuint index) = 0;
  virtual void DrawArrays(GLenum mode, GLint first, GLsizei count) = 0;
  virtual void Enable(GLenum cap) = 0;
  virtual void EnableVertexAttribArray(GLuint index) = 0;
  virtual void GenBuffers(GLsizei n, GLuint* buffers) = 0;
  virtual void GenTextures(GLsizei n, GLuint* textures) = 0;
  virtual int GetAttribLocation(GLuint program, const char* name) = 0;
  virtual GLenum GetError() = 0;
  virtual void GetIntegerv(GLenum pname, GLint* params) = 0;
  virtual void GetProgramiv(GLuint program, GLenum pname, GLint* params) = 0;
  virtual void GetProgramInfoLog(GLuint program, GLsizei bufsize,
                                 GLsizei* length, char* infolog) = 0;
  virtual void GetShaderiv(GLuint shader, GLenum pname, GLint* params) = 0;
  virtual void GetShaderInfoLog(GLuint shader, GLsizei bufsize,
                                GLsizei* length, char* infolog) = 0;
  virtual const GLubyte* GetString(GLenum name) = 0;
  virtual int GetUniformLocation(GLuint program, const char* name) = 0;
  virtual void LinkProgram(GLuint program) = 0;
  virtual void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                          GLenum format, GLenum type, void* pixels) = 0;
  virtual void ReleaseShaderCompiler() = 0;
  virtual void ShaderSource(GLuint shader, GLsizei count, const char** string,
                            const GLint* length) = 0;
  virtual void TexImage2D(GLenum target, GLint level, GLenum internalformat,
                          GLsizei width, GLsizei height, GLint border,
                          GLenum format, GLenum type, const void* pixels) = 0;
  virtual void TexParameteri(GLenum target, GLenum pname, GLint param) = 0;
  virtual void Uniform1f(GLint location, GLfloat x) = 0;
  virtual void Uniform1fv(GLint location, GLsizei count,
                          const GLfloat* v) = 0;
  virtual void Uniform1i(GLint location, GLint x) = 0;
  virtual void Uniform1iv(GLint location, GLsizei count, const GLint* v) = 0;
  virtual void Uniform2f(GLint location, GLfloat x, GLfloat y) = 0;
  virtual void Uniform2fv(GLint location, GLsizei count,
                          const GLfloat* v) = 0;
  virtual void Uniform2i(GLint location, GLint x, GLint y) = 0;
  virtual void Uniform2iv(GLint location, GLsizei count, const GLint* v) = 0;
  virtual void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z) = 0;
  virtual void Uniform3fv(GLint location, GLsizei count,
                          const GLfloat* v) = 0;
  virtual void Uniform3i(GLint location, GLint x, GLint y, GLint z) = 0;
  virtual void Uniform3iv(GLint location, GLsizei count, const GLint* v) = 0;
  virtual void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z,
                         GLfloat w) = 0;
  virtual void Uniform4fv(GLint location, GLsizei count,
                          const GLfloat* v) = 0;
  virtual void Uniform4i(GLint location, GLint x, GLint y, GLint z,
                         GLint w) = 0;
  virtual void Uniform4iv(GLint location, GLsizei count, const GLint* v) = 0;
  virtual void UniformMatrix2fv(GLint location, GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) = 0;
  virtual void UniformMatrix3fv(GLint location, GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) = 0;
  virtual void UniformMatrix4fv(GLint location, GLsizei count,
                                GLboolean transpose,
                                const GLfloat* value) = 0;
  virtual void UseProgram(GLuint program) = 0;
  virtual void VertexAttribPointer(GLuint indx, GLint size, GLenum type,
                                   GLboolean normalized, GLsizei stride,
                                   const void* ptr) = 0;

  // Fucntions from the GL_OES_EGL_image extension
  virtual void EGLImageTargetTexture2DOES(GLenum target,
                                          GLeglImageOES image) = 0;
  virtual void EGLImageTargetRenderbufferStorageOES(GLenum target,
                                                    GLeglImageOES image) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(Gles2Interface);
};

}  // namespace window_manager

#endif
