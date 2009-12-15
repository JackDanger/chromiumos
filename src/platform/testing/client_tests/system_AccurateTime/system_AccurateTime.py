import math, re, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class system_AccurateTime(test.test):
    version = 1


    def __get_ntp_server(self, filename):
        config_file = file(filename).read()
        match = re.search(r'\n\s*server (.*)', config_file)
        if not match:
            raise error.TestError('Unable to find NTP server')
        return match.group(1)


    def __parse_ntp_server_offset(self, output):
        match = re.search(r'offset ([-\.\d]+)', output)
        if not match:
            raise error.TestError('Did not find NTP server offset')
        return float(match.group(1))


    def run_once(self, max_offset):
        ntp_server = self.__get_ntp_server('/etc/ntp.conf')
        ntpdate = utils.run('/usr/sbin/ntpdate -q %s' % ntp_server)
        server_offset = self.__parse_ntp_server_offset(ntpdate.stdout)

        if (abs(server_offset) > max_offset):
          raise error.TestError(
              'NTP server time offset was %fs > max offset %ds' %
              (server_offset, drift))
