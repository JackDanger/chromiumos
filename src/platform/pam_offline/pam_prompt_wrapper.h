// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// PamPromptWrapper is an exremely thin wrapper class around callbacks
// registered by the user of pam_chromeos_offline.

#ifndef PAM_OFFLINE_PAM_PROMPT_WRAPPER_H_
#define PAM_OFFLINE_PAM_PROMPT_WRAPPER_H_

#include <security/pam_ext.h>
#include "base/basictypes.h"

namespace pam_offline {

extern const char kUserEnvVariable[];

extern const int kMaxUsernameLength;

class PamPromptWrapper {
 public:
  PamPromptWrapper();
  virtual ~PamPromptWrapper();

  virtual int GetUsername(pam_handle_t *pamh, char *response, int response_len);
  virtual int GetPassword(pam_handle_t *pamh, char *response, int response_len);
 private:
  DISALLOW_COPY_AND_ASSIGN(PamPromptWrapper);
};

}  // namespace pam_offline

#endif  // PAM_OFFLINE_PAM_PROMPT_WRAPPER_H_
