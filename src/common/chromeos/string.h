// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_STRING_H_
#define CHROMEOS_STRING_H_

#include <string>
#include <vector>

namespace chromeos {

void SplitString(const std::string& str, std::vector<std::string>* parts);

void SplitStringUsing(const std::string& str,
                      const std::string& delim,
                      std::vector<std::string>* parts);

}  // namespace chromeos

#endif /* CHROMEOS_STRING_H_ */

