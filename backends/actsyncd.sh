#!/bin/sh
# @(#) $Id$
# @(#) Under RCS control in /usr/local/news/src/inn/local/RCS/actsyncd.sh,v
#
# actsyncd - actsync daemon
#
# usage:
#	actsyncd [-x] config_file [debug_level [debug_outfmt]]
#
#	-x		xexec instead of reload
#	config_file	name of file used to determine how to run actsync
#	debug_level	force no action and use -v debug_level
#	debug_outfmt	change -o a1 output to -o debug_outfmt for debug

# By: Landon Curt Noll  	chongo@toad.com		(chongo was here /\../\)
#
# Copyright (c) Landon Curt Noll, 1993.
# All rights reserved.
#
# Permission to use and modify is hereby granted so long as this 
# notice remains.  Use at your own risk.  No warranty is implied.

# preset vars
#
# =()<. @<_PATH_SHELLVARS>@>()=
. @CONFPATH@/innshellvars
# Our lock file
LOCK=${LOCKS}/LOCK.actsyncd
# where actsync is located
ACTSYNC=${PATHBIN}/actsync
# timeout for ctlinnd
TIMEOUT=120
# exit value of actsync if unable to get an active file
NOSYNC=127
# where ctlinnd lives
CTLINND="${PATHBIN}/ctlinnd"
# by default we xexec instead of reloading
XEXEC=
export XEXEC

# parse args
#
if [ $# -gt 1 ]; then
    if [ X"-x" = X"$1" ]; then
	XEXEC=true
	shift
    elif [ X"-r" = X"$1" ]; then
	# backward compatibility
	XEXEC=
	shift
    fi
fi
case $# in
    1) cfg="$1"; DEBUG=; DEBUG_FMT=; ;;
    2) cfg="$1"; DEBUG="$2"; DEBUG_FMT=; ;;
    3) cfg="$1"; DEBUG="$2"; DEBUG_FMT="$3"; ;;
    *) echo "usage: $0 [-x] config_file [debug_level [debug_outfmt]]" 1>&2;
       exit 1 ;;
esac
if [ ! -s "$cfg" ]; then
    echo "$0: config_file not found or empty: $ign" 1>&2
    exit 2
fi

# parse config_file
#
host="`sed -n -e 's/#.*//' -e 's/^host=[ 	]*//p' $cfg | tail -1`"
if [ -z "$host" ]; then
    echo "$0: no host specified in $cfg" 1>&2
    exit 3
fi
flags="`sed -n -e 's/#.*//' -e 's/^flags=[ 	]*//p' $cfg | tail -1`"
if [ -z "$flags" ]; then
    echo "$0: no flags specified in $cfg" 1>&2
    exit 4
fi
ign="`sed -n -e 's/#.*//' -e 's/^ignore_file=[ 	]*//p' $cfg | tail -1`"
if [ -z "$ign" ]; then
    echo "$0: no ignore file specified in $cfg" 1>&2
    exit 5
fi
spool="`sed -n -e 's/#.*//' -e 's/^spool=[ 	]*//p' $cfg | tail -1`"
if [ -z "$spool" ]; then
    echo "$0: no spool directory specified in $cfg" 1>&2
    exit 6
fi
if [ ! -s "$ign" ]; then
    echo "$0: ignore_file not found or empty: $ign" 1>&2
    exit 7
fi

# force -S spool_dir mode (overrides any -S arg in the command line)
#
flags="$flags -S $spool"

# force -o a1 mode (overrides any -o argument in the command line)
#
if [ -z "$DEBUG" ]; then

    # standard actsyncd output mode
    flags="$flags -o a1"

# DEBUG processing, ig debug_level was given
#
else

    # force -v level as needed
    flags="$flags -v $DEBUG"

    # force -o level but reject -o x modes
    if [ ! -z "$DEBUG_FMT" ]; then
	case "$DEBUG_FMT" in
	x*) echo "$0: do not use any of the -o x debug_outfmt modes!" 1>&2;
	    exit 8 ;;
	*) flags="$flags -o $DEBUG_FMT" ;;
	esac
    fi

    # execute actsync directly
    echo "DEBUG: will execute $ACTSYNC -i $ign $flags $host" 1>&2
    eval "$ACTSYNC -i $ign $flags $host"
    status="$?"
    echo "DEBUG: exit status $status" 1>&2
    exit "$status"
fi

# Lock out others
#
trap 'rm -f ${LOCK}; exit 1' 0 1 2 3 15
shlock -p $$ -f ${LOCK} || {
    echo "$0: Locked by `cat ${LOCK}`" 1>&2
    exit 9
}

# setup
#
activenew="${PATHETC}/active.new.$$"
out="${TMPDIR}/.sync.$$"
trap "rm -f $activenew $out ${LOCK}; exit" 0 1 2 3 15
rm -f "$out"
touch "$out"
chmod 0644 "$out"

# try to sync 
# 
# Try to sync off of the host.  If unable to connect/sync then retry
# up to 9 more times waiting 6 minutes between each try.
#
echo "=-= `date` for $host" >>$out 2>&1
for loop in 1 2 3 4 5 6 7 8 9 10; do

    # pause the server
    #
    echo "=-= `date` for $host pause innd" >>$out 2>&1
    ${CTLINND} pause "actsyncd" >>$out
    if [ $? -ne 0 ]; then
	echo "FATAL: `date` for $host cannot pause innd" >>$out
	sed -e 's/^/    /' < "$out"
	exit 10
    fi

    # form the new active file
    #
    echo "rm -f $activenew" >>$out
    rm -f "$activenew" >>$out 2>&1
    echo "touch $activenew" >>$out
    touch "$activenew" >>$out 2>&1
    echo "chmod 0644 $activenew" >>$out
    chmod 0644 "$activenew" >>$out 2>&1
    echo "$ACTSYNC -i $ign $flags $host" >>$out
    eval "$ACTSYNC -i $ign $flags $host >$activenew 2>>$out"
    status=$?
    if [ "$status" -ne "$NOSYNC" ]; then

	# detect bad status
	#
	if [ "$status" -ne 0 ]; then
	    echo "FATAL: `date` for $host exit $status" >>$out
	    ${CTLINND} go "actsyncd" >>$out
	    sed -e 's/^/    /' < "$out"
	    exit "$status"
	fi

	# detect empty output
	#
	if [ ! -s "$activenew" ]; then
	    echo "FATAL: `date` for $host empty or missing $activenew" >>$out
	    ${CTLINND} go "actsyncd" >>$out
	    sed -e 's/^/    /' < "$out"
	    exit 11
	fi

	# form active.times lines for new newsgroups
	#
	# determine seconds since the Epoch (1 Jan 1970 0:00:00 UTC)
	EP_FMT='X%S + \( X%M \* 60 \) + \( X%H \* 3600 \) + \( X%j \* 86400 \) + \( \( X%y - 70 \) \* 31536000 \) + \( \( \( X%y - 73 \) / 4 \) \* 86400 \)'
	NOW=`eval expr \`TZ=UTC date "+$EP_FMT" | sed -e 's/X00* /0 /' -e 's/X0*/0/g'\``
	if [ -z "${NOW}" ]; then
	    echo "FATAL: `date` for $host secs output is empty" >>$out
	    ${CTLINND} go "actsyncd" >>$out
	    sed -e 's/^/    /' < "$out"
	    exit 12
	fi
	echo "=-= `date` for $host, forming active.times" >>$out
	eval "$ACTSYNC -i $ign $flags -o c -v 0 -q 12 ${ACTIVE} $activenew 2>>$out" | \
	    grep '^ctlinnd newgroup ' | \
	    awk '{print $3, NOW, "actsyncd";}' NOW=$NOW - >> ${ACTIVETIMES}

	# move the active file into place
	#
	wc "${ACTIVE}.old" "${ACTIVE}" "$activenew" | sed -e 's/^  *//' >>$out
	echo "rm -f ${ACTIVE}.old" >>$out
	rm -f "${ACTIVE}.old" >>$out 2>&1
	echo "ln ${ACTIVE} ${ACTIVE}.old" >>$out
	ln "${ACTIVE}" "${ACTIVE}.old" >>$out 2>&1
	echo "mv -f $activenew ${ACTIVE}" >>$out
	mv -f "$activenew" "${ACTIVE}" >>$out 2>&1

	# reload the new active or reexec the server if needed
	#
	if [ -z "$XEXEC" ]; then
	    # reload the modified active file
	    #
	    echo "=-= `date` for $host, reload active" >>$out
	    ${CTLINND} reload active actsyncd >>$out 2>&1
	    if [ $? -ne 0 ]; then
		echo "FATAL: `date` for $host cannot reload active" >>$out
		sed -e 's/^/    /' < "$out"
		exit 13
	    fi

	    # active file has been updated successfully, resume server
	    #
	    echo "=-= `date` for $host, go innd" >>$out
	    ${CTLINND} go "actsyncd" >>$out
	    if [ $? -ne 0 ]; then
		echo "FATAL: `date` for $host cannot go innd" >>$out
		sed -e 's/^/    /' < "$out"
		exit 14
	    fi
	else
	    # just re-exec the server and let the new server read new active
	    #
	    echo "=-= `date` for $host, xexec" >>$out
	    ${CTLINND} xexec '' >>$out 2>&1
	    if [ $? -ne 0 ]; then
		echo "FATAL: `date` for $host cannot xexec, server dead" >>$out
		sed -e 's/^/    /' < "$out"
		exit 15
	    fi
	    # see if the server has reloaded
	    #
	    sleep 30
	    ${CTLINND} mode >/dev/null 2>&1
	    if [ $? -ne 0 ]; then
		echo "FATAL: `date` for $host did not resume after xexec" >>$out
		sed -e 's/^/    /' < "$out"
		exit 16
	    fi
	fi

	# normal exit - all done
	#
	echo "=-= `date` for $host, end" >>$out
	sed -e 's/^/    /' < "$out"
	exit "$status"
    fi

    # failed to form the active file, resume server
    #
    echo "=-= `date` for $host failed to connect/sync, retrying" >>$out
    ${CTLINND} go "actsyncd" >>$out
    if [ $? -ne 0 ]; then
	echo "FATAL: `date` for $host cannot go innd" >>$out
	sed -e 's/^/    /' < "$out"
	exit 17
    fi

    # wait 6 minutes
    #
    sleep 360
done

# give up
#
echo "FATAL: `date` for $host failed to connect/sync 10 times" >>$out 2>&1
sed -e 's/^/    /' < "$out"
exit "$status"
