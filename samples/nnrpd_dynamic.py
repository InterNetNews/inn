##  $Id$
##
##  This is a sample dynamic access module for the Python nnrpd hook.
##
##  See the INN Python Filtering and Authentication Hooks documentation
##  for more information.
##  The perl_dynamic: parameter in readers.conf is used to load this script.
##
##  An instance of DYNACCESS class is passed to nnrpd via the set_auth_hook()
##  function imported from nnrpd.  The following methods of that class
##  are known to nnrpd:
##
##  __init__()                  - Use this method to initialize your
##                                general variables or open a common
##                                database connection.  May be omitted.
##  dynamic_init()              - Init function specific to
##                                authentication.  May be omitted.
##  dynamic(attributes)         - Called whenever a reader requests either
##                                read or post access to a
##                                newsgroup.  Returns None to grant
##                                access, or a non-empty string (which
##                                will be reported back to reader)
##                                otherwise.
##  dynamic_close()             - Called on nnrpd termination.  Save
##                                your state variables or close a database
##                                connection.  May be omitted.
##
##  If there is a problem with return codes from any of these methods, then nnrpd
##  will die and syslog the exact reason.
##
##  There are also a few Python functions defined in nnrpd:
##
##  set_auth_hook()             - Called by nnrpd as this module is loaded.
##                              It is used to pass a reference to an
##                              instance of authentication class to nnrpd.
##  syslog()                    - An equivalent replacement for regular syslog.
##                              One consideration for using it is to
##                              uniform nnrpd logging.

##  Sample dynamic access class.  It defines all dynamic access methods known
##  to nnrpd.
class DYNACCESS:
    """Provide dynamic access callbacks to nnrpd."""

    def __init__(self):
        """This is a good place to initialize variables or open a
           database connection."""
        syslog('notice', 'nnrpd dynamic access class instance created')

    def dynamic_init(self):
        """Called when this script is initialized."""
        pass

    def dynamic(self, attributes):
        """Called when python_dynamic: is reached in the processing of
           readers.conf and a reader requests either read or post
           permission for particular newsgroup."""

	# Just for debugging purposes.
	syslog('notice', 'n_a dynamic() invoked against type %s, hostname %s, ipaddress %s, interface %s, user %s' % (\
		attributes['type'], \
		attributes['hostname'], \
		attributes['ipaddress'], \
		attributes['interface'], \
		attributes['user']))

	# Allow reading of any newsgroup but not posting.
        if 'post' == str(attributes['type']):
            syslog('notice', 'dynamic authorization access for post access denied')
            return "no posting for you"
        elif 'read' == str(attributes['type']):
            syslog('notice', 'dynamic authorization access for read access granted')
            return None
        else:
            syslog('notice', 'dynamic authorization access type is not known: %s' % attributes['type'])
            return "Internal error";

    def dynamic_close(self):
        """Called on nnrpd termination."""
        pass


##  The rest is used to hook up the dynamic access module on nnrpd.  It is unlikely
##  you will ever need to modify this.

##  Import functions exposed by nnrpd.  This import must succeed, or nothing
##  will work!
from nnrpd import *

##  Create a class instance.
mydynaccess = DYNACCESS()

##  ...and try to hook up on nnrpd.  This would make auth object methods visible
##  to nnrpd.
try:
    set_auth_hook(mydynaccess)
    syslog('notice', "dynamic access module successfully hooked into nnrpd")
except Exception, errmsg:
    syslog('error', "Cannot obtain nnrpd hook for dynamic access method: %s" % errmsg[0])
