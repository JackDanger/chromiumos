# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class ui_ScreenSaverRunning(test.test):
    version = 1

    def run_once(self):
        # check if the screensaver process is running and alive
        try:
            utils.system('export DISPLAY=:0.0 && xscreensaver-command -version')
        except error.CmdError, e:
            logging.debug(e)
            raise error.TestFail('xscreensaver is not alive')
