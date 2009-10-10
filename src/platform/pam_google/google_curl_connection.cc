// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class that connects to Google using libcurl, and provides the
// ability to present an authentication request and to read the
// response from the server.

#include "pam_google/google_curl_connection.h"
#include <curl/curl.h>
#include <glog/logging.h>
#include <string>
#include "pam_google/curl_wrapper.h"
#include "pam_google/google_cookies.h"

namespace chromeos_pam {

const char kLoginTrustRoot[] = "/etc/login_trust_root.pem";
const char kClientLoginUrl[] =
    "https://www.google.com/accounts/ClientLogin";
const char kIssueAuthTokenUrl[] =
    "https://www.google.com/accounts/IssueAuthToken";
const char kService[] = "service=gaia";
const char kTokenAuthUrl[] =
  "https://www.google.com/accounts/TokenAuth?"
  "continue=http://www.google.com/&source=hourglass&auth=";
const int kHttpSuccess = 200;
const int kMaxRedirs = 3;

GoogleCurlConnection::GoogleCurlConnection()
    : cant_fit_(false),
      current_(0),
      curl_wrapper_(new CurlWrapper) {
}

GoogleCurlConnection::GoogleCurlConnection(CurlWrapper *wrapper)
    : cant_fit_(false),
      current_(0),
      curl_wrapper_(wrapper) {
}

GoogleCurlConnection::~GoogleCurlConnection() {
  memset(buffer_, 0, sizeof(buffer_));
}

size_t WriteData(const void *incoming, size_t size_of_element,
                 size_t number_of_elements, void *userp) {
  // according to curl docs, the amount of data stored at
  // "incoming" is size_of_element * number_of_elements.
  int incoming_data = size_of_element * number_of_elements;
  GoogleCurlConnection *conn =
      reinterpret_cast<GoogleCurlConnection*>(userp);
  return conn->AppendIfPossible(incoming, incoming_data);
}

size_t WriteCookies(const void *incoming, size_t size_of_element,
                    size_t number_of_elements, void *userp) {
  // according to curl docs, the amount of data stored at
  // "incoming" is size_of_element * number_of_elements.
  int incoming_data = size_of_element * number_of_elements;
  GoogleCurlConnection *conn =
      reinterpret_cast<GoogleCurlConnection*>(userp);
  if (strncmp(kCookieHeader,
              reinterpret_cast<const char*>(incoming),
              strlen(kCookieHeader)) == 0) {
    return conn->AppendIfPossible(incoming, incoming_data);
  }
  return incoming_data;
}

GoogleReturnCode GoogleCurlConnection::GoogleTransaction(CURL *curl,
    const string& url, const string& post_body) {
  CURLcode code;
  long server_response_code;
  CURL_CHECK_SETOPT(curl, CURLOPT_URL, url.c_str());
  if (!post_body.empty()) {
    CURL_CHECK_SETOPT(curl, CURLOPT_POST, 1);
    CURL_CHECK_SETOPT(curl, CURLOPT_POSTFIELDSIZE, post_body.length());
    CURL_CHECK_SETOPT(curl, CURLOPT_POSTFIELDS, post_body.c_str());
  } else {
    CURL_CHECK_SETOPT(curl, CURLOPT_POST, 0);
  }
  code = curl_wrapper_->do_curl_easy_perform(curl);

  // TODO(cmasone): add better error handling.
  if (CURLE_OK != code) {
    LOG(WARNING) << "Curl failed to connect: " << curl_easy_strerror(code);
    return NETWORK_FAILURE;
  }

  code = curl_wrapper_->do_curl_easy_get_response_code(curl,
                                                       &server_response_code);
  // TODO(cmasone): add better error handling.
  if (CURLE_OK != code) {
    LOG(WARNING) << "Curl could not determine HTTP response code: "
                 << curl_easy_strerror(code);
    return NETWORK_FAILURE;
  } else if (server_response_code != kHttpSuccess) {
    LOG(WARNING) << "HTTP return code is " << server_response_code;
    return GOOGLE_FAILED;
  }
  return GOOGLE_OK;
}

GoogleReturnCode GoogleCurlConnection::AttemptAuthentication(
    const char *payload, const int length) {
  memset(buffer_, 0, sizeof(buffer_));
  current_ = 0;
  CURL *curl = curl_easy_init();
  GoogleReturnCode google_code;
  CURL_CHECK_SETOPT(curl, CURLOPT_WRITEFUNCTION, WriteData);
  CURL_CHECK_SETOPT(curl, CURLOPT_WRITEDATA, this);
  CURL_CHECK_SETOPT(curl, CURLOPT_CAINFO, kLoginTrustRoot);
  CURL_CHECK_SETOPT(curl, CURLOPT_MAXREDIRS, kMaxRedirs);
  CURL_CHECK_SETOPT(curl,
                    CURLOPT_REDIR_PROTOCOLS,
                    CURLPROTO_HTTPS | CURLPROTO_HTTP);

  // Send user's credentials
  string post_body(payload, length);
  LOG(INFO) << "Logging in to Google...";
  google_code = GoogleTransaction(curl, kClientLoginUrl, post_body);
  if (GOOGLE_OK != google_code) {
    LOG(WARNING) << "ClientLogin failed: " << google_code;
    curl_easy_cleanup(curl);
    return google_code;
  }
  LOG(INFO) << "Done!";

  // Send cookies back to Google to convert them into one-time auth token.
  char *newline_ptr = NULL;
  while (NULL != (newline_ptr = strchr(buffer_, '\n'))) {
    *newline_ptr = '&';
  }
  post_body.assign(buffer_);
  post_body.append(kService, strlen(kService));
  Reset();  // Reset our internal buffer.

  LOG(INFO) << "Fetching AuthToken from Google...";
  google_code = GoogleTransaction(curl, kIssueAuthTokenUrl, post_body);
  if (GOOGLE_OK != google_code) {
    LOG(WARNING) << "Fetching AuthToken failed: " << google_code;
    curl_easy_cleanup(curl);
    return google_code;
  }
  LOG(INFO) << "Done.";

  // Send token back to get session cookies.
  string token_url(kTokenAuthUrl);
  token_url.append(buffer_);
  Reset();  // Reset our internal buffer.

  LOG(INFO) << "Getting Google Cookies";
  string empty;
  CURL_CHECK_SETOPT(curl, CURLOPT_HEADER, 1);
  CURL_CHECK_SETOPT(curl, CURLOPT_WRITEFUNCTION, WriteCookies);
  // We follow redirects for the TokenAuth process, because the TokenAuth API
  // uses redirects to handle different google domains; for example, if I
  // have an apps-for-your-domain account that I'm trying to use to
  // authenticate here, when I go to the standard
  // www.google.com/accounts/TokenAuth URL I will be redirected to the
  // appropriate accounts URL for my domain.  If I'm using a normal gmail
  // account, I will not be redirected at all.
  CURL_CHECK_SETOPT(curl, CURLOPT_FOLLOWLOCATION, 1);
  google_code = GoogleTransaction(curl,
                              token_url.c_str(),
                              empty);
  CURL_CHECK_SETOPT(curl, CURLOPT_HEADER, 0);
  CURL_CHECK_SETOPT(curl, CURLOPT_WRITEFUNCTION, WriteData);
  // I turn redirects back off here to reset to the default, in case we wind up
  // adding another step after TokenAuth at some point.
  CURL_CHECK_SETOPT(curl, CURLOPT_FOLLOWLOCATION, 0);
  if (google_code == GOOGLE_OK)
    LOG(INFO) << "Done.";
  else
    LOG(WARNING) << "Getting Cookies from Google failed: " << google_code;
  curl_easy_cleanup(curl);
  return google_code;
}

GoogleReturnCode GoogleCurlConnection::CopyAuthenticationResponse(
    char *output_buffer, const int length) {
  if (current_ == 0) {
    return GOOGLE_FAILED;
  } else if (length < current_) {
    return GOOGLE_NOT_ENOUGH_SPACE;
  }
  strncpy(output_buffer, buffer_, length);
  return GOOGLE_OK;
}

int GoogleCurlConnection::AppendIfPossible(const void *incoming,
                                         size_t incoming_bytes) {
  if (CanFit(incoming_bytes + 1)) {
    memcpy(buffer_ + current_, incoming, incoming_bytes);
    current_ += incoming_bytes;
    // in case anyone's relying on null-termination.
    buffer_[current_] = '\0';
    return incoming_bytes;
  } else {
    return 0;  // causes curl to fail the transfer.
  }
}

}  // chromeos_pam
