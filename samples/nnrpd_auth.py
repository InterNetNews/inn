#
#
# This is a sample authentication and authorization module for python
# nnrpd hook
#
# For details, see the file doc/hook-python that came with INN.
#

#
# This file is loaded when one of the python_* readers.conf parameters
# is encountered. An instance of AUTH class is passed to nnrpd via
# set_auth_hook() function imported from nnrpd. The following methods
# of that class are known to nnrpd:
#
#   __init__()                  - Use this method to initilalize your
#                                 general variables or open a common
#                                 database connection. May be omitted.
#   access_init()               - Init function specific to access
#                                 control. May be omitted
#   access(attributes)          - Called when a python_access
#                                 statement is reached in the
#                                 processing of readers.conf. Returns
#                                 a dictionary of values representing
#                                 statements to be included in an
#                                 access group. 
#   access_close()              - Called on nnrpd termination. Save
#                                 your state variables or close a
#                                 database connection. May be omitted
#   authen_init()               - Init function specific to
#                                 authentication. May be omitted
#   authenticate(attributes)    - Called when a python_auth statement
#                                 is reached in the processing of
#                                 readers.conf. Returns a response
#                                 code, an error string and an
#                                 optional string to appear in the
#                                 logs as the username.
#   authen_close()              - Called on nnrpd termination. Save
#                                 your state variables or close a database
#                                 connection. May be omitted
#   dynamic_init()              - Init function specific to
#                                 authentication. May be omitted
#   dynamic(attributes)         - Called whenever a reader requests either
#                                 read or post access to a
#                                 newsgroup. Returns None to grant
#                                 access, or a non-empty string (which
#                                 will be reported back to reader)
#                                 otherwise. 
#   dynamic_close()             - Called on nnrpd termination. Save
#                                 your state variables or close a database
#                                 connection. May be omitted
#
# If there is a problem with return codes from any of these methods then nnrpd
# will die and syslog the exact reason.
#
# There are also a few Python functions defined in nnrpd:
#
# set_auth_hook()               - Called by nnrpd as this module is loaded.
#                                 It is used to pass a reference to an 
#                                 instance of authentication class to nnrpd.
# syslog()                      - An equivalent replacement for regular syslog.
#                                 One consideration for using it is to
#                                 uniform nnrpd logging.

#
# Sample authentication and authorization class. It defines all methods known
# to nnrpd.
#
class AUTH:
    """Provide authentication and authorization callbacks to nnrpd."""
    def __init__(self):
        """This is a good place to initialize variables or open a
           database connection. 
        """
        # Create a list of NNTP codes to respond on connect
        self.connectcodes = {   'READPOST':200,
                                'READ':201,
                                'AUTHNEEDED':480,
                                'PERMDENIED':502
        }

        # Create a list of NNTP codes to respond on authentication
        self.authcodes = {  'ALLOWED':281,
                            'DENIED':502
        }

        syslog('notice', 'nnrpd authentication class instance created')

    def authenticate(self, attributes):
        """Called when python_auth is encountered in readers.conf"""

	# just for debugging purposes
	syslog('notice', 'n_a authenticate() invoked: hostname %s, ipaddress %s, interface %s, user %s' % (\
		attributes['hostname'], \
		attributes['ipaddress'], \
		attributes['interface'], \
		attributes['user']))

	# do username passworld authentication
        if 'foo' == str(attributes['user'])  \
           and 'foo' == str(attributes['pass']):
            syslog('notice', 'authentication by username succeeded')
            return ( self.authcodes['ALLOWED'], 'No error', 'default_user')
        else:
            syslog('notice', 'authentication by username failed')
            return ( self.authcodes['DENIED'], 'Access Denied!')

    def access(self, attributes):
        """Called when python_access is encountered in readers.conf"""

	# just for debugging purposes
	syslog('notice', 'n_a access() invoked: hostname %s, ipaddress %s, interface %s, user %s' % (\
		attributes['hostname'], \
		attributes['ipaddress'], \
		attributes['interface'], \
		attributes['user']))

	# allow newsreading from specific host only
        if '127.0.0.1' == str(attributes['ipaddress']):
            syslog('notice', 'authentication by IP address succeeded')
            return {'read':'*','post':'*'}
        else:
            syslog('notice', 'authentication by IP address failed')
            return {'read':'!*','post':'!*'}

    def dynamic(self, attributes):
        """Called when python_dynamic was reached in the processing of
           readers.conf and a reader requests either read or post
           permission for particular newsgroup.
        """
	# just for debugging purposes
	syslog('notice', 'n_a dyanmic() invoked against type %s, hostname %s, ipaddress %s, interface %s, user %s' % (\
		attributes['type'], \
		attributes['hostname'], \
		attributes['ipaddress'], \
		attributes['interface'], \
		attributes['user']))

	# Allow reading of any newsgroup but not posting
        if 'post' == str(attributes['type']):
            syslog('notice', 'authorization for post access denied')
            return "no posting for you"
        elif 'read' == str(attributes['type']):
            syslog('notice', 'authorization for read access granted')
            return None
        else:
            syslog('notice', 'authorization type is not known: %s' % attributes['type'])
            return "Internal error";


#
# The rest is used to hook up the auth module on nnrpd. It is unlikely
# you will ever need to modify this.
#

# Import functions exposed by nnrpd. This import must succeed, or nothing
# will work!
from nnrpd import *

# Create a class instance
myauth = AUTH()

# ...and try to hook up on nnrpd. This would make auth object methods visible
# to nnrpd.
try:
    set_auth_hook(myauth)
    syslog('notice', "authentication module successfully hooked into nnrpd")
except Exception, errmsg:
    syslog('error', "Cannot obtain nnrpd hook for authentication method: %s" % errmsg[0])

