import os
from autotest_lib.client.bin import utils, test
from autotest_lib.client.common_lib import error

class network_DhclientLeaseTestCase(test.test):
    """Tests that dhclient can save its lease file."""
    version = 1

    def GetFileLastModified(self, file_name):
        """Returns when a file was last modified, or -1 if the file isn't
        found."""
        try:
          return os.stat(file_name).st_mtime
        except OSError, e:
          return -1


    def run_once(self, lease_file="/var/lib/dhcp3/dhclient.leases"):
        before = self.GetFileLastModified(lease_file)
        utils.system("dhclient")
        after = self.GetFileLastModified(lease_file)
        if after <= before:
          raise error.TestFail("dhclient did not update the lease file")
