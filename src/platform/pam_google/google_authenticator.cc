// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_google/google_authenticator.h"
#include <glog/logging.h>
#include <security/pam_ext.h>
#include "pam_google/google_connection.h"
#include "pam_google/google_cookies.h"
#include "pam_google/google_credentials.h"
#include "pam_google/cookie_exporter.h"

namespace chromeos_pam {

int GoogleAuthenticator::Authenticate(GoogleCredentials *const credentials,
                                    GoogleConnection *const conn,
                                    CookieExporter *const exporter) {
  int retval = PAM_AUTH_ERR;

#ifdef CHROMEOS_PAM_LOCALACCOUNT
  if (credentials->IsLocalAccount()) {
    LOG(WARNING) << "Logging in with local account credentials.";
    return PAM_SUCCESS;
  }
#endif

  if (credentials->IsAcceptable()) {
    if (offline_first_ && credentials->ValidForOfflineLogin()) {
        LOG(INFO) << "Offline login success with offline_first";
        retval = PAM_SUCCESS;
    } else {
      char buffer[2048];
      int actual_length = credentials->Format(buffer, sizeof(buffer));
      GoogleReturnCode google_return_code =
          conn->AttemptAuthentication(buffer, actual_length);
      if (GOOGLE_OK == google_return_code) {
        LOG(INFO) << "Successfully talked to Google. Storing credentials for "
                     "future offline login.";
        credentials->StoreCredentials();
        if (GOOGLE_OK == conn->CopyAuthenticationResponse(buffer,
                                                        sizeof(buffer))) {
          LOG(INFO) << "Successfully copied Google response";
          GoogleCookies cookies(exporter);
          if (cookies.Parse(buffer) && !cookies.IsError()) {
            retval = PAM_SUCCESS;
            cookies.Export();
          }
        }
      } else if (NETWORK_FAILURE == google_return_code) {
        LOG(INFO) << "Network failure talking to google. Trying offline login";
        if (credentials->ValidForOfflineLogin()) {
          LOG(INFO) << "Offline login success";
          retval = PAM_SUCCESS;
        } else {
          LOG(INFO) << "Offline login failure";
        }
      }
      memset(buffer, 0, sizeof(buffer));
    }
  }
  return retval;
}

}  // namespace chromeos_pam
