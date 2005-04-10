#! /bin/sh
# $Id$
#
# Test suite for getlist.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Takes a file, a grep expression and then arguments for getlist, calls
# getlist with those arguments, and compares the result from the results of
# running grep on the file.
getlist () {
    file="$1"
    shift
    grep="$1"
    shift
    grep "$grep" "$file" > wanted
    $getlist -h localhost -p 11119 "$@" > seen
    if [ $? = 0 ] && diff wanted seen ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
    rm wanted seen
}

# Find the right directory.
getlist="../../frontends/getlist"
dirs='. clients tests/clients'
for dir in $dirs ; do
    if [ -r "$dir/server-list" ] ; then
        cd $dir
        break
    fi
done
if [ ! -x "$getlist" ] ; then
    echo 'Could not find getlist' >&2
    exit 1
fi
if [ ! -x 'server-list' ] ; then
    echo 'Could not find server-list' >&2
    exit 1
fi

# Print out the count of tests.
echo 15

# Point getlist at the appropriate inn.conf file.
INNCONF=../data/etc/inn.conf; export INNCONF

# Start the daemon.
./server-list

# Run the tests.
getlist ../data/db/active .
getlist ../data/db/active . active
getlist ../data/db/active . active '*'
getlist ../data/db/active . active '*' ymx=
getlist ../data/db/active '\.test ' active '*.test'
getlist ../data/db/active '\.test ' active '*.test' y
getlist ../data/db/active '#######' active '*.test' m
getlist ../data/db/active ' =' active '*' =
getlist ../data/db/active ' x$' active 'example.*' x
getlist ../data/db/active ' y$' active '*' y
getlist ../data/db/newsgroups . newsgroups
getlist ../data/db/newsgroups . newsgroups '*'
getlist ../data/db/newsgroups 'config' newsgroups '*.config'
getlist ../data/db/active.times . active.times
getlist ../data/db/distributions . distributions

# Kill the server.
kill `cat pid`
rm pid
