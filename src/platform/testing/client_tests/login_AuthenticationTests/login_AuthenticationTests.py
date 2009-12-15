# Copyright (c) 2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class login_AuthenticationTests(test.test):
  """
  Tests if we can successfully authenticate to google using credentials listed
  in authentication-test.dat.
  """

  version = 1
  def run_once(self):
    fd = open(os.path.join(self.bindir, "authentication-test.dat"))
    # TODO(gauravsh): Change this to read data from a simple CSV file rather
    # than doing an eval()
    auth_test_data = eval(fd.read())
    try:
      success = self.RunAuthenticationTests(auth_test_data)
      if not success:
        raise error.TestFail("Failed to Authenticate")
    except error.TestFail, tfe:
      raise tfe
    except Exception, e:
      raise error.TestError(e)

  def RunAuthenticationTests(self,auth_data):
    pipe_path = '/tmp/cookie_pipe'
    for username, properties in auth_data.iteritems():
      login_cmd = os.path.join(self.bindir, "login_AuthenticationTest")
      password = properties.get("password", None)
      cmdline = [login_cmd, username, password]
      return_code = subprocess.call(cmdline)
      if return_code == 255:
        return False
      if return_code == 1:
        raise error.TestError("Test failed")
      if os.path.exists(pipe_path):
        """if we successfully authenticated, there's a process waiting to send
        us cookies on a named pipe.  We need to read them."""
        for line in file(pipe_path):
          pass
        os.unlink(pipe_path)
    return True
