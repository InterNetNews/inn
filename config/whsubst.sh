#!/bin/sh
# Search the subst victims files for a parameter reference.
# Example usage:
#    ./whsubst.sh _PATH_ACTIVE
# Forrest J. Cavalier III, Mib Software, 1998/02/19
#
if test $1
then
else
cat <<EOF
Use this program to locate subst victim files in files.list which 
reference a particular parameter.

Example usage:
    ./whsubst.sh _PATH_ACTIVE

Example output:
../doc/active.5
../doc/makeactive.8
../doc/makehistory.8
../include/paths.h
../samples/inncheck
../samples/innshellvars
../samples/innshellvars.csh
../samples/innshellvars.pl
../samples/innshellvars.tcl
../samples/scanspool

To list every file in files.list which contains any substitution,
use
    ./whsubst.sh any

To list every file in files.list which contains no substitutions,
use
    ./whsubst.sh none
EOF
exit
fi
search="@<$1>@"
if test $1 = any 
then
    search='=()<'
fi
if test $1 = none
then
    search=$1
fi
#echo $search
shift
FILE=files.list
LIST=`grep -v '^;' <$FILE`
set x $LIST
#echo $LIST
shift
while test $1
do
#	echo trying $1
    file=$1
    if test -f $file ; then
    else
#    try a .in version.
	file=$file.in
    fi
    if test $search = none ; then
	if fgrep '=()<' $file >/dev/null; then
	else
	    echo $file
	fi
    else
        if (fgrep '=()<' $file | fgrep $search ) >/dev/null; then 
	    echo $file
	fi
    fi
    shift
done
