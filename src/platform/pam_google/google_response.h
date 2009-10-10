// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// An interface that can take buffer of data with a response from Google
// in it and fill itself in with the data.

#ifndef CHROMEOS_PAM_GOOGLE_RESPONSE_H_
#define CHROMEOS_PAM_GOOGLE_RESPONSE_H_

#include <string>

namespace chromeos_pam {

extern const char kGoogleErrorString[];

class GoogleResponse {
 public:
  GoogleResponse();
  virtual ~GoogleResponse();

  // |buffer| _must_ be null-terminated.
  // Returning true means that we successfully parsed "buffer", NOT that
  // we did not find an error.  The caller MUST also call IsError() in
  // order to determine whether or not the parsed response indicated
  // success.
  virtual bool Parse(const char *buffer) = 0;
  virtual bool Export() = 0;
  virtual bool IsError() { return !error_.empty(); }

 protected:
  // Error will be non-empty ONLY if there is an error to report.
  std::string error_;

};
}  // chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_RESPONSE_H_
