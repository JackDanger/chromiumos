// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_PAM_GOOGLE_PIPE_WRITER_H_
#define CHROMEOS_PAM_GOOGLE_PIPE_WRITER_H_

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"

#include "pam_google/cookie_exporter.h"

// Given the name of a pipe, this class writes character data to it.
namespace chromeos_pam {
class PipeWriter : public CookieExporter {
 public:
  explicit PipeWriter(const std::string& pipe_name)
      : pipe_(NULL),
        pipe_name_(pipe_name) {
    CHECK(!pipe_name.empty());
  }
  virtual ~PipeWriter() {
    if (pipe_)
      fclose(pipe_);
  }

  void Init() {}
  void Export(const std::vector<std::string>& data);

 private:
  uint32 TryWrite(const char *data, const uint32 length);

  FILE *pipe_;
  std::string pipe_name_;
};
}  // namespace chromeos_pam

#endif  // CHROMEOS_PAM_GOOGLE_PIPE_WRITER_H_
