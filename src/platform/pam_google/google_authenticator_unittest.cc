// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for GoogleAuthenticator.

// TODO: uses many mock classes...maybe we should get gMock up in here?

#include "pam_google/google_authenticator.h"
#include <gtest/gtest.h>
#include <security/pam_ext.h>
#include <unistd.h>
#include "pam_google/cookie_exporter.h"
#include "pam_google/google_credentials.h"
#include "pam_google/google_connection.h"
#include "pam_google/google_cookies.h"

namespace chromeos_pam {

class GoogleAuthenticatorTest : public ::testing::Test { };

const char google_error[] = "Error=BadAuthentication";
const char google_happy[] = "Set-Cookie: Happy";
const char formatted[] = "Formatted credentials";

class LocalAccountCredentials : public GoogleCredentials {
 public:
  LocalAccountCredentials() {}
  ~LocalAccountCredentials() {}
  int Format(char *payload, int length) { return 0; }
  void GetActiveUser(char *name_buffer, int length) {}
  void GetActiveUserFull(char *name_buffer, int length) {}
  void GetWeakHash(char *name_buffer, int length) {}
  bool IsLocalAccount() { return true; }
  bool IsAcceptable() { return false; }
  void StoreCredentials() {}
  bool ValidForOfflineLogin() { return false; }
};

TEST(GoogleAuthenticatorTest, TestLocalAccountCredentials) {
  LocalAccountCredentials bdc;
  GoogleAuthenticator authenticator;
#ifdef CHROMEOS_PAM_LOCALACCOUNT
  EXPECT_EQ(PAM_SUCCESS, authenticator.Authenticate(&bdc, NULL, NULL));
#else
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&bdc, NULL, NULL));
#endif
}

class UnacceptableCredentials : public GoogleCredentials {
 public:
  UnacceptableCredentials() {}
  ~UnacceptableCredentials() {}
  int Format(char *payload, int length) { return 0; }
  void GetActiveUser(char *name_buffer, int length) {}
  void GetActiveUserFull(char *name_buffer, int length) {}
  void GetWeakHash(char *name_buffer, int length) {}
  bool IsLocalAccount() { return false; }
  bool IsAcceptable() { return false; }
  void StoreCredentials() {}
  bool ValidForOfflineLogin() { return false; }
};

TEST(GoogleAuthenticatorTest, TestUnacceptableCredentials) {
  UnacceptableCredentials uac;
  GoogleAuthenticator authenticator;
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&uac, NULL, NULL));
}

class AcceptableCredentials : public GoogleCredentials {
 public:
  AcceptableCredentials() {}
  ~AcceptableCredentials() {}
  int Format(char *payload, int length) {
    strncpy(payload, formatted, length);
    return strlen(formatted);
  }
  void GetActiveUser(char *name_buffer, int length) {}
  void GetActiveUserFull(char *name_buffer, int length) {}
  void GetWeakHash(char *name_buffer, int length) {}
  bool IsLocalAccount() { return false; }
  bool IsAcceptable() { return true; }
  void StoreCredentials() {}
  bool ValidForOfflineLogin() { return false; }
};

class WorkingConnection : public GoogleConnection {
 public:
  WorkingConnection() {}
  ~WorkingConnection() {}
  GoogleReturnCode AttemptAuthentication(const char *payload,
                                       const int length) {
    EXPECT_EQ(0, strncmp(payload, formatted, length));
    return GOOGLE_OK;
  }
  GoogleReturnCode CopyAuthenticationResponse(char *output_buffer,
                                            const int length) {
    strncpy(output_buffer, google_happy, length);
    return GOOGLE_OK;
  }
};

class MockExporter : public CookieExporter {
 public:
  MockExporter() {}
  virtual ~MockExporter() {}

  void Init() {}
  void Export(const std::vector<std::string>& data) {
    string cookie_output(kCookieHeader);
    EXPECT_FALSE(data.empty());
    cookie_output.append(data[0]);
  }
};

TEST(GoogleAuthenticatorTest, TestAcceptableCredentials) {
  AcceptableCredentials ac;
  WorkingConnection wc;
  MockExporter writer;
  GoogleAuthenticator authenticator;
  EXPECT_EQ(PAM_SUCCESS, authenticator.Authenticate(&ac, &wc, &writer));
}

class BrokenConnection : public GoogleConnection {
 public:
  BrokenConnection() {}
  ~BrokenConnection() {}
  GoogleReturnCode AttemptAuthentication(const char *payload,
                                       const int length) {
    EXPECT_EQ(0, strncmp(payload, formatted, length));
    return GOOGLE_FAILED;
  }
  GoogleReturnCode CopyAuthenticationResponse(char *output_buffer,
                                            const int length) {
    EXPECT_TRUE(false);  // If we even get here, that's bad.
    return GOOGLE_OK;
  }
};

TEST(GoogleAuthenticatorTest, TestAcceptableCredentialsBrokenConnection) {
  AcceptableCredentials ac;
  BrokenConnection wc;
  GoogleAuthenticator authenticator;
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&ac, &wc, NULL));
}

class DenyingConnection : public GoogleConnection {
 public:
  DenyingConnection() {}
  ~DenyingConnection() {}
  GoogleReturnCode AttemptAuthentication(const char *payload,
                                       const int length) {
    EXPECT_EQ(0, strncmp(payload, formatted, length));
    return GOOGLE_OK;
  }
  GoogleReturnCode CopyAuthenticationResponse(char *output_buffer,
                                            const int length) {
    strncpy(output_buffer, google_error, length);
    return GOOGLE_OK;
  }
};

TEST(GoogleAuthenticatorTest, TestAcceptableCredentialsFailingAuth) {
  AcceptableCredentials ac;
  DenyingConnection dc;
  GoogleAuthenticator authenticator;
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&ac, &dc, NULL));
}

class NocopyConnection : public DenyingConnection {
 public:
  NocopyConnection() {}
  ~NocopyConnection() {}
  GoogleReturnCode CopyAuthenticationResponse(char *output_buffer,
                                            const int length) {
    return GOOGLE_NOT_ENOUGH_SPACE;
  }
};

TEST(GoogleAuthenticatorTest, TestAcceptableCredentialsCantCopy) {
  AcceptableCredentials ac;
  NocopyConnection wc;
  GoogleAuthenticator authenticator;
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&ac, &wc, NULL));
}

class FailcopyConnection : public DenyingConnection {
 public:
  FailcopyConnection() {}
  ~FailcopyConnection() {}
  GoogleReturnCode CopyAuthenticationResponse(char *output_buffer,
                                            const int length) {
    return GOOGLE_FAILED;
  }
};

TEST(GoogleAuthenticatorTest, TestAcceptableCredentialsFailCopy) {
  AcceptableCredentials ac;
  FailcopyConnection wc;
  GoogleAuthenticator authenticator;
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&ac, &wc, NULL));
}

class OfflineCredentials : public GoogleCredentials {
 public:
  OfflineCredentials() {}
  ~OfflineCredentials() {}
  int Format(char *payload, int length) {
    strncpy(payload, formatted, length);
    return strlen(formatted);
  }
  void GetActiveUser(char *name_buffer, int length) {}
  void GetActiveUserFull(char *name_buffer, int length) {}
  void GetWeakHash(char *name_buffer, int length) {}
  bool IsLocalAccount() { return false; }
  bool IsAcceptable() { return true; }
  void StoreCredentials() {}
  bool ValidForOfflineLogin() { return true; }
};

TEST(GoogleAuthenticatorTest, TestOfflineCredentialsBrokenConnOfflineFirst) {
  OfflineCredentials oc;
  BrokenConnection wc;
  GoogleAuthenticator authenticator;

  authenticator.set_offline_first(true);
  EXPECT_EQ(PAM_SUCCESS, authenticator.Authenticate(&oc, &wc, NULL));

  authenticator.set_offline_first(false);
  EXPECT_EQ(PAM_AUTH_ERR, authenticator.Authenticate(&oc, &wc, NULL));
}


}  // namespace chromeos_pam
