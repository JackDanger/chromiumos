// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_google/google_cookies.h"

#include <gtest/gtest.h>
#include <string>
#include "base/string_util.h"
#include "base/logging.h"
#include "pam_google/cookie_exporter.h"

namespace chromeos_pam {

class GoogleCookiesTest : public ::testing::Test { };

class RememberingExporter : public CookieExporter {
 public:
  RememberingExporter() {}
  virtual ~RememberingExporter() {}

  void Init() {}
  void Export(const std::vector<std::string>& data) {
    vector<string>::const_iterator it;
    for (it = data.begin(); it != data.end(); ++it) {
      cookie_output_.append(kCookieHeader);
      cookie_output_.append(*it);
    }
  }
  string cookie_output_;
};

TEST(GoogleCookiesTest, ParseAndExportTest) {
  const char first[] = "all\n";
  const char second[] = "the\n";
  const char third[] = "cookies\n";
  string some_cookies = StringPrintf("%s%s%s%s%s%s",
                                     kCookieHeader,
                                     first,
                                     kCookieHeader,
                                     second,
                                     kCookieHeader,
                                     third);
  RememberingExporter exporter;
  GoogleCookies cookies(&exporter);
  cookies.Parse(some_cookies.c_str());
  cookies.Export();

  EXPECT_EQ(some_cookies, exporter.cookie_output_);
}

TEST(GoogleCookiesTest, ErroneousResponseParseTest) {
  char badness[80];
  snprintf(badness, sizeof(badness), "%s=fail", kGoogleErrorString);

  GoogleCookies token(NULL);
  EXPECT_TRUE(token.Parse(badness));
  EXPECT_TRUE(token.IsError());
}

TEST(GoogleCookiesTest, MalformedResponseParseTest) {
  GoogleCookies token(NULL);
  EXPECT_FALSE(token.Parse("gobbledygook"));
}
}  // chromeos_pam
