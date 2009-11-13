// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <exception>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <security/pam_appl.h>
#include <security/pam_ext.h>
#include <security/pam_misc.h>

#include "base/basictypes.h"
//#include "base/ref_ptr.h"
#include "base/scoped_ptr.h"
#include <base/logging.h>
#include <gflags/gflags.h>

DEFINE_string(user, "", "user to log in");
DEFINE_string(pass, "", "password");

using namespace std;

int PamConversationCallback(int num_msg, const struct pam_message **msg,
                            struct pam_response **resp, void *credentials) {
  *resp = new struct pam_response[num_msg];
  for (int i = 0; i < num_msg; i++) {
    resp[i]->resp = 0;
    resp[i]->resp_retcode = 0;
    switch (msg[i]->msg_style) {
      case PAM_PROMPT_ECHO_ON:
        resp[i]->resp = strdup(FLAGS_user.c_str());
        break;
      case PAM_PROMPT_ECHO_OFF:
        resp[i]->resp = strdup(FLAGS_pass.c_str());
        break;
      default:
        break;
    }
  }
  return PAM_SUCCESS;
}

int fake_conv(int nmsgs, const struct pam_message **pmsgs,
              struct pam_response **resp, void *credentials)
{
  int i;
  *resp = new struct pam_response[nmsgs];

  for (i = 0; i < nmsgs; i++) {
    const struct pam_message *msg = &(*pmsgs)[i];
    (*resp)[i].resp_retcode = 0;

    switch (msg->msg_style) {
      case PAM_PROMPT_ECHO_OFF:
        cout << "PAM_PROMPT_ECHO_OFF" << msg->msg << endl;
        cout << "sending: " << FLAGS_pass.c_str() << endl;

        (*resp)[i].resp = strdup(FLAGS_pass.c_str());
        break;
      case PAM_PROMPT_ECHO_ON:
        cout << "PAM_PROMPT_ECHO_ON" << msg->msg << endl;
        (*resp)[i].resp = strdup(FLAGS_user.c_str());
        cout << "sending: " << FLAGS_user.c_str() << endl;
        break;
      case PAM_ERROR_MSG:
        cout << "PAM_ERROR_MSG" << endl;
        (*resp)[i].resp = NULL;
        cerr << msg->msg << endl;
        break;
      case PAM_TEXT_INFO:
        cout << "PAM_TEXT_INFO" << endl;
        (*resp)[i].resp = NULL;
        cerr << msg->msg << endl;
        break;
      default:
        break;
    }
  }
  return PAM_SUCCESS;
}


int main(int argc, char** argv) {
  google::SetUsageMessage("test_login --user user@domain --pass passwd");
  google::ParseCommandLineFlags(&argc, &argv, true);
  // logging::InitLogging(argv[0],
  //                      logging::LOG_ONLY_TO_FILE,
  //                      logging::DONT_LOCK_LOG_FILE,
  //                      logging::APPEND_TO_OLD_LOG_FILE);

  if (FLAGS_user.empty() || FLAGS_pass.empty()) {
    google::ShowUsageWithFlags("");
    exit(1);
  }

  const struct pam_conv conv = { PamConversationCallback, NULL };
  //const struct pam_conv conv = { fake_conv, NULL };

  int retval;
  // do a thing with PAM
  //PamPromptWrapper wrapper;
  //scoped_ptr<pam_handle_t> pamh;
  pam_handle_t *pamh=NULL;

  retval = pam_start("test_login", FLAGS_user.c_str(), &conv, &pamh);
  if (retval == PAM_SUCCESS)
    retval = pam_authenticate(pamh, 0);
  if (retval == PAM_SUCCESS)
    retval = pam_acct_mgmt(pamh, 0);

  if (retval == PAM_SUCCESS) {
    cout << "Authenticated " << FLAGS_user << endl;
  } else {
    cout << "Not Authenticated." << endl;
  }
  //pam_setcred??

  int result = pam_end(pamh, retval);
  if (result != PAM_SUCCESS) {
    pamh = NULL;
    cout << "Couldn't release authenticator" << endl;
    exit(1);
  }

  //pam_open_session
  // pam_close_session
  // grab env and do something like execle the xsession?
  // pam_getenvlist
  // exec Xsession

  exit(retval == PAM_SUCCESS ? 0:1);
}
