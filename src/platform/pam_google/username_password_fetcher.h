// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class that can speak back to slim (and, potentially, other
// pam-using authentication software) to get the user's username
// and password.

#ifndef CHROMEOS_PAM_USERNAME_PASSWORD_FETCHER_H_
#define CHROMEOS_PAM_USERNAME_PASSWORD_FETCHER_H_

#include <security/pam_ext.h>
#include "base/basictypes.h"
#include "pam_google/credential_fetcher.h"

namespace chromeos_pam {

class GoogleCredentials;
class OfflineCredentialStore;
class PamPromptWrapper;

class UsernamePasswordFetcher : public CredentialFetcher {
 public:
  explicit UsernamePasswordFetcher(PamPromptWrapper *pam);
  ~UsernamePasswordFetcher();

  /**
   * FetchCredentials
   * Queries the user for her authentication credentials.
   *
   * Returns:
   *  NULL upon failure, a freshly allocated object with the user's
   *  credentials in it upon success.  Caller takes ownership.
   */
  GoogleCredentials* FetchCredentials(pam_handle_t *pamh,
                                      OfflineCredentialStore *store);
 private:
  PamPromptWrapper *pam_;
  DISALLOW_COPY_AND_ASSIGN(UsernamePasswordFetcher);
};

}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_USERNAME_PASSWORD_FETCHER_H_
