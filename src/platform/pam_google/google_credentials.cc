// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GoogleCredentials is the interface for objects that wrap up a set
// of credentials with which we can authenticate to Google.

#include "pam_google/google_credentials.h"

namespace chromeos_pam {

GoogleCredentials::GoogleCredentials() {}
GoogleCredentials::~GoogleCredentials() {}

}  // namespace chromeos_pam
