# -*- tcl -*-
#
# $Revision$
#
# A TCL procedure that will be run over every article. See README.tcl_hook
# for more details.

proc filter_news {} {
#  global o Headers
#  set sum [checksum_article]
#  puts $o "$Headers(Message-ID) $sum"
#  set newsgroups [split $Headers(Newsgroups) ,]
#  foreach i $newsgroups {
#    if {$i=="alt.test" && [string match "*heiney@pa.dec.com*" $Headers(From)]} {
#      return "dont like alt.test from heiney"
#    }
#  }
  return "accept"
}
