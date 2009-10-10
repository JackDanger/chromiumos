// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CredentialFetcher is an interface for classes that interface with
// users to get credentials with which we can authenticate to Google.

#ifndef CHROMEOS_PAM_CREDENTIAL_FETCHER_H_
#define CHROMEOS_PAM_CREDENTIAL_FETCHER_H_

#include <security/pam_ext.h>

namespace chromeos_pam {

class GoogleCredentials;
class OfflineCredentialStore;

class CredentialFetcher {
 public:
  CredentialFetcher() {}
  virtual ~CredentialFetcher() {}

  /**
   * FetchCredentials
   * Queries the user for her authentication credentials.
   *
   * Returns:
   *  NULL upon failure, a freshly allocated object with the user's
   *  credentials in it upon success.  Caller takes ownership.
   */
  virtual GoogleCredentials* FetchCredentials(pam_handle_t *pamh,
                                            OfflineCredentialStore *store) = 0;
};

}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_CREDENTIAL_FETCHER_H_
