##  This is a sample authentication module for the Python nnrpd hook.
##
##  See the INN Python Filtering and Authentication Hooks documentation
##  for more information.
##  The python_auth: parameter in readers.conf is used to load this script.
##
##  An instance of AUTH class is passed to nnrpd via the set_auth_hook()
##  function imported from nnrpd.  The following methods of that class
##  are known to nnrpd:
##
##  __init__()                  - Use this method to initialize your
##                                general variables or open a common
##                                database connection.  May be omitted.
##  authen_init()               - Init function specific to
##                                authentication.  May be omitted.
##  authenticate(attributes)    - Called when a python_auth statement
##                                is reached in the processing of
##                                readers.conf.  Returns a response
##                                code, an error string and an
##                                optional string to appear in the
##                                logs as the username (make sure that
##                                such a message is properly encoded
##                                in UTF-8 so as to comply with the
##                                NNTP protocol).
##  authen_close()              - Called on nnrpd termination.  Save
##                                your state variables or close a database
##                                connection.  May be omitted.
##
##  If there is a problem with return codes from any of these methods,
##  then nnrpd will die and syslog the exact reason.
##
##  There are also a few Python functions defined in nnrpd:
##
##  set_auth_hook()             - Called by nnrpd as this module is loaded.
##                              It is used to pass a reference to an
##                              instance of authentication class to nnrpd.
##  syslog()                    - An equivalent replacement for regular syslog.
##                              One consideration for using it is to
##                              uniform nnrpd logging.


##  Sample authentication class.  It defines all auth methods known to nnrpd.
class AUTH:
    """Provide authentication callbacks to nnrpd."""

    def __init__(self):
        """This is a good place to initialize variables or open a
        database connection."""

        # Create a list of NNTP codes to respond on authentication.
        self.authcodes = {
            "ALLOWED": 281,
            "DENIED": 481,
            "ERROR": 403,
        }

        syslog("notice", "nnrpd authentication class instance created")

    def authen_init(self):
        """Called when this script is initialized."""
        pass

    def authenticate(self, attributes):
        """Called when python_auth: is encountered in readers.conf."""

        # Just for debugging purposes (in Python 3.x syntax).
        # By default, do not log passwords (available in attributes["pass"]).
        #  syslog(
        #      "notice",
        #      "n_a authenticate() invoked: hostname %s, ipaddress %s,"
        #      " port %lu, interface %s, intipaddr %s, intport %lu, user %s"
        #      % (
        #          attributes["hostname"]
        #          .tobytes()
        #          .decode(errors="backslashreplace"),
        #          attributes["ipaddress"].tobytes().decode(),
        #          attributes["port"],
        #          attributes["interface"].tobytes().decode(),
        #          attributes["intipaddr"].tobytes().decode(),
        #          attributes["intport"],
        #          (
        #              attributes["user"]
        #              .tobytes()
        #              .decode(errors="backslashreplace")
        #              if attributes["user"]
        #              else "-"
        #          ),
        #      ),
        #  )

        # Do username password authentication.
        # Python 2.x syntax:
        #  if (
        #      attributes["user"]
        #      and attributes["pass"]
        #      and "foo" == str(attributes["user"])
        #      and "foo" == str(attributes["pass"])
        #  ):
        # Python 3.x syntax:
        #  if (
        #      attributes["user"]
        #      and attributes["pass"]
        #      and b"foo" == attributes["user"].tobytes()
        #      and b"foo" == attributes["pass"].tobytes()
        #  ):
        #      syslog("notice", "authentication by username succeeded")
        #      return (self.authcodes["ALLOWED"], "No error", "default_user")

        syslog("notice", "authentication by username failed")
        return (self.authcodes["DENIED"], "Access Denied!")

    def authen_close(self):
        """Called on nnrpd termination."""
        pass


##  The rest is used to hook up the auth module on nnrpd.  It is unlikely
##  you will ever need to modify this.

##  Import functions exposed by nnrpd.  This import must succeed, or nothing
##  will work!
from nnrpd import *

##  Create a class instance.
myauth = AUTH()

##  ...and try to hook up on nnrpd.  This would make auth object methods
##  visible to nnrpd.
import sys

try:
    set_auth_hook(myauth)
    syslog("notice", "authentication module successfully hooked into nnrpd")
except Exception:  # Syntax valid in both Python 2.x and 3.x.
    e = sys.exc_info()[1]
    syslog(
        "error",
        "Cannot obtain nnrpd hook for authentication method: %s" % e.args[0],
    )
