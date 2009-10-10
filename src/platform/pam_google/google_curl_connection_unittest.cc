// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for GoogleCurlConnection.

#include "pam_google/google_curl_connection.h"
#include "pam_google/google_username_password.h"
#include "pam_google/google_cookies.h"
#include "pam_google/google_authenticator.h"
#include <gtest/gtest.h>

namespace chromeos_pam {

class GoogleCurlConnectionTest : public ::testing::Test { };

TEST(GoogleCurlConnectionTest, WriteDataSuccessTest) {
  GoogleCurlConnection appender;
  void *voidp = reinterpret_cast<void*>(&appender);
  char data[] = "data";
  int len = WriteData(data, 1, strlen(data), voidp);
  EXPECT_EQ(0, strncmp(appender.buffer_, data, len));
}

TEST(GoogleCurlConnectionTest, WriteCookieSuccessTest) {
  GoogleCurlConnection appender;
  void *voidp = reinterpret_cast<void*>(&appender);
  WriteCookies("data\n", 1, strlen("data\n"), voidp);
  string cookie(kCookieHeader);
  cookie.append("some cookie stuff\n");
  int len = WriteCookies(cookie.c_str(), 1, cookie.length(), voidp);
  WriteCookies("data\n", 1, strlen("data\n"), voidp);
  string output(appender.buffer_, len);
  EXPECT_EQ(cookie, output);
}

TEST(GoogleCurlConnectionTest, CantFitWriteDataTest) {
  GoogleCurlConnection appender;
  appender.SetCantFit();
  void *voidp = reinterpret_cast<void*>(&appender);
  char data[] = "data";
  int len = WriteData(data, 1, strlen(data), voidp);
  EXPECT_EQ(0, len);
}

TEST(GoogleCurlConnectionTest, AppendTest) {
  GoogleCurlConnection appender;
  char data[] = "data";
  appender.AppendIfPossible(data, 2);
  appender.AppendIfPossible(data+2, strlen(data) - 2);
  EXPECT_EQ(0, strncmp(appender.buffer_, data, strlen(data)));
  EXPECT_EQ(strlen(data), appender.current_);
}

TEST(GoogleCurlConnectionTest, CopyResponseTooEarlyTest) {
  GoogleCurlConnection appender;
  char buffer[2];
  EXPECT_TRUE(GOOGLE_FAILED ==
              appender.CopyAuthenticationResponse(buffer, 2));
}

TEST(GoogleCurlConnectionTest, CopyResponseTooBigTest) {
  GoogleCurlConnection appender;
  char buffer[3];
  char data[] = "data";
  appender.AppendIfPossible(data, strlen(data));
  EXPECT_TRUE(GOOGLE_NOT_ENOUGH_SPACE ==
              appender.CopyAuthenticationResponse(buffer, 3));
}

// long is used in this class, because that's what the curl interface uses.
class CurlMock : public CurlWrapper {
 public:
  CurlMock(CURLcode login_code,
           CURLcode token_code,
           CURLcode cookie_code,
           long login_response,
           long token_response,
           long cookie_response)
      : login_return_code_(login_code),
        token_return_code_(token_code),
        cookie_return_code_(cookie_code),
        login_response_(login_response),
        token_response_(token_response),
        cookie_response_(cookie_response),
        login_called_(false),
        token_fetched_(false) {
  }
  virtual ~CurlMock() {}
  CURLcode do_curl_easy_perform(CURL *curl) {
    if (!login_called_) {
      return login_return_code_;
    } else if (!token_fetched_) {
      return token_return_code_;
    } else {
      return cookie_return_code_;
    }
  }
  CURLcode do_curl_easy_get_response_code(CURL *curl, long *response) {
    if (!login_called_) {
      login_called_ = true;
      *response = login_response_;
      return login_return_code_;
    } else if (!token_fetched_) {
      token_fetched_ = true;
      *response = token_response_;
      return token_return_code_;
    } else {
      *response = cookie_response_;
      return cookie_return_code_;
    }
  }
 private:
  CURLcode login_return_code_;
  CURLcode token_return_code_;
  CURLcode cookie_return_code_;
  long login_response_;
  long token_response_;
  long cookie_response_;
  bool login_called_;
  bool token_fetched_;
};

const int kHttpFail = 500;

TEST(GoogleCurlConnectionTest, GoodAuthenticationToCookiesAttemptTest) {
  GoogleCurlConnection conn(new CurlMock(CURLE_OK,
                                         CURLE_OK,
                                         CURLE_OK,
                                         kHttpSuccess,
                                         kHttpSuccess,
                                         kHttpSuccess));
  char data[] = "Set-Cookie: data";
  EXPECT_EQ(GOOGLE_OK, conn.AttemptAuthentication(data, strlen(data)));
}

TEST(GoogleCurlConnectionTest, BadLoginTest) {
  GoogleCurlConnection conn(new CurlMock(CURLE_URL_MALFORMAT,
                                         CURLE_OK,
                                         CURLE_OK,
                                         kHttpFail,
                                         kHttpSuccess,
                                         kHttpSuccess));
  char data[] = "data";
  EXPECT_EQ(NETWORK_FAILURE,
            conn.AttemptAuthentication(data, strlen(data)));
}

TEST(GoogleCurlConnectionTest, BadTokenAuthTest) {
  GoogleCurlConnection conn(new CurlMock(CURLE_OK,
                                         CURLE_URL_MALFORMAT,
                                         CURLE_OK,
                                         kHttpSuccess,
                                         kHttpFail,
                                         kHttpSuccess));
  char data[] = "data";
  EXPECT_EQ(NETWORK_FAILURE,
            conn.AttemptAuthentication(data, strlen(data)));
}

TEST(GoogleCurlConnectionTest, FailedLoginTest) {
  GoogleCurlConnection conn(new CurlMock(CURLE_OK,
                                         CURLE_OK,
                                         CURLE_OK,
                                         kHttpFail,
                                         kHttpSuccess,
                                         kHttpSuccess));
  char data[] = "data";
  EXPECT_EQ(GOOGLE_FAILED,
            conn.AttemptAuthentication(data, strlen(data)));
}

TEST(GoogleCurlConnectionTest, FailedTokenAuthTest) {
  GoogleCurlConnection conn(new CurlMock(CURLE_OK,
                                         CURLE_OK,
                                         CURLE_OK,
                                         kHttpSuccess,
                                         kHttpFail,
                                         kHttpSuccess));
  char data[] = "data";
  EXPECT_EQ(GOOGLE_FAILED,
            conn.AttemptAuthentication(data, strlen(data)));
}

TEST(GoogleCurlConnectionTest, BadCookieFetchTest) {
  GoogleCurlConnection conn(
      new CurlMock(CURLE_OK,
                   CURLE_OK,
                   CURLE_URL_MALFORMAT,
                   kHttpSuccess,
                   kHttpSuccess,
                   kHttpFail));
  char data[] = "data";
  EXPECT_EQ(NETWORK_FAILURE,
            conn.AttemptAuthentication(data, strlen(data)));
}

TEST(GoogleCurlConnectionTest, FailedCookieFetchTest) {
  GoogleCurlConnection conn(
      new CurlMock(CURLE_OK,
                   CURLE_OK,
                   CURLE_OK,
                   kHttpSuccess,
                   kHttpSuccess,
                   kHttpFail));
  char data[] = "data";
  EXPECT_EQ(GOOGLE_FAILED,
            conn.AttemptAuthentication(data, strlen(data)));
}

}  // namespace chromeos_pam
