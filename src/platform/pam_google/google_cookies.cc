// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_google/google_cookies.h"

#include <glog/logging.h>
#include <string>

#include "pam_google/cookie_exporter.h"

namespace chromeos_pam {
const char kCookieHeader[] = "Set-Cookie: ";
const char GoogleCookies::cookie_pipe_[] = "/tmp/cookie_pipe";

GoogleCookies::GoogleCookies(CookieExporter *exporter)
    : exporter_(exporter) {
}

GoogleCookies::~GoogleCookies() {}

// |buffer| must be null-terminated
bool GoogleCookies::Parse(const char *buffer) {
  char *error_ptr;
  if (NULL != (error_ptr = strstr(buffer, kGoogleErrorString))) {
    error_.assign(error_ptr + strlen(kGoogleErrorString));
    cookies_.clear();
  } else {
    error_.clear();
    char *header;
    while ( (header = strstr(buffer, kCookieHeader)) ) {
      char *newline = strchrnul(header, '\n');
      header += strlen(kCookieHeader);
      newline += *newline == 0 ? 0 : 1;
      string cookie(header, newline);
      cookies_.push_back(cookie);
      buffer = newline;
    }
    if (cookies_.empty())
      return false;
  }
  return true;
}

bool GoogleCookies::Export() {
  DCHECK(exporter_ != NULL);
  exporter_->Init();
  exporter_->Export(cookies_);
  return true;
}

}  // chromeos_pam
