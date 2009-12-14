# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time, os.path, logging, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class logging_CollectPerformanceMetrics(test.test):
    version = 1    

    def run_once(self):
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
              raise error.TestFail('Chrome OS metrics file is corrupt')
            else:
              name = name_value_split[0]
              value = name_value_split[1]
            self.write_perf_keyval({name : value})                                  
      except IOError, e:
        print e
        raise error.TestFail('Chrome OS metrics file is missing')