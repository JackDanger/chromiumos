// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for PamPromptWrapper.

#include "pam_google/pam_prompt_wrapper.h"
#include <gtest/gtest.h>

namespace chromeos_pam {

class PamPromptWrapperTest : public ::testing::Test { };

TEST(PamPromptWrapperTest, BadResponsePointerTest) {
  PamPromptWrapper wrapper;
  pam_handle_t *pamh = reinterpret_cast<pam_handle_t*>(7);
  EXPECT_EQ(PAM_BUF_ERR, wrapper.GetUsername(pamh, NULL, 0));
  EXPECT_EQ(PAM_BUF_ERR, wrapper.GetPassword(pamh, NULL, 0));
}

}  // namespace chromeos_pam
