// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

namespace cryptohome {
const char *kCryptohomeInterface = "org.chromium.CryptohomeInterface";
const char *kCryptohomeServiceName = "org.chromium.Cryptohome";
const char *kCryptohomeServicePath = "/org/chromium/Cryptohome";
const char *kCryptohomeIsMounted = "IsMounted";
const char *kCryptohomeMount = "Mount";
const char *kCryptohomeUnmount = "Unmount";
}  // namespace cryptohome

namespace login_manager {
const char *kSessionManagerInterface = "org.chromium.SessionManagerInterface";
const char *kSessionManagerServiceName = "org.chromium.SessionManager";
const char *kSessionManagerServicePath = "/org/chromium/SessionManager";
const char *kSessionManagerEmitLoginPromptReady = "EmitLoginPromptReady";
const char *kSessionManagerStartSession = "StartSession";
const char *kSessionManagerStopSession = "StopSession";
}  // namespace login_manager
