# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, time, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class ui_ChromeKillRestart(test.test):
    version = 1

    def run_once(self, binary = '/opt/google/chrome/chrome'):
        # Try to kill all running instances of the binary.
        try:
            utils.system('pkill -KILL -f %s' % binary)
        except error.CmdError, e:
            logging.debug(e)
            raise error.TestFail('%s is not running before kill' % binary)

        # Give the system a chance to restart the binary.
        time.sleep(3)

        # Check if the binary is running again.
        try:
            utils.system('pgrep -f %s' % binary)
        except error.CmdError, e:
            logging.debug(e)
            raise error.TestFail('%s is not running after kill' % binary)
