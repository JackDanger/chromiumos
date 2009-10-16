// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include "verity.h"

namespace chromeos {

const char *Verity::kManifestFile = "/.verity_manifest";
const char *Verity::kManifestDigest = "64a17769525546b19ea554ff27848544d621cdda";
const char *Verity::kDigestAlgorithm = "sha1"; // = sha1
const int Verity::kHexDigestLength = 40;  // 40 from sha
const int Verity::kBlockSize = 4096;  // 4096
const unsigned int Verity::kMaxTableSize = (1024*1024*1024) / kBlockSize;

};  // namespace chromeos
