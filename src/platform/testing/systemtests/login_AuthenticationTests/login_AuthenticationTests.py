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
    except Exception, e:
      raise error.TestError("Error running the test");

   def RunAuthenticationTests(self,auth_data):
    for username, properties in auth_data.iteritems():
      login_cmd = "./login_AuthenticationTest"
      password = properties.get("password", None)
      cmdline = [login_cmd, username, password]
      return_code = subprocess.call(cmdline)
      if return_code == 255:
        return false
      if return_code == 1:
        raise error.TestError("Test failed")
    return true
