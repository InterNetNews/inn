##  $Revision$
##  innwatch.ctl -- control file for innwatch.
##  Indicates what to run to test the state of the news system, and what
##  to do about it.  Format:
##	!state!when!command!test!limit!command!reason/comment
##  where
##	<!>		Delimiter; pick from [,:@;?!]
##	<state>		State to enter if true.
##	<when>		States we must be in to match.
##	<command>	Command to run to test condition.
##	<test>		Operator to use in test(1) command.
##	<limit>		Value to test against.
##	<command>	Command for innwatch to perform; use exit,
##			flush, go, pause, shutdown, skip, or throttle.
##	<reason>	Used in ctlinnd command (if needed).

##  First, just exit innwatch if innd has gone away.
##  =()<!!! test -f @<_PATH_SERVERPID>@ && echo 0 || echo 1 ! eq ! 1 ! exit ! innd dead>()=
!!! test -f /var/news/run/innd.pid && echo 0 || echo 1 ! eq ! 1 ! exit ! innd dead

##  Next test the load average.  Above first threshold pause, above higher
##  threshold throttle, below restart limit undo whatever was done.
##  =()<!load!load hiload! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! lt ! @<INNWATCH_LOLOAD>@ ! go ! loadav>()=
!load!load hiload! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! lt ! 1000 ! go ! loadav
##  =()<!hiload!+ load! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! gt ! @<INNWATCH_HILOAD>@ ! throttle ! loadav>()=
!hiload!+ load! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! gt ! 2000 ! throttle ! loadav
##  =()<!load!+! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! gt ! @<INNWATCH_PAUSELOAD>@ ! pause ! loadav>()=
!load!+! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! gt ! 1500 ! pause ! loadav

##  If load is OK, check space (and inodes) on various filesystems
##  =()<!!! @<INNWATCH_DF>@ . | awk 'NR == 2 { print $@<INNWATCH_BLOCKS>@ }' ! lt ! @<INNWATCH_SPOOLSPACE>@ ! throttle ! No space (spool)>()=
!!! /bin/df . | awk 'NR == 2 { print $4 }' ! lt ! 8000 ! throttle ! No space (spool)
##  =()<!!! @<INNWATCH_DF>@ @<_PATH_BATCHDIR>@ | awk 'NR == 2 { print $@<INNWATCH_BLOCKS>@ }' ! lt ! @<INNWATCH_BATCHSPACE>@ ! throttle ! No space (newsq)>()=
!!! /bin/df /var/news/spool/out.going | awk 'NR == 2 { print $4 }' ! lt ! 800 ! throttle ! No space (newsq)
##  =()<!!! @<INNWATCH_DF>@ @<_PATH_NEWSLIB>@ | awk 'NR == 2 { print $@<INNWATCH_BLOCKS>@ }' ! lt ! @<INNWATCH_LIBSPACE>@ ! throttle ! No space (newslib)>()=
!!! /bin/df /var/news/etc | awk 'NR == 2 { print $4 }' ! lt ! 25000 ! throttle ! No space (newslib)
##  =()<!!! @<INNWATCH_DF>@ -i . | awk 'NR == 2 { print $@<INNWATCH_INODES>@ }' ! lt ! @<INNWATCH_SPOOLNODES>@ ! throttle ! No space (spool inodes)>()=
!!! /bin/df -i . | awk 'NR == 2 { print $7 }' ! lt ! 200 ! throttle ! No space (spool inodes)
##  =()<!!! @<INNWATCH_DF>@ @<_PATH_OVERVIEWDIR>@ | awk 'NR == 2 { print $@<INNWATCH_BLOCKS>@ }' ! lt ! @<INNWATCH_SPOOLSPACE>@ ! throttle ! No space (overview)>()=
!!! /bin/df /var/news/spool/over.view | awk 'NR == 2 { print $4 }' ! lt ! 8000 ! throttle ! No space (overview)
