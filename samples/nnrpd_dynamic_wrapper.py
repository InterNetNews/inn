# Example wrapper nnrpd_dynamic_wrapper.py for support of old python
# authentication scripts, by Erik Klavon. 

# This file contains a sample python script which can be used to
# duplicate the behavior of the old nnrppythonauth functionality. This
# script only supports dynamic access control by group.

# How to use this wrapper:
# - insert your authentication class into this file.
# - rename your authentication class OLDAUTH
#

# Old AUTH class
# Insert your old auth class here
# do not include the code which sets the hook



#
# Wrapper AUTH class. It creates an instance of the old class and
# calls its methods. Arguments and return values are munged as
# needed to fit the new way of doing things.
#

class MYAUTH:
    """Provide dynamic access callbacks to nnrpd."""
    def dynamic_init(self):
        self.old = OLDAUTH()

    def dynamic(self, attributes):
        return (self.old).authorize(attributes)

    def dynamic_close(self):
        (self.old).close()

#
# The rest is used to hook up the auth module on nnrpd. It is unlikely
# you will ever need to modify this.
#

# Import functions exposed by nnrpd. This import must succeed, or nothing
# will work!
from nnrpd import *

# Create a class instance
myauth = MYAUTH()

# ...and try to hook up on nnrpd. This would make auth object methods visible
# to nnrpd.
try:
    set_auth_hook(myauth)
    syslog('notice', "authentication module successfully hooked into nnrpd")
except Exception, errmsg:
    syslog('error', "Cannot obtain nnrpd hook for authentication method: %s" % errmsg[0])
