// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is supposed to be defined before the pam includes.
#define PAM_SM_AUTH

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <base/command_line.h>
#include <base/logging.h>
#include <curl/curl.h>
#include <gflags/gflags.h>
#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>
#include <stdio.h>
#include <stdlib.h>

#include "pam_google/google_authenticator.h"
#include "pam_google/google_curl_connection.h"
#include "pam_google/google_username_password.h"
#include "pam_google/offline_credential_store.h"
#include "pam_google/pam_prompt_wrapper.h"
#include "pam_google/username_password_fetcher.h"
#include "pam_google/pipe_writer.h"

// We map all users to the "chronos" user, at least for now.
const char kUserName[] = "chronos";
const char kCookiePipe[] = "/tmp/cookie_pipe";

const char kPamArgOfflineFirst[] = "offline_first";
const unsigned int kPamArgOfflineFirstLength = sizeof(kPamArgOfflineFirst) - 1;

#define AUTH_RETURN                                               \
do {                                                              \
  *ret_data = retval;                                             \
  pam_set_data(pamh, "unix_setcred_return",                       \
               reinterpret_cast<void *>(ret_data), setcred_free); \
  return retval;                                                  \
} while (0)

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

/**
 * pam_sm_authenticate() performs GOOGLE authentication against live,
 * external GOOGLE servers
 */
PAM_EXTERN int pam_sm_authenticate(pam_handle_t * pamh, int flags,
                                   int argc, const char **argv) {
  // "flags" can contain PAM_SILENT, which means we shouldn't emit
  // any messages, and PAM_DISALLOW_NULL_AUTHTOK, which means that
  // unknown users should NOT be silently logged in.
  //
  // TODO(cmasone): support PAM_SILENT
  // TODO(cmasone): Should we behave as though DISALLOW_NULL_AUTHTOK
  // is always set?  I think so...

  curl_global_init(CURL_GLOBAL_SSL);
  static bool google_logging_initialized = false;
  if (!google_logging_initialized) {
    CommandLine::Init(argc, argv);
    logging::InitLogging(NULL, logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
                         logging::DONT_LOCK_LOG_FILE,
                         logging::APPEND_TO_OLD_LOG_FILE);
    google_logging_initialized = true;
  }

  // Walk arguments and save off anything we actually support.
  bool offline_first = false;
  for (int i = 0; i < argc; ++i) {
    if (!strncmp(kPamArgOfflineFirst, argv[i], kPamArgOfflineFirstLength)) {
      offline_first = true;
      break;  // only offline_first is supported.
    }
  }

  int retval, *ret_data = NULL;
  // ret_data points to some space that we use to store our return
  // value for later use in pam_sm_setcred
  ret_data = new int;
  retval = PAM_AUTH_ERR;

  chromeos_pam::PamPromptWrapper pam;
  chromeos_pam::OfflineCredentialStore store(
      new chromeos_pam::ExportWrapper(pamh));
  chromeos_pam::UsernamePasswordFetcher fetcher(&pam);
  chromeos_pam::GoogleCredentials *credentials =
      fetcher.FetchCredentials(pamh, &store);
  // If fetcher.FetchCredentials times out you get NULL credentials
  if (credentials) {
    chromeos_pam::GoogleCurlConnection conn;
    chromeos_pam::GoogleAuthenticator authenticator;
    chromeos_pam::PipeWriter writer(kCookiePipe);
    authenticator.set_offline_first(offline_first);
    retval = authenticator.Authenticate(credentials, &conn, &writer);

    if (PAM_SUCCESS == retval) {
      pam_set_item(pamh,
                   PAM_USER,
                   reinterpret_cast<const void*>(kUserName));
      LOG(INFO) << "returning PAM_SUCCESS";
    } else {
      LOG(INFO) << "returning " << retval;
    }

    delete credentials;
  } else {
    LOG(INFO) << "FetchCredentials timed out.  Returning failure.";
  }

  *ret_data = retval;
  pam_set_data(pamh, "unix_setcred_return",
               reinterpret_cast<void *>(ret_data), setcred_free);
  return retval;
}

/**
 * This function is copied from pam_unix_auth.c, in the pam_unix
 * module of Linux-PAM.  Apparently, PAM client programs expect this
 * function to return the same value as pam_sm_authenticate, so we
 * grab the value from the place we stored it in memory above and
 * return that.  If this is called BEFORE pam_sm_authenticate, just
 * return PAM_SUCCESS;
 */
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
struct pam_module _pam_google_modstruct = {
  "pam_google",
  pam_sm_authenticate,
  pam_sm_setcred,
  NULL,
  NULL,
  NULL,
  NULL,
};
#endif
