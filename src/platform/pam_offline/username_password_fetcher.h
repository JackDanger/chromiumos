// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// A class that can speak back to xscreensaver (and, potentially, other
// pam-using authentication software) to get the user's username
// and password.

#ifndef PAM_OFFLINE_USERNAME_PASSWORD_FETCHER_H_
#define PAM_OFFLINE_USERNAME_PASSWORD_FETCHER_H_

#include <security/pam_ext.h>

#include "base/basictypes.h"
#include "pam_offline/credentials.h"
#include "pam_offline/credential_fetcher.h"

namespace pam_offline {

class PamPromptWrapper;

class UsernamePasswordFetcher : public CredentialFetcher {
 public:
  explicit UsernamePasswordFetcher(PamPromptWrapper *pam);
  ~UsernamePasswordFetcher();

  // Queries the user for their authentication credentials.
  //
  // Returns:
  //  NULL upon failure, a freshly allocated object with the user's
  //  credentials in it upon success.  Caller takes ownership.
  //
  Credentials* FetchCredentials(pam_handle_t *pamh);

 private:
  PamPromptWrapper *pam_;
  DISALLOW_COPY_AND_ASSIGN(UsernamePasswordFetcher);
};

}  // namespace pam_offline

#endif  // PAM_OFFLINE_USERNAME_PASSWORD_FETCHER_H_
