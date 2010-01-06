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
        
        # Parse other metrics generated from dev library
        
        # Wait for autotest metrics to come in (timeout in v1)
        seconds = 30 
        time.sleep(seconds)
        
        try:              
            # Open the metrics file using with to ensure it's closed
            with open('/tmp/.chromeos-metrics-autotest', 'r') as metrics_file:            
            
                # Write the metric out for autotest to see
                for name_value in metrics_file:
                    name_value_split = name_value.split('=')
                    if (len(name_value_split) != 2):
                        raise error.TestFail('ChromeOS metrics file is corrupt')
                    else:
                        name = name_value_split[0]
                        value = name_value_split[1]
                    self.write_perf_keyval({name : value})                                  
        except IOError, e:
            print e
            raise error.TestFail('ChromeOS metrics file is missing')
