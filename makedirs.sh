#! /bin/sh
##  $Revision$
##  Script to make INN directories.

##  Some systems don't support -x, so we have to use -f.
CHOWN=chown
if [ ${CHOWN} = chown ] ; then
    if [ -f /sbin/chown ] ; then
	CHOWN=/sbin/chown
    else
	if [ -f /etc/chown ] ; then
	    CHOWN=/etc/chown
	else
	    if [ -f /usr/etc/chown ] ; then
		CHOWN=/usr/etc/chown
	    fi
	fi
    fi
fi

##  =()<MAN1=@<MAN1>@>()=
MAN1=/usr/news/man/man1
##  =()<MAN3=@<MAN3>@>()=
MAN3=/usr/news/man/man3
##  =()<MAN5=@<MAN5>@>()=
MAN5=/usr/news/man/man5
##  =()<MAN8=@<MAN8>@>()=
MAN8=/usr/news/man/man8
MAN=`echo $MAN8 | sed -e 's@\(.*\)/[^/]*@\1@'`

##  =()<SPOOL=@<_PATH_SPOOL>@>()=
SPOOL=/var/news/spool/articles
## =()<OVERVIEWDIR=@<_PATH_OVERVIEWDIR>@>()=
OVERVIEWDIR=/var/news/spool/over.view
##  =()<ARCHIVEDIR=@<_PATH_ARCHIVEDIR>@>()=
ARCHIVEDIR=/var/news/spool/archive
##  =()<BATCHDIR=@<_PATH_BATCHDIR>@>()=
BATCHDIR=/var/news/spool/out.going
##  =()<MOST_LOGS=@<_PATH_MOST_LOGS>@>()=
MOST_LOGS=/var/log/news
## =()<PATH_AUTHDIR=@<_PATH_AUTHDIR>@>()=
PATH_AUTHDIR=/usr/news/bin/auth
##  =()<SPOOLNEWS=@<_PATH_SPOOLNEWS>@>()=
SPOOLNEWS=/var/news/spool/in.coming
##  =()<BADNEWS=@<_PATH_BADNEWS>@>()=
BADNEWS=/var/news/spool/in.coming/bad
##  =()<SPOOLTEMP=@<_PATH_SPOOLTEMP>@>()=
SPOOLTEMP=/var/tmp

##  =()<NEWSLIB=@<_PATH_NEWSLIB>@>()=
NEWSLIB=/var/news/etc
##  =()<NEWSBIN=@<_PATH_NEWSBIN>@>()=
NEWSBIN=/usr/news/bin
##  =()<CONTROLPROGS=@<_PATH_CONTROLPROGS>@>()=
CONTROLPROGS=/usr/news/bin/control
##  =()<RNEWSPROGS=@<_PATH_RNEWSPROGS>@>()=
RNEWSPROGS=/usr/news/bin/rnews.libexec
##  =()<INNDDIR=@<_PATH_INNDDIR>@>()=
INNDDIR=/var/news/run
##  =()<LOCKSDIR=@<_PATH_LOCKS>@>()=
LOCKSDIR=/var/news/locks
## =()<MOST_LOGS=@<_PATH_MOST_LOGS>@>()=
MOST_LOGS=/var/log/news
## =()<LOCKS=@<_PATH_LOCKS>@>()=
LOCKS=/var/news/locks

##  =()<NEWSETC=@<_PATH_NEWSBOOT>@>()=
NEWSETC=/usr/news/bin/rc.news
NEWSETC="`dirname ${NEWSETC}`"
##  =()<NEWSHIST=@<_PATH_HISTORY>@>()=
NEWSHIST=/var/news/etc/history
NEWSHIST="`dirname ${NEWSHIST}`"
##  =()<NEWSTMP=@<_PATH_TMP>@>()=
NEWSTMP=/var/tmp
##  =()<NEWSHOME=@<_PATH_NEWSHOME>@>()=
NEWSHOME=/usr/news
NEWSLBIN=${NEWSHOME}/local

##  =()<NEWSUSER=@<NEWSUSER>@>()=
NEWSUSER=news
##  =()<NEWSGROUP=@<NEWSGROUP>@>()=
NEWSGROUP=news

PFLAG="$1"
umask 0
set -x

for F in \
${MAN} ${MAN1} ${MAN3} ${MAN5} ${MAN8} \
${NEWSHOME} ${NEWSETC} ${NEWSHIST} ${NEWSTMP} ${NEWSLBIN} \
${SPOOL} ${OVERVIEWDIR} ${ARCHIVEDIR} ${BATCHDIR} ${LOCKS} \
${MOST_LOGS} ${MOST_LOGS}/OLD \
${SPOOLNEWS} ${BADNEWS} ${SPOOLTEMP} \
${NEWSLIB} ${INNDDIR} ${LOCKSDIR} \
${NEWSBIN} ${CONTROLPROGS} ${RNEWSPROGS} \
${PATH_AUTHDIR} ; do

    if [ ! -d ${F} ] ; then
	mkdir ${PFLAG} ${DESTDIR}${F} || exit 1
	${CHOWN} ${NEWSUSER} ${DESTDIR}${F} || exit 1
	chgrp ${NEWSGROUP} ${DESTDIR}${F} || exit 1
	case ${F} in
	${INNDDIR})
	    chmod 0770 ${DESTDIR}${F} || exit 1
	    ;;
	*)
	    chmod 0775 ${DESTDIR}${F} || exit 1
	    ;;
	esac
    fi

done

exit 0
