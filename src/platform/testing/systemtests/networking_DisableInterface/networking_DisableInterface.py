import logging, re, utils
from autotest_lib.client.bin import test
from autotest_lib.client.common_lib import error

class networking_DisableInterface(test.test):
    version = 1

    def run_once(self, iface_name = 'eth0'):
        forced_up = False

        # bring up the interface if its not already up
        if not self.is_iface_up(iface_name):
            utils.system('ifconfig %s up' % iface_name)
            if not self.is_iface_up(iface_name):
                raise error.TestFail('interface failed to come up')
            forced_up = True

        # bring interface down
        utils.system('ifconfig %s down' % iface_name)
        if self.is_iface_up(iface_name):
            raise error.TestFail('interface failed to go down')

        # if initial interface state was down, don't bring it back up
        if forced_up:
            return
        
        # bring interface back up
        utils.system('ifconfig %s up' % iface_name)
        if not self.is_iface_up(iface_name):
            raise error.TestFail('interface failed to come up')


    def is_iface_up(self, name):
        try:
            out = utils.system_output('ifconfig %s' % name)
        except error.CmdError, e:
            logging.info(e)
            raise error.TestNAError('test interface not found')

        match = re.search('UP', out, re.S)
        return match
