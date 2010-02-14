// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is supposed to be defined before the pam includes.
#define PAM_SM_AUTH

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <stdio.h>
#include <stdlib.h>

#include "pam_offline/credentials.h"
#include "pam_offline/authenticator.h"
#include "pam_offline/pam_prompt_wrapper.h"
#include "pam_offline/username_password_fetcher.h"

const char kUserName[] = "chronos";

static void setcred_free(pam_handle_t *pamh /*unused*/,
                         void *ptr,
                         int err /*unused*/) {
  if (ptr) {
    int *intptr = reinterpret_cast<int*>(ptr);
    delete intptr;
  }
}

// PAM framework looks for these entry-points to pass control to the
// authentication module.

// pam_sm_authenticate() will decrypt something using the given creds
// and return success if the something decrypts successfully, failure
// otherwise.
PAM_EXTERN int pam_sm_authenticate(pam_handle_t * pamh, int flags,
                                   int argc, const char **argv) {
  // "flags" can contain PAM_SILENT, which means we shouldn't emit
  // any messages, and PAM_DISALLOW_NULL_AUTHTOK, which means that
  // unknown users should NOT be silently logged in.
  //
  // TODO(cmasone): support PAM_SILENT
  // TODO(cmasone): Should we behave as though DISALLOW_NULL_AUTHTOK
  // is always set?  I think so...

  // ret_data points to some space that we use to store our return
  // value for later use in pam_sm_setcred
  int retval = PAM_AUTH_ERR;
  int *ret_data = new int;

  pam_offline::PamPromptWrapper pam;
  pam_offline::UsernamePasswordFetcher fetcher(&pam);
  pam_offline::Credentials *credentials = fetcher.FetchCredentials(pamh);

  // If fetcher.FetchCredentials times out you get NULL credentials
  if (credentials) {
    pam_offline::Authenticator auth;

    if (auth.Init()) {
      if (auth.TestAllMasterKeys(*credentials)) {
        retval = PAM_SUCCESS;
        pam_set_item(pamh, PAM_USER,
                     reinterpret_cast<const void*>(kUserName));
      } else {
        LOG(INFO) << "Invalid credentials.";
      }
    } else {
      LOG(ERROR) << "Authenticator failed to Init().";
    }

    delete credentials;
  } else {
    LOG(INFO) << "FetchCredentials returned NULL.";
  }

  *ret_data = retval;
  pam_set_data(pamh, "unix_setcred_return",
               reinterpret_cast<void *>(ret_data), setcred_free);
  return retval;
}

// This function is copied from pam_unix_auth.c, in the pam_unix
// module of Linux-PAM.  Apparently, PAM client programs expect this
// function to return the same value as pam_sm_authenticate, so we
// grab the value from the place we stored it in memory above and
// return that.  If this is called BEFORE pam_sm_authenticate, just
// return PAM_SUCCESS;
//
PAM_EXTERN int pam_sm_setcred(pam_handle_t *pamh,
                              int flags /*unused*/,
                              int argc /*unused*/,
                              const char **argv /*unused*/) {
  int retval;
  const void *pretval = NULL;
  retval = PAM_SUCCESS;

  if (PAM_SUCCESS == pam_get_data(pamh,
                                  "unix_setcred_return",
                                  &pretval)
      && pretval) {
    retval = *reinterpret_cast<const int *>(pretval);
    pam_set_data(pamh, "unix_setcred_return", NULL, NULL);
  }

  return retval;
}

// TODO(cmasone) : put stubs in for the pam functions that should
// never be called.
#ifdef PAM_STATIC
struct pam_module _pam_offline_modstruct = {
  "pam_offline",
  pam_sm_authenticate,
  pam_sm_setcred,
  NULL,
  NULL,
  NULL,
  NULL,
};
#endif
