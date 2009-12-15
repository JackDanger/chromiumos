// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <string>

#include <security/pam_appl.h>
#include <security/pam_misc.h>

#include "pam_client.h"

void usage(const char* prog) {
  std::cerr << "Usage: "<<prog<<" <username> <password>\n";
}

PamClient::UserCredentials user_credentials;

int main(int argc, char* argv[]) {
  if (argc!=3) {
    usage(argv[0]);
    return 1;   // autotest uses error code 1 to mean test error.
  }

  user_credentials.username = argv[1];
  user_credentials.password = argv[2];

  PamClient *client = new PamClient(&user_credentials);
  if (client->Authenticate()) {
    std::cerr << "Authentication Succeeded\n";
    return 0;
  } else {
    std::cerr << "Authentication Failed\n";
    return 255;
  }
}
