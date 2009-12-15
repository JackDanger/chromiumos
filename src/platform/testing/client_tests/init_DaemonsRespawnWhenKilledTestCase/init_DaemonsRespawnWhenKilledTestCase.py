import os
import time
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class init_DaemonsRespawnWhenKilledTestCase(test.test):
  version = 1

  def run_once(self):
    return_code = os.system(self.bindir + "/test_respawn.sh") >> 8
    if return_code == 255:
      raise error.TestError("Test failed")
    elif return_code == 1:
      raise error.TestFail("Test error")
