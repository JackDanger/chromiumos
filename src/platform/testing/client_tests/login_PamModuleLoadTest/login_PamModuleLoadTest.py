# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import subprocess

from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class login_PamModuleLoadTest(test.test):
  """
   Tests that pam_google.so, the Linux-PAM module that
   implements authentication with a Google account, exists and can be
   dynamically loaded properly.
  """
  version = 1

  def run_once(self):
    """
    the return value from os.system is a 16-bit number in which the high-
    order byte is the exit code, and the low-order byte is the signal
    which caused the exit, if any.
    """
    exit_code = subprocess.call(os.path.join(self.bindir, "pam_loader_ia32"))
    if exit_code != 0:
      print 'exit_code was %d' % exit_code
      if exit_code == 1:
        raise error.TestFail("Could not dlopen the module")
      elif exit_code == 2:
        raise error.TestFail("Could not find pam_sm_authenticate")
      else:
        raise error.TestFail("Unknown exit code from pam_loader")
