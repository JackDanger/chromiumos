// Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pam_google/pipe_writer.h"

#include <errno.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace chromeos_pam {

class PipeWriterTest : public testing::Test { };

TEST_F(PipeWriterTest, SuccessfulRead) {
  std::string pipe_name("/tmp/MYFIFO");
  std::vector<std::string> data;
  data.push_back("foo");

  PipeWriter writer(pipe_name);
  writer.Export(data);

  // Create the FIFO if it does not exist
  umask(0);
    mknod(pipe_name.c_str(), S_IFIFO|0666, 0);
  int pipe = open(pipe_name.c_str(), O_RDONLY);
  EXPECT_NE(pipe, -1) << strerror(errno);
  char buf[10];
  int data_read = read(pipe, buf, strlen("foo"));
  close(pipe);
  EXPECT_EQ(data_read, strlen("foo"));
  buf[data_read] = 0;
  EXPECT_EQ(0, data[0].compare(buf));
}

}  // namespace chromeos_pam
