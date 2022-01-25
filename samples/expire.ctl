##  Sample configuration file for article expiration.
##
##  Format:
##      /remember/:<keep>
##      <class>:<min>:<default>:<max>
##      <wildmat>:<flag>:<min>:<default>:<max>
##
##  First line gives history retention; second line specifies expiration
##  for classes; third line specifies expiration for group if groupbaseexpiry
##  is true in inn.conf.
##      <class>         Class specified in storage.conf.
##      <wildmat>       Wildmat-style patterns for the newsgroups.
##      <flag>          Status of the newsgroups.
##      <keep>          Number of days to retain a message-ID in history.
##      <min>           Minimum number of days to keep the article.
##      <default>       Default number of days to keep the article.
##      <max>           Flush the article after this many days.
##  <keep>, <min>, <default> and <max> can be floating-point numbers or the
##  word "never".  Times are based on the arrival time for expire and expireover
##  (unless -p is used; see expire(8) and expireover(8)), and on the posting
##  time for history retention.
##
##  See the expire.ctl man page for more information.

##  When an article is rejected or expires before 10 days, we still remember
##  it for 11 days from its original posting time in case we get offered it
##  again.  See the artcutoff parameter in inn.conf; it should match this
##  parameter (/remember/ uses 11 days instead of 10 in order to take into
##  account articles whose posting date is one day into the future).
/remember/:11

##  Keep for 1-90 days, allow Expires header fields to work.  This entry uses
##  the syntax appropriate when groupbaseexpiry is true in inn.conf.  Times
##  are based on the arrival time (unless -p is used).
*:A:1:90:never

##  Keep for 1-90 days, allow Expires header fields to work.  This is an entry
##  based on storage class, used when groupbaseexpiry is false.  Times
##  are based on the arrival time (unless -p is used).
#0:1:90:never
