// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_PAM_GOOGLE_AUTHENTICATOR_H_
#define CHROMEOS_PAM_GOOGLE_AUTHENTICATOR_H_

namespace chromeos_pam {
class GoogleCredentials;
class GoogleConnection;
class CookieExporter;

class GoogleAuthenticator {
 public:
  GoogleAuthenticator() : offline_first_(false) {}
  virtual ~GoogleAuthenticator() {}

  virtual int Authenticate(GoogleCredentials *const credentials,
                           GoogleConnection *const conn,
                           CookieExporter *const exporter);
  virtual void set_offline_first(bool offline_first) {
    offline_first_ = offline_first;
  }
 private:
  bool offline_first_;
};

}  // namespace chromeos_pam
#endif  // CHROMEOS_PAM_GOOGLE_AUTHENTICATOR_H_
