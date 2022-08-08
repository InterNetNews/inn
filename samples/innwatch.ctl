##  Sample control file for innwatch.
##
##  Indicates what to run to test the state of the news system, and what
##  to do about it.  Format:
##      !state!when!condition!test!limit!command!reason/comment
##  where
##      <!>             Delimiter; pick from [!,:@;?].
##      <state>         State to enter if true.
##      <when>          States we must be in to match.
##      <condition>     Command to run to test condition.
##      <test>          Operator to use in test(1) condition.
##      <limit>         Value to test against.
##      <command>       Command for innwatch to perform; use exit,
##                      flush, go, pause, shutdown, skip, or throttle.
##      <reason>        Used in ctlinnd command (if needed).
##
##  See the innwatch.ctl man page for more information.

##  First, just exit innwatch if innd has gone away.
!!! test -f ${LOCKS}/innd.pid && echo 0 || echo 1 ! eq ! 1 ! exit ! innd dead

##  If another innwatch has started, exit.
!!! test -f ${LOCKS}/LOCK.${PROGNAME} && cat ${LOCKS}/LOCK.${PROGNAME} || echo 0 ! ne ! $$ ! exit ! innwatch replaced

##  Next test the load average.  Above first threshold pause, above higher
##  threshold throttle, below restart limit undo whatever was done.
!load!load hiload! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! lt ! ${INNWATCHLOLOAD} ! go ! loadav
!hiload!+ load! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! gt ! ${INNWATCHHILOAD} ! throttle ! loadav
!load!+! uptime | tr -d ,. | awk '{ print $(NF - 2) }' ! gt ! ${INNWATCHPAUSELOAD} ! pause ! loadav

##  Uncomment these to keep overchan backlog in check.  Assumes your overchan
##  feed is named 'overview!'.
#::overblog: ctlinnd feedinfo overview! | awk 'NR==1{print $7}' : lt : 100000 : go : overviewbacklog
#:overblog:+: ctlinnd feedinfo overview! | awk 'NR==1{print $7}' : gt : 400000 : throttle : overviewbacklog

##  If load is OK, check space (and inodes) on various filesystems
!!! ${INNDF} . ! lt ! ${INNWATCHSPOOLSPACE} ! throttle ! No space (spool)
!!! ${INNDF} ${BATCH} ! lt ! ${INNWATCHBATCHSPACE} ! throttle ! No space (newsq)
!!! ${INNDF} ${PATHDB} ! lt ! ${INNWATCHLIBSPACE} ! throttle ! No space (newslib)
!!! ${INNDF} -i . ! lt ! ${INNWATCHSPOOLNODES} ! throttle ! No space (spool inodes)
!!! test -d ${OVERVIEWDIR} && ${INNDF} ${OVERVIEWDIR} ! lt ! ${INNWATCHSPOOLSPACE} ! throttle ! No space (overview)
