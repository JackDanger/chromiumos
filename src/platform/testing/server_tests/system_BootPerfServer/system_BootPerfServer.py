# Copyright (c) 2009 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
import shutil
from autotest_lib.server import test, autotest

class system_BootPerfServer(test.test):
    version = 1

    def run_once(self, host=None):
        self.client = host
        self.client_test = 'system_BootPerf'

        # Reboot the client
        logging.info('BootPerfServer: reboot %s' % self.client.hostname)
        self.client.reboot()

        # Collect the performance metrics by running a client side test
        client_at = autotest.Autotest(self.client)
        client_at.run_test(self.client_test)


    def postprocess(self):
        logging.info('BootPerfServer: postprocess %s' % self.client.hostname)

        # Promote the client test keyval as our own
        src = os.path.join(self.outputdir, self.client_test, "results/keyval")
        dst = os.path.join(self.resultsdir, "keyval")
        if os.path.exists(src):
            shutil.copy(src, dst)
        else:
            logging.warn('Unable to locate %s' % src)
