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
EOF
exit
fi
search="@<$1>@"
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
    if fgrep $search $1 >/dev/null; then 
	echo $1
    fi
    shift
done
