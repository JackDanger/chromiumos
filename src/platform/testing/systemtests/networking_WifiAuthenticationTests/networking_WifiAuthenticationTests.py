from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

import os
import re
import sys
import time
import wifi


class networking_WifiAuthenticationTests(test.test):
  """
    Tests that we can connect to the routers specified in router-test.dat

    The test attempts to connect to each router using the given password,
    once a connection is established we will verify that the IP returned
    by ifconfig matches an ip range specifed in the same config file.
  """
  version = 1

  def run_once(self):
    fd = open(os.path.join(self.bindir, "router-test.dat"))
    router_test_data = eval(fd.read())
    try:
      success = self.RunConnectionTests(router_test_data)
      if not success:
        raise error.TestFail("failed to connect")
    except Exception, e:
      raise error.TestFail("Error running the test")

  def GetWifiIP(self):
    cli = os.popen(
        "/sbin/ifconfig wlan0 | grep \"inet addr\"").readline().strip(" ")
    r = re.search("inet addr:([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)", cli)
    ip = "NONE"
    try:
      ip = r.group(1)
    except Exception, e:
      print e
    return ip

  def IsConnected(self, ip_prefix):
    wlan_ip = ""
    try:
      wlan_ip = self.GetWifiIP()
    except Exception, e:
      print e

    return wlan_ip.find(ip_prefix) == 0

  def RunConnectionTests(self, routers):
    passed = 0
    failed = 0
    skipped = 0
    for ssid, properties in routers.iteritems():
      available_networks = wifi.Scan()
      if not available_networks.has_key(ssid):
        print "Unable to scan for % s" % ssid
        skipped += 1
        continue
      wifi.AddNetwork(ssid, properties.get("passphrase", None))
      if self.IsConnected(properties.get("ip-prefix", "UNKNOWN")):
        passed += 1
      else:
        failed += 1
      
      return failed == 0

