// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
#define CHROMEOS_DBUS_SERVICE_CONSTANTS_H_

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
}  // namespace login_manager

#endif  // CHROMEOS_DBUS_SERVICE_CONSTANTS_H_
