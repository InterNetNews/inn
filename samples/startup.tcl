##  $Id$
##
##  Tcl filter initialization code
##
##  If you compile with Tcl support enabled, this file (even if empty) must
##  exist as pathfilter/_PATH_TCL_STARTUP (as defined in paths.h).  This
##  sample file defines the two functions that are called before and after
##  reloading the filter code, but defines them as empty procs that do
##  nothing.

proc filter_before_reload {} {
}

proc filter_after_reload {} {
}
