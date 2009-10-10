// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Wraps curl_easy_perform() so that it can be mocked out for testing.

#ifndef CHROMEOS_PAM_CURL_WRAPPER_H_
#define CHROMEOS_PAM_CURL_WRAPPER_H_

#include <curl/curl.h>

namespace chromeos_pam {

class CurlWrapper {
 public:
  CurlWrapper() {}
  virtual ~CurlWrapper() {}
  virtual CURLcode do_curl_easy_perform(CURL *curl) {
    return curl_easy_perform(curl);
  }
  virtual CURLcode do_curl_easy_get_response_code(CURL *curl, long *response) {
    return curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, response);
  }
};

}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_CURL_WRAPPER_H_
