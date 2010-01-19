#include <gtest/gtest.h>

#include <security/_pam_macros.h>
#include <security/pam_modules.h>
#include <security/pam_ext.h>

int pam_sm_authenticate(pam_handle_t * pamh, int flags,
                        int argc, const char **argv);

namespace pam_offline {

class PamPromptWrapperTest : public ::testing::Test { };

TEST(PamPromptWrapperTest, PamOfflineTest) {
  pam_sm_authenticate(NULL, 0, 0, NULL);
}

}  // namespace pam_offline
