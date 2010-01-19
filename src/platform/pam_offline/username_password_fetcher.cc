// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class that can speak back to slim (and, potentially, other
// pam-using authentication software) to get the user's username
// and password.

#include "pam_offline/username_password_fetcher.h"

#include "pam_offline/pam_prompt_wrapper.h"
#include "pam_offline/username_password.h"

#include <base/logging.h>
#include <chromeos/utility.h>

namespace pam_offline {

UsernamePasswordFetcher::UsernamePasswordFetcher(PamPromptWrapper *pam)
    : pam_(pam) {
}

UsernamePasswordFetcher::~UsernamePasswordFetcher() {}

// FetchCredentials
// Queries the user for her authentication credentials.
//
// Returns:
//  NULL upon failure, a freshly allocated object with the user's
//  credentials in it upon success.  Caller takes ownership.
//
Credentials* UsernamePasswordFetcher::FetchCredentials(pam_handle_t *pamh) {
  if (NULL == pamh) {
    LOG(INFO) << "FetchCredentials called with NULL pamh";
    return NULL;
  }

  char username[320];
  memset(username, 0, sizeof(username));
  const int password_len(50);
  char password[password_len];
  memset(password, 0, sizeof(password));
  Credentials *cred = NULL;

  if (PAM_SUCCESS == pam_->GetUsername(pamh, username, sizeof(username)) &&
      PAM_SUCCESS == pam_->GetPassword(pamh, password, sizeof(password))) {
    cred = new UsernamePassword(username,
                                strlen(username),
                                password,
                                strlen(password));
  }

  chromeos::SecureMemset(password, 0, password_len);
  return cred;
}

}  // namespace pam_offline
