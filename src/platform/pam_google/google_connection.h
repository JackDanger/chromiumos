// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An interface for classes that talk to Google's Authentication service.

#ifndef CHROMEOS_PAM_GOOGLE_CONNECTION_H_
#define CHROMEOS_PAM_GOOGLE_CONNECTION_H_

#include "base/basictypes.h"

namespace chromeos_pam {

enum GoogleReturnCode {
  GOOGLE_FAILED,
  GOOGLE_OK,
  GOOGLE_NOT_ENOUGH_SPACE,
  NETWORK_FAILURE  // offline login is acceptable, tho
};

class GoogleConnection {
 public:
  GoogleConnection();
  virtual ~GoogleConnection();

  virtual GoogleReturnCode AttemptAuthentication(const char *payload,
                                                 const int length) = 0;
  virtual GoogleReturnCode CopyAuthenticationResponse(
      char *output_buffer, const int length) = 0;
 private:
  DISALLOW_COPY_AND_ASSIGN(GoogleConnection);
};

}  // chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_CONNECTION_H_
