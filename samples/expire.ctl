##  $Revision$
##  expire.ctl - expire control file
##  Format:
##	/remember/:<keep>
##	<class>:<keep>:<default>:<purge>
##	<wildmat>:<flag>:<keep>:<default>:<purge>
##  First line gives history retention; second line specifies expiration
##  for classes; third line specifies expiration for group if groupbaseexpiry
##  is true
##	<class>		class specified in storage.conf
##	<wildmat>	wildmat-style patterns for the newsgroups
##	<keep>		Mininum number of days to keep article
##	<default>	Default number of days to keep the article
##	<purge>		Flush article after this many days
##  <keep>, <default>, and <purge> can be floating-point numbers or the
##  word "never."  Times are based on when received unless -p is used;
##  see expire.8

##  If article expires before 14 days, we still remember it for 14 days in
##  case we get offered it again.  Depending on what you use for the innd
##  -c flag and how paranoid you are about old news, you might want to
##  make this 28, 30, etc.
/remember/:14

##  Keep for 1-10 days, allow Expires headers to work(groupbaseexpiry is false).
0:1:10:never
##  Keep for 1-10 days, allow Expires headers to work(groupbaseexpiry is true).
#*:A:1:10:never
