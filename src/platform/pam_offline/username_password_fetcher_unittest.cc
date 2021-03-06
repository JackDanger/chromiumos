// Copyright (c) 2009-2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_offline/username_password_fetcher.h"
#include <gtest/gtest.h>
#include <security/pam_ext.h>
#include "pam_offline/username_password.h"
#include "pam_offline/pam_prompt_wrapper.h"

namespace pam_offline {

const char kFakeUser[] = "fakeuser";
const char kFakePass[] = "fakepass";

class UsernamePasswordFetcherTest : public ::testing::Test { };

class PamPromptWrapperMock : public PamPromptWrapper {
 public:
  PamPromptWrapperMock() {}
  ~PamPromptWrapperMock() {}
  int GetUsername(pam_handle_t *pamh, char *response, int response_len) {
    int len = std::min(static_cast<int>(strlen(kFakeUser)), response_len - 1);
    strncpy(response, kFakeUser, len);
    response[len] = '\0';
    return PAM_SUCCESS;
  }
  int GetPassword(pam_handle_t *pamh, char *response, int response_len) {
    int len = std::min(static_cast<int>(strlen(kFakePass)), response_len - 1);
    strncpy(response, kFakePass, len);
    response[len] = '\0';
    return PAM_SUCCESS;
  }
};

TEST(UsernamePasswordFetcherTest, FetchTest) {
  PamPromptWrapperMock mock;
  UsernamePasswordFetcher fetcher(&mock);
  UsernamePassword *cred =
      reinterpret_cast<UsernamePassword*>(
          fetcher.FetchCredentials(reinterpret_cast<pam_handle_t*>(7)));
  // The '7' above is some dummy non-NULL value.  I check for NULL in the
  // code, but since this is a mock, I just need a "happy signal" for my
  // pam handle.  7 is adequate.
  EXPECT_EQ(0, strcmp(kFakeUser, cred->username_));
  EXPECT_EQ(0, strcmp(kFakePass, cred->password_));
  delete cred;
}

class FailingUsernamePromptWrapper : public PamPromptWrapper {
 public:
  FailingUsernamePromptWrapper(int return_code) : return_code_(return_code) {}
  ~FailingUsernamePromptWrapper() {}
  int GetUsername(pam_handle_t *pamh, char *response, int response_len) {
    return return_code_;
  }
  int GetPassword(pam_handle_t *pamh, char *response, int response_len) {
    return PAM_SUCCESS;
  }
 private:
  int return_code_;
};

class FailingPasswordPromptWrapper : public PamPromptWrapper {
 public:
  FailingPasswordPromptWrapper(int return_code) : return_code_(return_code) {}
  ~FailingPasswordPromptWrapper() {}
  int GetUsername(pam_handle_t *pamh, char *response, int response_len) {
    return return_code_;
  }
  int GetPassword(pam_handle_t *pamh, char *response, int response_len) {
    return PAM_SUCCESS;
  }
 private:
  int return_code_;
};

bool FetchFailureHelper(int return_code, bool fail_username) {
  Credentials *cred;
  PamPromptWrapper *wrapper;
  // This pam handle is fake and never meant to be used.  Why 7?  I
  // check for NULL in the code, but since this is a mock, I just need
  // a "happy signal" for my pam handle.  7 is adequate.
  pam_handle_t *pamh = reinterpret_cast<pam_handle_t*>(7);
  if (fail_username) {
    wrapper = new FailingUsernamePromptWrapper(return_code);
  } else {
    wrapper = new FailingPasswordPromptWrapper(return_code);
  }
  UsernamePasswordFetcher err_fetcher(wrapper);
  cred = err_fetcher.FetchCredentials(pamh);
  delete wrapper;
  bool ret = (NULL == cred);
  delete cred;
  return ret;
}

TEST(UsernamePasswordFetcherTest, FetchUsernameBufErrTest) {
  EXPECT_TRUE(FetchFailureHelper(PAM_BUF_ERR, true));
}

TEST(UsernamePasswordFetcherTest, FetchPasswordBufErrTest) {
  EXPECT_TRUE(FetchFailureHelper(PAM_BUF_ERR, false));
}

TEST(UsernamePasswordFetcherTest, FetchUsernameConvErrTest) {
  EXPECT_TRUE(FetchFailureHelper(PAM_CONV_ERR, true));
}

TEST(UsernamePasswordFetcherTest, FetchPasswordConvErrTest) {
  EXPECT_TRUE(FetchFailureHelper(PAM_CONV_ERR, false));
}

TEST(UsernamePasswordFetcherTest, FetchUsernameSystemErrTest) {
  EXPECT_TRUE(FetchFailureHelper(PAM_SYSTEM_ERR, true));
}

TEST(UsernamePasswordFetcherTest, FetchPasswordSystemErrTest) {
  EXPECT_TRUE(FetchFailureHelper(PAM_SYSTEM_ERR, false));
}

}  // namespace pam_offline
