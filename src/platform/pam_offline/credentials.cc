// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Credentials is the interface for objects that wrap up a set
// of credentials with which we can authenticate.  At the moment, the
// only implementation of this class is UsernamePassword.

#include "pam_offline/credentials.h"

namespace pam_offline {

Credentials::Credentials() {}
Credentials::~Credentials() {}

}  // namespace pam_offline
