// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_PAM_GOOGLE_COOKIE_EXPORTER_H_
#define CHROMEOS_PAM_GOOGLE_COOKIE_EXPORTER_H_

namespace chromeos_pam {
class CookieExporter {
 public:
  CookieExporter() {}
  virtual ~CookieExporter() {}

  virtual void Init() = 0;
  virtual void Export(const std::vector<std::string>& data) = 0;

};
}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_COOKIE_EXPORTER_H_
