import os
from autotest_lib.server import installable_object

class SiteAutotest(installable_object.InstallableObject):

    def get(self, location = None):
        if not location:
            location = os.path.join(self.serverdir, '../client')
            location = os.path.abspath(location)
        installable_object.InstallableObject.get(self, location)
        self.got = True

