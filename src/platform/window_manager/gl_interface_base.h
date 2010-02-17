// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WINDOW_MANAGER_GL_INTERFACE_BASE_H_
#define WINDOW_MANAGER_GL_INTERFACE_BASE_H_

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace window_manager {

// This is an abstract base class representing any kind of GL
// interface, so that we can pass them opaquely into the
// TidyInterface without knowing if it is OpenGL or OpenGL|ES.
class GLInterfaceBase {
 public:
  GLInterfaceBase() {}
  virtual ~GLInterfaceBase() {}
 protected:
  // Parse an OpenGL extension string, adding all of the available extensions
  // to the out vector
  static void ParseExtensionString(std::vector<std::string>* out,
                                   const char* extensions);

  // Check the vector of strings for the named extension
  static bool HasExtension(const std::vector<std::string>& extensions,
                           const char* extension);
 private:
  DISALLOW_COPY_AND_ASSIGN(GLInterfaceBase);
};

};  // namespace window_manager

#endif  //  WINDOW_MANAGER_GL_INTERFACE_BASE_H_
