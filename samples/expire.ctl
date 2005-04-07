##  $Revision$
##  expire.ctl - expire control file
##  Format:
##	/remember/:<keep>
##	<class>:<min>:<default>:<max>
##	<wildmat>:<flag>:<min>:<default>:<max>
##  First line gives history retention; second line specifies expiration
##  for classes; third line specifies expiration for group if groupbaseexpiry
##  is true
##	<class>		class specified in storage.conf
##	<wildmat>	wildmat-style patterns for the newsgroups
##	<min>		Mininum number of days to keep article
##	<default>	Default number of days to keep the article
##	<max>		Flush article after this many days
##  <min>, <default>, and <max> can be floating-point numbers or the
##  word "never."  Times are based on when received unless -p is used;
##  see expire.8

##  If article expires before 10 days, we still remember it for 10 days in
##  case we get offered it again.  Depending on what you use for the innd
##  -c flag and how paranoid you are about old news, you might want to
##  make this 28, 30, etc, but it's probably safe to reduce it to 7 in most
##  cases if you want to keep your history file smaller.
/remember/:10

##  Keep for 1-10 days, allow Expires headers to work.  This entry uses
##  the syntax appropriate when groupbaseexpiry is true in inn.conf.
*:A:1:10:never

##  Keep for 1-10 days, allow Expires headers to work.  This is an entry
##  based on storage class, used when groupbaseexpiry is false.
#0:1:10:never
