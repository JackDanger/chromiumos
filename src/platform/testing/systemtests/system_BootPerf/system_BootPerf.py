# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, re, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class system_BootPerf(test.test):
    version = 1


    def __parse_uptime_login_prompt_ready(self, results):
        data = file('/tmp/uptime-login-prompt-ready').read()
        vals = re.split(r' +', data.strip())
        results['uptime_login_prompt_ready'] = vals[0]


    def __parse_disk_login_prompt_ready(self, results):
        data = file('/tmp/disk-login-prompt-ready').read()
        vals = re.split(r' +', data.strip())
        results['sectors_read_login_prompt_ready'] = vals[2]


    def run_once(self):
        # Parse key metric files and generate key/value pairs
        results = {}
        self.__parse_uptime_login_prompt_ready(results)
        self.__parse_disk_login_prompt_ready(results)
        self.write_perf_keyval(results)
