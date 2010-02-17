// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "window_manager/gl_interface_base.h"

#include <string>
#include <vector>

namespace window_manager {

void GLInterfaceBase::ParseExtensionString(std::vector<std::string>* out,
                                           const char* extensions) {
  std::string ext(extensions);
  for (std::string::size_type pos = 0; pos != std::string::npos;) {
    std::string::size_type last_pos = ext.find_first_of(" ", pos);
    out->push_back(ext.substr(pos, last_pos - pos));
    pos = ext.find_first_not_of(" ", last_pos);
  }
}

bool GLInterfaceBase::HasExtension(const std::vector<std::string>& extensions,
                                   const char* extension) {
  for (std::vector<std::string>::const_iterator i = extensions.begin();
       i != extensions.end(); ++i) {
    if (*i == extension)
      return true;
  }
  return false;
}

}

