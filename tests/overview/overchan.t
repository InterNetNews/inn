#! /bin/sh
# $Id$
#
# Test suite for overchan.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Given two files, make sure that the first file exists and that its contents
# match the contents of the second file.
compare () {
    if [ -r "$1" ] && diff "$1" "$2" ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Find the right directory.
overchan="../../backends/overchan"
tdxutil="../../storage/tradindexed/tdx-util"
dirs='../data data tests/data'
for dir in $dirs ; do
    if [ -r "$dir/overview/1-4" ] ; then
        cd $dir
        break
    fi
done
if [ ! -x "$overchan" ] ; then
    echo "Could not find sm" >&2
    exit 1
fi
if [ ! -x "$tdxutil" ] ; then
    echo "Could not find tdx-util" >&2
    exit 1
fi

# Point programs at the appropriate inn.conf file and create the root of
# the overview directory.
INNCONF=etc/inn-tdx.conf; export INNCONF
INN_TESTSUITE=true; export INN_TESTSUITE
mkdir -p ov-tmp

# Print out the number of tests
echo 2

# We can use a common prefix; the way overchan works isn't going to corrupt
# tokens and arrival times differently for different articles.
prefix="@0502000005A4000000010000000000000000@ 1120456320"
sed "s/^/$prefix /" overview/1-4 > input

# Create the groups.
$tdxutil -c -n example.config
$tdxutil -c -n example.test

# Actually run overchan.
$overchan input
if [ $? = 0 ] ; then
    printcount "ok"
else
    printcount "not ok"
fi

# Now, dump the database again and make sure the right thing was stored.
$tdxutil -O -n example.config -a 1 >  output
$tdxutil -O -n example.test   -a 1 >> output
$tdxutil -O -n example.config -a 2 >> output
$tdxutil -O -n example.test   -a 3 >> output
compare input output

# All done.  Clean up.
rm -f input output
rm -rf ov-tmp
