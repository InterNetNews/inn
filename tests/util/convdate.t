#! /bin/sh
# $Id$
#
# Test suite for convdate.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Given the output from convdate and the expected output, compare them.
compare () {
    status=$?
    if [ $status = 0 ] && [ "$1" = "$2" ] ; then
        printcount "ok"
    else
        echo "  $1"
        echo "  $2"
        printcount "not ok"
    fi
}

# Find convdate.
convdate=false
for file in ../expire/convdate ../../expire/convdate expire/convdate ; do
    [ -x $file ] && convdate=$file
done
if [ $convdate = "false" ] ; then
    echo "Could not find convdate" >&2
    exit 1
fi

# Print out the count of tests.
echo 7

# Run our tests.  These are all from the man page, but with time zones
# added.
TZ=EST5EDT; export TZ
compare "`$convdate '10 Feb 1991 10:00:00 -0500'`" 'Sun Feb 10 10:00:00 1991'
compare "`$convdate '13 Dec 91 12:00 EST' '04 May 1990 0:0:0'`" \
    'Fri Dec 13 12:00:00 1991
Fri May  4 00:00:00 1990'
compare "`$convdate -n '10 FEB 1991 10:00-0500' '5 may 90 00:00-0400'`" \
    '666198000
641880000'
compare "`$convdate -c 666198000`" 'Sun Feb 10 10:00:00 1991'
compare "`$convdate -dc 666198000`" 'Sun, 10 Feb 1991 15:00:00 +0000 (UTC)'
compare "`env TZ=PST8PDT $convdate -dlc 666198000`" \
    'Sun, 10 Feb 1991 07:00:00 -0800 (PST)'
compare "`env TZ=EST5EDT $convdate -dlc 666198000`" \
    'Sun, 10 Feb 1991 10:00:00 -0500 (EST)'
