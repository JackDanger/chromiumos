/* Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include <EGL/egl.h>

static const char* kClientApis = "OpenGL_ES";
static const char* kExtensions = "";
static const char* kVendor = "Chromium OS";
static const char* kVersion = "1.4";

EGLint eglGetError() {
  /* This EGL stub cannot be initialized.  As such we don't need to track
   * error state, we can always return EGL_NOT_INITIALIZED. */
  return EGL_NOT_INITIALIZED;
}

EGLDisplay eglGetDisplay(EGLNativeDisplayType display_id) {
  return EGL_NO_DISPLAY;
}

EGLBoolean eglInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) {
  /* "EGL_FALSE is reutrned on failure and major and minor are not updated"
   * -- EGL 1.4 Section 3.2 */
  return EGL_FALSE;
}

EGLBoolean eglTerminate(EGLDisplay dpy) {
  return EGL_FALSE;
}

const char* eglQueryString(EGLDisplay dpy, EGLint name) {
  switch (name) {
    case EGL_CLIENT_APIS:
      return kClientApis;
    case EGL_EXTENSIONS:
      return kExtensions;
    case EGL_VENDOR:
      return kVendor;
    case EGL_VERSION:
      return kVersion;
    default:
      return NULL;
  }
}

EGLBoolean eglGetConfigs(EGLDisplay dpy, EGLConfig* configs,
                         EGLint config_size, EGLint* num_config) {
  /* EGL_NOT_INITIALIZED */
  return EGL_FALSE;
}

EGLBoolean eglChooseConfig(EGLDisplay dpy, const EGLint* attrib_list,
                           EGLConfig* configs, EGLint config_size,
                           EGLint* num_config) {
  /* EGL_NOT_INITIALIZED */
  return EGL_FALSE;
}

EGLBoolean eglGetConfigAttrib(EGLDisplay dpy, EGLConfig config,
                              EGLint attribute, EGLint* value) {
  /* EGL_NOT_INITIALIZED */
  return EGL_FALSE;
}

EGLSurface eglCreateWindowSurface(EGLDisplay dpy, EGLConfig config,
                                  EGLNativeWindowType win,
                                  const EGLint* attrib_list) {
  /* EGL_NOT_INITIALIZED */
  return EGL_NO_SURFACE;
}

EGLSurface eglCreatePbufferSurface(EGLDisplay dpy, EGLConfig config,
                                   const EGLint* attrib_list) {
  /* EGL_NOT_INITIALIZED */
  return EGL_NO_SURFACE;
}

EGLSurface eglCreatePixmapSurface(EGLDisplay dpy, EGLConfig config,
                                  EGLNativePixmapType pixmap,
                                  const EGLint* attrib_list) {
  /* EGL_NOT_INITIALIZED */
  return EGL_NO_SURFACE;
}

EGLBoolean eglDestroySurface(EGLDisplay dpy, EGLSurface surface) {
  return EGL_FALSE;
}

EGLBoolean eglQuerySurface(EGLDisplay dpy, EGLSurface surface,
                           EGLint attribute, EGLint* value) {
  /* "eglQuerySurface returns EGL_FALSE on failure and value is not updated."
   * -- EGL 1.4 Section 3.5.6 */
  return EGL_FALSE;
}

EGLBoolean eglBindAPI(EGLenum api) {
  /* "api must specify one of the supported client APIs , either
   * EGL_OPENGL_API, EGL_OPENGL_ES_API, or EGL_OPENVG_API." -- EGL 1.4 Section
   * 3.7
   * We only support EGL_OPENGL_ES_API from the kClientApis string above. */
  if (api == EGL_OPENGL_ES_API)
    return EGL_TRUE;
  return EGL_FALSE;
}

EGLenum eglQueryAPI() {
  /* "The initial value of the current rendering API is EGL_OPENGL_ES_API,
   * unless OpenGL ES is not supported by an implementation, in which case the
   * initial value is EGL_NONE." -- EGL 1.4 Section 3.7
   * We only support EGL_OPENGL_ES_API so this cannot be changed from the
   * initial value. */
  return EGL_OPENGL_ES_API;
}

EGLBoolean eglWaitClient() {
  /* "If there is no current context for the current rendering API, the
   * function has no effect but still returns EGL_TRUE." -- EGL 1.4 Section 3.8
   */
  return EGL_TRUE;
}

EGLBoolean eglReleaseThread() {
  /* "There are no defined conditions under which failure will occur." -- EGL
   * 1.4 Section 3.11 */
  return EGL_TRUE;
}

EGLSurface eglCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype,
                                            EGLClientBuffer buffer,
                                            EGLConfig config,
                                            const EGLint* attrib_list) {
  /* EGL_NOT_INITIALIZED */
  return EGL_NO_SURFACE;
}

EGLBoolean eglSurfaceAttrib(EGLDisplay dpy, EGLSurface surface,
                            EGLint attribute, EGLint value) {
  /* EGL_NOT_INITIALIZED */
  return EGL_FALSE;
}

EGLBoolean eglBindTexImage(EGLDisplay dpy, EGLSurface surface,
                           EGLint buffer) {
  return EGL_FALSE;
}

EGLBoolean eglReleaseTexImage(EGLDisplay dpy, EGLSurface surface,
                              EGLint buffer) {
  return EGL_FALSE;
}

EGLBoolean eglSwapInterval(EGLDisplay dpy, EGLint interval) {
  return EGL_FALSE;
}

EGLContext eglCreateContext(EGLDisplay dpy, EGLConfig config,
                            EGLContext share_context,
                            const EGLint *attrib_list) {
  /* EGL_NOT_INITIALIZED */
  return EGL_NO_CONTEXT;
}

EGLBoolean eglDestroyContext(EGLDisplay dpy, EGLContext ctx) {
  return EGL_FALSE;
}

EGLBoolean eglMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read,
                          EGLContext ctx) {
  /* EGL_NOT_INITIALIZED */
  return EGL_FALSE;
}

EGLContext eglGetCurrentContext() {
  return EGL_NO_CONTEXT;
}

EGLSurface eglGetCurrentSurface(EGLint readdraw) {
  return EGL_NO_SURFACE;
}

EGLDisplay eglGetCurrentDisplay() {
  return EGL_NO_DISPLAY;
}

EGLBoolean eglWaitGL() {
  /* Functionally equivalent to calling WaitClient() with the GL API current.
   */
  return eglWaitClient();
}

EGLBoolean eglQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute,
                           EGLint *value) {
  /* "eglQueryContext returns EGL_FALSE on failure and value is not updated."
   * -- EGL 1.4 Section 3.7.4 */
  return EGL_FALSE;
}

EGLBoolean eglWaitNative(EGLint engine) {
  /* "If there is no current context, the function has no effect but still
   * returns EGL_TRUE." "If engine does not denote a recognized marking engine,
   * EGL_FALSE is returned and an EGL_BAD_PARAMETER error is generated." -- EGL
   * 1.4 Section 3.8 */
  if (engine == EGL_CORE_NATIVE_ENGINE)
    return EGL_TRUE;
  return EGL_FALSE;
}

EGLBoolean eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
  return EGL_FALSE;
}

EGLBoolean eglCopyBuffers(EGLDisplay dpy, EGLSurface surface,
                          EGLNativePixmapType target) {
  return EGL_FALSE;
}

__eglMustCastToProperFunctionPointerType eglGetProcAddress(
    const char* procname) {
  /* "eglGetProcAddress may not be queried for core (non-extension) functions
   * in EGL or client APIs." -- EGL 1.4 Section 3.10. */
  return NULL;
}

