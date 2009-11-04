// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_OBSOLETE_LOGGING_H_
#define CHROMEOS_OBSOLETE_LOGGING_H_

#include "base/logging.h"

// These macros are defined in glog/logging.h but not in base/logging.h.
// As part of syncing up with the chrome base libraries, these calls are
// provided, they should not be used otherwise.

#define VLOG(n) LOG(INFO)
#define CHECK_EQ(x, y) CHECK((x) == (y))
#define CHECK_NE(x, y) CHECK((x) != (y))
#define CHECK_GT(x, y) CHECK((x) > (y))
#define CHECK_GE(x, y) CHECK((x) >= (y))
#define CHECK_LT(x, y) CHECK((x) < (y))
#define CHECK_LE(x, y) CHECK((x) <= (y))


#endif /* CHROMEOS_OBSOLETE_LOGGING_H_ */

