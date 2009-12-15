# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class login_AuthenticationEnvVarsTests(test.test):

  version = 1
  def run_once(self):
    fd = open(os.path.join(self.bindir, "authentication-test.dat"))
    # TODO(gauravsh): Change this to read data from a simple CSV file rather
    # than doing an eval()
    auth_test_data = eval(fd.read())
    try:
      self.RunAuthenticationTests(auth_test_data)
    except error.TestFail, tfe:
      raise tfe
    except Exception, e:
      raise error.TestError(e)

  def RunAuthenticationEnvVarsTests(self,auth_data):
    pipe_path = '/tmp/cookie_pipe'
    for username, properties in auth_data.iteritems():
      login_cmd = os.path.join(self.bindir, "login_AuthenticationEnvVarsTest")
      password = properties.get("password", None)
      cmdline = [login_cmd, username, password]
      return_code = subprocess.call(cmdline, shell=False)
      if return_code == 0 and os.path.exists(pipe_path):
        """if we successfully authenticated, there's a process waiting to send
        us cookies on a named pipe.  We need to read them."""
        for line in file(pipe_path):
          pass
        os.unlink(pipe_path)
      elif return_code == 255:
        raise error.TestFail("Authentication or environment setup failed.")
      else:
        raise error.TestError("Unknown test error.")
    return True
