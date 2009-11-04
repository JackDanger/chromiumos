// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_google/pipe_writer.h"

#include <cerrno>
#include <cstdlib>

#include <pwd.h>
#include <unistd.h>

namespace chromeos_pam {
void PipeWriter::Export(const std::vector<std::string>& data) {
  pid_t pid = fork();
  if (0 == pid) {
    // Create the FIFO if it does not exist
    umask(0);
    // TODO(cmasone): Can we do some kind of setuid here?
    mknod(pipe_name_.c_str(), S_IFIFO|0644, 0);
    if (pipe_ || (pipe_ = fopen(pipe_name_.c_str(), "w"))) {
      // chown the pipe to the user so that it can be removed by Chrome when
      // it's done reading cookies.
      // TODO(cmasone): use some constant here instead of "chronos"
      struct passwd *pwd = getpwnam("chronos");
      if (pwd != NULL) {
        if (chown(pipe_name_.c_str(), pwd->pw_uid, -1) == -1)
          LOG(WARNING) << "Couldn't chown the cookie pipe: "<< strerror(errno);
      } else {
        LOG(WARNING) << "couldn't look up the user: " << strerror(errno);
      }

      std::vector<std::string>::const_iterator it;
      for (it = data.begin(); it != data.end(); ++it) {
        uint32 length = (*it).length();
        const char *start = (*it).c_str();
        while (length > 0) {
          int data_written = TryWrite(start, length);
          length -= data_written;
          start += data_written;
        }
      }
    }
    exit(0);
  }
}

uint32 PipeWriter::TryWrite(const char *data, const uint32 length) {
  CHECK(data);
  CHECK(length >= 0);
  CHECK(pipe_);
  return fwrite(data, sizeof(char), length, pipe_);
}
}  // namespace chromeos_pam
