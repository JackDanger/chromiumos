// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class that can take buffer of data with a response from Google in it
// and fill itself in with the data, assuming the data is the set of cookies
// that come back is response to a TokenAuth request to Google.

#ifndef CHROMEOS_PAM_GOOGLE_COOKIES_H_
#define CHROMEOS_PAM_GOOGLE_COOKIES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "gtest/gtest.h"
#include "pam_google/google_response.h"

using namespace std;

namespace chromeos_pam {
class CookieExporter;

extern const char kCookieHeader[];

class GoogleCookies : public GoogleResponse {
 public:
  explicit GoogleCookies(CookieExporter *exporter);
  virtual ~GoogleCookies();

  virtual bool Parse(const char *buffer);
  virtual bool Export();

 private:
  static const char cookie_pipe_[];

  vector<string> cookies_;
  CookieExporter *exporter_;

  FRIEND_TEST(GoogleAuthenticatorTest, TestAcceptableCredentials);
  FRIEND_TEST(GoogleCookiesTest, GenerateRunScriptTest);
  FRIEND_TEST(GoogleCookiesTest, ParseAndExportTest);

  DISALLOW_COPY_AND_ASSIGN(GoogleCookies);
};

}  // chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_COOKIES_H_
