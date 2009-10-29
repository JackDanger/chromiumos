import logging
import os
from autotest_lib.client.bin import utils, test
from autotest_lib.client.common_lib import error

class ltp(test.test):
    version = 1

    def setup(self, tarball = 'ltp-20090731-compiled.ia32.tar.bz2'):
        tarball = utils.unmap_url(self.bindir, tarball, self.tmpdir)
        utils.extract_tarball_to_dir(tarball, self.srcdir)
        os.chdir(self.srcdir)

    # Note: to run a specific test, try '-f cmdfile -s test' in the
    # in the args (-f for test file and -s for the test case)
    # eg, job.run_test('ltp', '-f math -s float_bessel')
    def run_once(self, args = '', script = 'runltp', ignore_tests = []):
        # In case the user wants to run another test script
        if script == 'runltp':
            logfile = os.path.join(self.resultsdir, 'ltp.log')
            failcmdfile = os.path.join(self.resultsdir, 'failcmdfile')
            outputfile = os.path.join(self.resultsdir, 'ltp.output')
            htmlfile = os.path.join(self.resultsdir, 'ltp.html')
            args2 = ('-p -q -l %s -o %s -f ../../chromeos.tests -C %s -g %s' %
                     (logfile, outputfile, failcmdfile, htmlfile))
            args = args + ' ' + args2

        cmd = os.path.join(self.srcdir, script) + ' ' + args
        logging.info(os.getcwd())
        logging.info(cmd)
        result = utils.run(cmd, ignore_status=True)

        # look for the first line in result.stdout containing FAIL and,
        # if found, raise the whole line as a reason of the test failure.
        for line in result.stdout.split():
            if 'FAIL' in line:
                test_name = line.strip().split(' ')[0]
                if not test_name in ignore_tests:
                    raise error.TestFail(line)

