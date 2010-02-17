// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_REAL_GLES2_INTERFACE_H_
#define WINDOW_MANAGER_REAL_GLES2_INTERFACE_H_

#include <string>
#include <vector>

#include "window_manager/gles/gles2_interface.h"

namespace window_manager {

class RealXConnection;

// This wraps an actual GLES2 interface so that we can mock it and use it for
// testing
class RealGles2Interface : public Gles2Interface {
 public:
  RealGles2Interface(RealXConnection* x);
  virtual ~RealGles2Interface();

  bool InitExtensions();

  EGLDisplay egl_display() { return egl_display_; }

  // EGL core
  EGLBoolean EglChooseConfig(EGLDisplay dpy, const EGLint *attrib_list,
                             EGLConfig *configs, EGLint config_size,
                             EGLint *num_config);
  EGLContext EglCreateContext(EGLDisplay dpy, EGLConfig config,
                              EGLContext share_context,
                              const EGLint *attrib_list);
  EGLSurface EglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                    EGLNativeWindowType win,
                                    const EGLint *attrib_list);
  EGLBoolean EglDestroyContext(EGLDisplay dpy, EGLContext ctx);
  EGLBoolean EglDestroySurface(EGLDisplay dpy, EGLSurface surface);
  EGLDisplay EglGetDisplay(EGLNativeDisplayType display_id);
  EGLint EglGetError();
  EGLBoolean EglInitialize(EGLDisplay dpy, EGLint *major, EGLint *minor);
  EGLBoolean EglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                            EGLContext ctx);
  const char* EglQueryString(EGLDisplay dpy, EGLint name);
  EGLBoolean EglSwapBuffers(EGLDisplay dpy, EGLSurface surface);
  EGLBoolean EglTerminate(EGLDisplay dpy);

  // Functions from the EGL_KHR_image extension
  EGLImageKHR EglCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target,
                                EGLClientBuffer buffer,
                                const EGLint* attrib_list);
  EGLBoolean EglDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image);

  // OpenGLES 2 Core
  void ActiveTexture(GLenum texture);
  void AttachShader(GLuint program, GLuint shader);
  void BindBuffer(GLenum target, GLuint buffer);
  void BindTexture(GLenum target, GLuint texture);
  void BufferData(GLenum target, GLsizeiptr size, const void* data,
                  GLenum usage);
  void Clear(GLbitfield mask);
  void ClearColor(GLclampf red, GLclampf green, GLclampf blue,
                  GLclampf alpha);
  void CompileShader(GLuint shader);
  GLuint CreateProgram();
  GLuint CreateShader(GLenum type);
  void DeleteBuffers(GLsizei n, const GLuint* buffers);
  void DeleteProgram(GLuint program);
  void DeleteShader(GLuint shader);
  void DeleteTextures(GLsizei n, const GLuint* textures);
  void Disable(GLenum cap);
  void DisableVertexAttribArray(GLuint index);
  void DrawArrays(GLenum mode, GLint first, GLsizei count);
  void Enable(GLenum cap);
  void EnableVertexAttribArray(GLuint index);
  void GenBuffers(GLsizei n, GLuint* buffers);
  void GenTextures(GLsizei n, GLuint* textures);
  int GetAttribLocation(GLuint program, const char* name);
  GLenum GetError();
  void GetIntegerv(GLenum pname, GLint* params);
  void GetProgramiv(GLuint program, GLenum pname, GLint* params);
  void GetProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei* length,
                         char* infolog);
  void GetShaderiv(GLuint shader, GLenum pname, GLint* params);
  void GetShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei* length,
                        char* infolog);
  const GLubyte* GetString(GLenum name);
  int GetUniformLocation(GLuint program, const char* name);
  void LinkProgram(GLuint program);
  void ReadPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                  GLenum format, GLenum type, void* pixels);
  void ReleaseShaderCompiler();
  void ShaderSource(GLuint shader, GLsizei count, const char** string,
                    const GLint* length);
  void TexImage2D(GLenum target, GLint level, GLenum internalformat,
                  GLsizei width, GLsizei height, GLint border, GLenum format,
                  GLenum type, const void* pixels);
  void TexParameteri(GLenum target, GLenum pname, GLint param);
  void Uniform1f(GLint location, GLfloat x);
  void Uniform1fv(GLint location, GLsizei count, const GLfloat* v);
  void Uniform1i(GLint location, GLint x);
  void Uniform1iv(GLint location, GLsizei count, const GLint* v);
  void Uniform2f(GLint location, GLfloat x, GLfloat y);
  void Uniform2fv(GLint location, GLsizei count, const GLfloat* v);
  void Uniform2i(GLint location, GLint x, GLint y);
  void Uniform2iv(GLint location, GLsizei count, const GLint* v);
  void Uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z);
  void Uniform3fv(GLint location, GLsizei count, const GLfloat* v);
  void Uniform3i(GLint location, GLint x, GLint y, GLint z);
  void Uniform3iv(GLint location, GLsizei count, const GLint* v);
  void Uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
  void Uniform4fv(GLint location, GLsizei count, const GLfloat* v);
  void Uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w);
  void Uniform4iv(GLint location, GLsizei count, const GLint* v);
  void UniformMatrix2fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value);
  void UniformMatrix3fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value);
  void UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose,
                        const GLfloat* value);
  void UseProgram(GLuint program);
  void VertexAttribPointer(GLuint indx, GLint size, GLenum type,
                           GLboolean normalized, GLsizei stride,
                           const void* ptr);

  // Fucntions from the GL_OES_EGL_image extension
  void EGLImageTargetTexture2DOES(GLenum target, GLeglImageOES image);
  void EGLImageTargetRenderbufferStorageOES(GLenum target,
                                            GLeglImageOES image);

 private:
  typedef std::vector<std::string> string_vector;

  string_vector extensions_;

  EGLDisplay egl_display_;

  PFNEGLCREATEIMAGEKHRPROC egl_create_image_khr_;
  PFNEGLDESTROYIMAGEKHRPROC egl_destroy_image_khr_;

  PFNGLEGLIMAGETARGETTEXTURE2DOESPROC
      gl_egl_image_target_texture_2d_oes_;
  PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC
      gl_egl_image_target_renderbuffer_storage_oes_;

  DISALLOW_COPY_AND_ASSIGN(RealGles2Interface);
};

}  // namespace window_manager

#endif
