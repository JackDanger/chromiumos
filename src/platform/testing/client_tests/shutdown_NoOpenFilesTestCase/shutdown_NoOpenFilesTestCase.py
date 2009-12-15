import os
import time
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class shutdown_NoOpenFilesTestCase(test.test):
  version = 1

  def run_once(self):
    # If the shutdown_force_kill_processes file exists, then a previous
    # shutdown encountered processes with open files on one of our stateful
    # partitions at the time that it wanted to unmount them. The lsof output
    # will be recorded in the shutdown_force_kill_prcesses file for you to
    # inspect.
    if os.path.exists("/var/log/shutdown_force_kill_processes"):
      raise error.TestError("Test failed")
