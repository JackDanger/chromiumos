// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// CredentialFetcher is an interface for classes that interface with
// users to get their credentials.

#ifndef PAM_OFFLINE_CREDENTIAL_FETCHER_H_
#define PAM_OFFLINE_CREDENTIAL_FETCHER_H_

#include <security/pam_ext.h>

namespace pam_offline {

class Credentials;

class CredentialFetcher {
 public:
  CredentialFetcher() {}
  virtual ~CredentialFetcher() {}

  // FetchCredentials
  // Queries the user for her authentication credentials.
  //
  //
  // Parameters:
  //  pamh - Pointer to a pam_handle
  //
  // Returns:
  //  NULL upon failure, a freshly allocated object with the user's
  //  credentials in it upon success.  Caller takes ownership.
  //
  virtual Credentials* FetchCredentials(pam_handle_t *pamh) = 0;
};

}  // namespace pam_offline

#endif  // PAM_OFFLINE_CREDENTIAL_FETCHER_H_
