// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PamPromptWrapper is an exremely thin wrapper class around callbacks
// registered by the user of pam_google.

#include "pam_google/pam_prompt_wrapper.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "base/logging.h"

namespace chromeos_pam {

// Environment variable that stores the full chrome os user with @
const char kUserEnvVariable[] = "CHROMEOS_USER";

// Max size of full username
const int kMaxUsernameLength = 200;

PamPromptWrapper::PamPromptWrapper() {}

PamPromptWrapper::~PamPromptWrapper() {}

// Use pam_prompt to prompt the user for her username
int PamPromptWrapper::GetUsername(pam_handle_t *pamh, char *response,
                                  int response_len) {
  if (NULL == pamh) {
    LOG(ERROR) << "GetUsername called with no pam handle\n";
    return PAM_ABORT;
  }
  if (NULL == response) {
    LOG(ERROR) << "GetUsername called with no response buffer\n";
    return PAM_BUF_ERR;
  }

  // Let's check to see if we have the user name already
  char *name = getenv(kUserEnvVariable);
  int r = PAM_SUCCESS;

  // If not a blank user, we use that user instead of starting a conversation
  if (NULL != name && strlen(name) > 0) {
     strncpy(response, name, strlen(name));
  } else {
    // PAM_PROMPT_ECHO_ON tells slim we want the username.
    char *resp = NULL;
    r = pam_prompt(pamh, PAM_PROMPT_ECHO_ON, &resp, "%s", "Username: ");
    if (PAM_SUCCESS == r) {
      strncpy(response, resp, response_len);
    }
    if (resp)
      free(resp);
  }
  return r;
}

// Use pam_prompt to prompt the user for her password
int PamPromptWrapper::GetPassword(pam_handle_t *pamh, char *response,
                                  int response_len) {
  if (NULL == pamh) {
    LOG(ERROR) << "GetPassword called with no pam handle\n";
    return PAM_ABORT;
  }
  if (NULL == response) {
    LOG(ERROR) << "GetPassword called with no response buffer\n";
    return PAM_BUF_ERR;
  }
  // PAM_PROMPT_ECHO_OFF tells slim we want the password.
  char *resp = NULL;
  const int r =
      pam_prompt(pamh, PAM_PROMPT_ECHO_OFF, &resp, "%s", "Password: ");
  if (PAM_SUCCESS == r) {
    strncpy(response, resp, response_len);
  }
  if (resp) {
    memset(resp, 0, strlen(resp));
    free(resp);
  }
  // Make sure that PAM_AUTHTOK exists and is not NULL.
  // If it should be something non-empty, that will get taken care of later.
  pam_set_item(pamh, PAM_AUTHTOK, "");
  return r;
}

}  // namespace chromeos_pam
