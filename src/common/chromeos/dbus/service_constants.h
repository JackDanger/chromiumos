// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
#define CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

#include <glib.h>

// To conform to the GError conventions...
#define CHROMEOS_LOGIN_ERROR chromeos_login_error_quark()
GQuark chromeos_login_error_quark();

namespace cryptohome {
extern const char *kCryptohomeInterface;
extern const char *kCryptohomeServicePath;
extern const char *kCryptohomeServiceName;
extern const char *kCryptohomeIsMounted;
extern const char *kCryptohomeMount;
extern const char *kCryptohomeUnmount;
}  // namespace cryptohome

namespace login_manager {
extern const char *kSessionManagerInterface;
extern const char *kSessionManagerServicePath;
extern const char *kSessionManagerServiceName;
extern const char *kSessionManagerEmitLoginPromptReady;
extern const char *kSessionManagerStartSession;
extern const char *kSessionManagerStopSession;

// Also, conforming to GError conventions
typedef enum {
  CHROMEOS_LOGIN_ERROR_INVALID_EMAIL,  // email address is illegal.
  CHROMEOS_LOGIN_ERROR_EMIT_FAILED,    // could not emit upstart signal.
  CHROMEOS_LOGIN_ERROR_SESSION_EXISTS  // session already exists for this user.
} ChromeOSLoginError;

}  // namespace login_manager

#endif  // CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
