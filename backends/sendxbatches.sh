#!/bin/sh
# $Id$ 
# By petri@ibr.cs.tu-bs.de with mods by libove@jerry.alf.dec.com
#
#  Script to send xbatches for a site, wrapped around innxbatch
#  Invocation: sendxbatches.sh <sitename> <hostname> <xbatch file name> ...
#
##  TODO: - we should check the amount of queued batches for the site,
##          to prevent disk overflow due to unreachable sites.

if [ $# -lt 3 ]
then
	echo "usage: $0 <sitename> <hostname> <xbatch file name>"
	exit 1
fi

##  =()<. @<_PATH_SHELLVARS>@>()=
. @CONFPATH@/innshellvars

LOCK=${LOCKS}/LOCK.sendxbatches
shlock -p $$ -f ${LOCK}
if [ $? -ne 0 ]
then
	echo Locked by `cat ${LOCK}`
	exit 1
fi

trap 'rm -f ${LOCK} ; exit 1' 1 2 3 15
site="$1"
host="$2"
shift; shift

ctlinnd -s flush "$site"
if [ $? -ne 0 ]
then
	echo "ctlinnd flush $site failed."
	exit 1
fi
sleep 5
$NEWSBIN/innxbatch -D -v "$host" $*
