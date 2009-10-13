// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/chromeos_login_manager/pam_client.h"

const std::string kServiceName = "chromeos_login_manager";
const std::string kDisplayName = ":0.0";
const std::string kLocalUser = "root";
const std::string kLocalHost = "localhost";

int PamConversationCallback(int num_msg, const struct pam_message **msg,
                            struct pam_response **resp, void *credentials) {
  *resp = new struct pam_response[num_msg];
  PamClient::UserCredentials* user_credentials =
    static_cast<PamClient::UserCredentials*>(credentials);
  for (int i = 0; i < num_msg; i++) {
    resp[i]->resp = 0;
    resp[i]->resp_retcode = 0;
    switch (msg[i]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
        resp[i]->resp = strdup(user_credentials->username.c_str());
        break;
      case PAM_PROMPT_ECHO_OFF:
        resp[i]->resp = strdup(user_credentials->password.c_str());
        break;
    }
  }
  return PAM_SUCCESS;
}

PamClient::PamClient(UserCredentials* user_credentials)
    : pam_handle_(NULL), last_pam_result_(PAM_SUCCESS) {
  // Initialize pam with our service name, no default user name,
  // pam conversation handle and finally our pam handle
  pam_conversation_callback_.conv = PamConversationCallback;
  pam_conversation_callback_.appdata_ptr =
    static_cast<void*>(user_credentials);

  last_pam_result_ = pam_start(kServiceName.c_str(), NULL,
                               &pam_conversation_callback_, &pam_handle_);

  // Set startup items
  last_pam_result_ = pam_set_item(pam_handle_, PAM_TTY, kDisplayName.c_str());
  last_pam_result_ = pam_set_item(pam_handle_, PAM_RHOST, kLocalHost.c_str());
  last_pam_result_ = pam_set_item(pam_handle_, PAM_RUSER, kLocalUser.c_str());
}

PamClient::~PamClient() {
  last_pam_result_ = pam_end(pam_handle_, last_pam_result_);
}

bool PamClient::Authenticate() {
  // TODO(sosa) - add better logging data by adding different cases
  last_pam_result_ = pam_authenticate(pam_handle_, 0);
  return last_pam_result_ == PAM_SUCCESS;
}

bool PamClient::StartSession() {
  // TODO(sosa) - add better logging data by adding different cases
  last_pam_result_ = pam_setcred(pam_handle_, PAM_ESTABLISH_CRED);
  if (last_pam_result_ == PAM_SUCCESS) {
    last_pam_result_ = pam_open_session(pam_handle_, 0);
  }
  return last_pam_result_ == PAM_SUCCESS;
}

bool PamClient::CloseSession() {
  // TODO(sosa) - add better logging data by adding different cases
  last_pam_result_ = pam_close_session(pam_handle_, 0);
  // Do this regardless of last result
  last_pam_result_ = pam_setcred(pam_handle_, PAM_DELETE_CRED);
  return last_pam_result_ == PAM_SUCCESS;
}
