# -*- tcl -*-
#
# $Revision$
#
# If you compile with tcl support enabled then this file (even if empty),
# must exist in the location defined in the config.data

## =()<set o [open "@<_PATH_MOST_LOGS>@/tcl.log" "a+"]>()=
set o [open "/news/log/tcl.log" "a+"]

proc filter_before_reload {} {
    global o
    puts $o "Reloading tcl filter script (before)."
}

proc filter_after_reload {} {
    global o 
    puts $o "Reloading tcl filter script (after)."
}
