// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_BASE_CALLBACK_H_
#define PLATFORM_BASE_CALLBACK_H_

#include <google/protobuf/stubs/common.h>

// We currently use the callbacks from the protobuf code.
// TODO: Better callbacks.

namespace chromeos {

using google::protobuf::Closure;
using google::protobuf::NewPermanentCallback;
using google::protobuf::NewCallback;

}  // namespace chromeos

#endif  // PLATFORM_BASE_PORT_H_
