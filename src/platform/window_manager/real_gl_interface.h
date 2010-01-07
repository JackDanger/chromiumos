// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_REAL_GL_INTERFACE_H_
#define WINDOW_MANAGER_REAL_GL_INTERFACE_H_

#include "window_manager/gl_interface.h"

namespace window_manager {

class RealXConnection;

// This wraps an actual GL interface so that we can mock it and use it
// for testing.
class RealGLInterface : public GLInterface {
 public:
  explicit RealGLInterface(RealXConnection* connection);

  void GlxFree(void* item);

  GLXPixmap CreateGlxPixmap(GLXFBConfig config,
                          XPixmap pixmap,
                          const int* attrib_list);
  void DestroyGlxPixmap(GLXPixmap pixmap);
  GLXContext CreateGlxContext(XVisualInfo* vis);
  void DestroyGlxContext(GLXContext context);
  void SwapGlxBuffers(GLXDrawable drawable);
  Bool MakeGlxCurrent(GLXDrawable drawable,
                    GLXContext ctx);
  GLXFBConfig* GetGlxFbConfigs(int* nelements);
  XVisualInfo* GetGlxVisualFromFbConfig(GLXFBConfig config);
  int GetGlxFbConfigAttrib(GLXFBConfig config,
                         int attribute,
                         int* value);
  void BindGlxTexImage(GLXDrawable drawable,
                        int buffer,
                        int* attrib_list);
  void ReleaseGlxTexImage(GLXDrawable drawable,
                           int buffer);

  // GL Functions
  void BindBuffer(GLenum target, GLuint buffer);
  void BindTexture(GLenum target, GLuint texture);
  void BlendFunc(GLenum sfactor, GLenum dfactor);
  void BufferData(GLenum target, GLsizeiptr size, const GLvoid* data,
                  GLenum usage);
  void Clear(GLbitfield mask);
  void Color4f(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
  void DeleteBuffers(GLsizei n, const GLuint* buffers);
  void DeleteTextures(GLsizei n, const GLuint* textures);
  void DepthMask(GLboolean flag);
  void Disable(GLenum cap);
  void DisableClientState(GLenum array);
  void DrawArrays(GLenum mode, GLint first, GLsizei count);
  void Enable(GLenum cap);
  void EnableClientState(GLenum cap);
  void Finish();
  void GenBuffers(GLsizei n, GLuint* buffers);
  void GenTextures(GLsizei n, GLuint* textures);
  GLenum GetError();
  void LoadIdentity();
  void MatrixMode(GLenum mode);
  void Ortho(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
             GLdouble near, GLdouble far);
  void PushMatrix();
  void PopMatrix();
  void Rotatef(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
  void Scalef(GLfloat x, GLfloat y, GLfloat z);
  void TexCoordPointer(GLint size, GLenum type, GLsizei stride,
                       const GLvoid* pointer);
  void TexParameteri(GLenum target, GLenum pname, GLint param);
  void Translatef(GLfloat x, GLfloat y, GLfloat z);
  void VertexPointer(GLint size, GLenum type, GLsizei stride,
                     const GLvoid* pointer);

 private:
  RealXConnection* xconn_;
  DISALLOW_COPY_AND_ASSIGN(RealGLInterface);
};

}  // namespace window_manager

#endif  // WINDOW_MANAGER_REAL_GL_INTERFACE_H_
