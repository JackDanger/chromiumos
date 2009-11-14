import os
import time
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class shutdown_UnmountCleanlyTestCase(test.test):
  version = 1

  def run_once(self):
    # If either of the shutdown_cryptohome_umount_failure or
    # shutdown_stateful_umount_failure files exist then a previous shutdown
    # was unable to umount one of these successfully. The files will contain
    # the output of the "mount" command at the time of the failure for you to
    # inspect.
    if os.path.exists("/var/log/shutdown_cryptohome_umount_failure"):
      raise error.TestError("Test failed")
    if os.path.exists("/var/log/shutdown_stateful_umount_failure"):
      raise error.TestError("Test failed")
