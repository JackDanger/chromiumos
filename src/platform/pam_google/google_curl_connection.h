// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class that connects to Google using libcurl, and provides the
// ability to present an authentication request and to read the
// response from the server.

#ifndef CHROMEOS_PAM_GOOGLE_CURL_CONNECTION_H_
#define CHROMEOS_PAM_GOOGLE_CURL_CONNECTION_H_

#include <curl/curl.h>
#include <glog/logging.h>
#include <gtest/gtest.h>
#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "pam_google/curl_wrapper.h"
#include "pam_google/google_connection.h"

// A wrapper around curl_easy_setopt() that CHECKs for success.
//
// curl_easy_setopt() is declared as a variable argument function,
// probably not for the variable number of args but as a way to avoid
// declaring the third parameter and overloading the function in plain C.
// Let's use a macro here to work with this. One reason to prefer a
// macro over a templated function is that with the macro we preserve
// the CHECK line number and details for more precise debugging.
#define CURL_CHECK_SETOPT(curl_handle, option, ...) \
  do { \
    CHECK_EQ(CURLE_OK, curl_easy_setopt(curl_handle, option, __VA_ARGS__)); \
  } while (0)


namespace chromeos_pam {

extern const char kClientLoginUrl[];
extern const char kIssueAuthTokenUrl[];
extern const char kTokenAuthUrl[];
extern const char kService[];
extern const int kHttpSuccess;

class GoogleCurlConnection : public GoogleConnection {
 public:
  GoogleCurlConnection();
  explicit GoogleCurlConnection(CurlWrapper *wrapper);
  virtual ~GoogleCurlConnection();

  GoogleReturnCode AttemptAuthentication(const char *payload,
                                         const int length);
  GoogleReturnCode CopyAuthenticationResponse(
      char *output_buffer, const int length);

  bool CanFit(size_t bytes) {
    if (cant_fit_) {
      return false;
    } else {
      return bytes <= (sizeof(buffer_) - current_);
    }
  }
  int AppendIfPossible(const void *incoming, size_t incoming_bytes);

 private:
  void Reset() {
    current_ = 0;
  }

  GoogleReturnCode GoogleTransaction(CURL *curl,
                                     const string& url,
                                     const string& post_body);

  // For testing.
  void SetCantFit() { cant_fit_ = true; }

  bool cant_fit_;
  char buffer_[4096];  // 4k is how big a header can be.
  int current_;
  scoped_ptr<CurlWrapper> curl_wrapper_;

  FRIEND_TEST(GoogleCurlConnectionTest, AppendTest);
  FRIEND_TEST(GoogleCurlConnectionTest, CantFitWriteDataTest);
  FRIEND_TEST(GoogleCurlConnectionTest, GoodAuthenticationToCookiesAttemptTest);
  FRIEND_TEST(GoogleCurlConnectionTest, WriteCookieSuccessTest);
  FRIEND_TEST(GoogleCurlConnectionTest, WriteDataSuccessTest);
  DISALLOW_COPY_AND_ASSIGN(GoogleCurlConnection);
};

// These are callbacks for libcurl to use when it has data to write.
// libcurl seems to fail at passing parameters if this function is
// a member of a class.
// According to curl docs, the amount of data stored at "incoming"
// is size_of_element * number_of_elements.  We expect a
// GoogleCurlConnection* passed in as "userp".
size_t WriteData(const void *incoming, size_t size_of_element,
                 size_t number_of_elements, void *userp);
size_t WriteCookies(const void *incoming, size_t size_of_element,
                    size_t number_of_elements, void *userp);

}  // chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_CURL_CONNECTION_H_
