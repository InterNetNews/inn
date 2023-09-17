##  This is a sample access module for the Python nnrpd hook.
##
##  See the INN Python Filtering and Authentication Hooks documentation
##  for more information.
##  The python_access: parameter in readers.conf is used to load this script.
##
##  An instance of ACCESS class is passed to nnrpd via the set_auth_hook()
##  function imported from nnrpd.  The following methods of that class
##  are known to nnrpd:
##
##  __init__()                  - Use this method to initialize your
##                                general variables or open a common
##                                database connection.  May be omitted.
##  access_init()               - Init function specific to access
##                                control.  May be omitted.
##  access(attributes)          - Called when a python_access
##                                statement is reached in the
##                                processing of readers.conf.  Returns
##                                a dictionary of values representing
##                                statements to be included in an
##                                access group.
##  access_close()              - Called on nnrpd termination.  Save
##                                your state variables or close a
##                                database connection.  May be omitted.
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


##  Sample access class.  It defines all access methods known to nnrpd.
class ACCESS:
    """Provide access callbacks to nnrpd."""

    def __init__(self):
        """This is a good place to initialize variables or open a
        database connection."""
        syslog("notice", "nnrpd access class instance created")

    def access_init(self):
        """Called when this script is initialized."""
        pass

    def access(self, attributes):
        """Called when python_access: is encountered in readers.conf."""

        # Just for debugging purposes (in Python 3.x syntax).
        #  syslog(
        #      "notice",
        #      "n_a access() invoked: hostname %s, ipaddress %s, port %lu,"
        #      " interface %s, intipaddr %s, intport %lu, user %s"
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

        # Allow newsreading from specific host only.
        # Python 2.x syntax:
        #  if "127.0.0.1" == str(attributes["ipaddress"]):
        # Python 3.x syntax:
        #  if b"127.0.0.1" == attributes["ipaddress"].tobytes():
        #      syslog(
        #          "notice",
        #          "authentication access by IP address succeeded"
        #      )
        #      return {"read": "*", "post": "*"}

        syslog("notice", "authentication access by IP address failed")
        return {"read": "!*", "post": "!*"}

    def access_close(self):
        """Called on nnrpd termination."""
        pass


##  The rest is used to hook up the access module on nnrpd.  It is unlikely
##  you will ever need to modify this.

##  Import functions exposed by nnrpd.  This import must succeed, or nothing
##  will work!
from nnrpd import *

##  Create a class instance.
myaccess = ACCESS()

##  ...and try to hook up on nnrpd.  This would make auth object methods
##  visible to nnrpd.
import sys

try:
    set_auth_hook(myaccess)
    syslog("notice", "access module successfully hooked into nnrpd")
except Exception:  # Syntax valid in both Python 2.x and 3.x.
    e = sys.exc_info()[1]
    syslog(
        "error", "Cannot obtain nnrpd hook for access method: %s" % e.args[0]
    )
