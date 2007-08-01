#! /bin/sh
# $Id$
#
# Test suite for inndf.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Find the right directory.
inndf="../../backends/inndf"
makehistory="../../expire/makehistory"
dirs='../data data tests/data'
for dir in $dirs ; do
    if [ -r "$dir/articles/1" ] ; then
        cd $dir
        break
    fi
done
if [ ! -x "$inndf" ] ; then
    echo "Could not find inndf" >&2
    exit 1
fi

# Print out the count of tests.
echo 6

# Make sure df -k works, or we have to just skip this test.
if df -k . > /dev/null 2>&1 ; then
    real=`df -k . | sed 1d | awk '{ print $4 }'`
    try=`$inndf .`
    diff=`expr "$real" - "$try"`
    if [ "$diff" -gt 200 ] || [ "$diff" -lt -200 ] ; then
        printcount "not ok"
    else
        printcount "ok"
    fi
else
    printcount "ok" "# skip"
fi

# Make sure df -i works, or we have to just skip this test.  Also accept a
# return value of 2^32 - 1 from inndf regardless of what df says, since this
# is what Reiser and some other file systems return in some versions of
# Linux.
if df -i . > /dev/null 2>&1 ; then
    real=`df -i . | sed 1d | awk '{ print $4 }'`
    try=`$inndf -i .`
    if [ "$try" = 4294967295 ] ; then
        printcount "ok"
    else
        diff=`expr "$real" - "$try"`
        if [ "$diff" -gt 10 ] || [ "$diff" -lt -10 ] ; then
            printcount "not ok"
        else
            printcount "ok"
        fi
    fi
else
    printcount "ok" "# skip"
fi

# Create a message spool so that we can use makehistory to generate an
# overview database.
mkdir -p spool/example/config
mkdir -p spool/example/test
cp articles/1 spool/example/test/1
cp articles/2 spool/example/config/1
cp articles/3 spool/example/test/2
cp articles/4 spool/example/test/3
rm -f spool/example/config/2
ln -s ../test/2 spool/example/config/2

# First, generate a tradindexed overview to test inndf -n and make sure that
# inndf -o returns the appropriate thing.
INN_TESTSUITE=1; export INN_TESTSUITE
INNCONF="etc/inn-tdx.conf"; export INNCONF
mkdir -p ov-tmp tmp run
if ! $makehistory -x -O > /dev/null 2>&1 ; then
    echo "makehistory failed, unable to continue" >&2
    exit 1
fi
out=`$inndf -n`
if [ "$out" = "5 overview records stored" ] ; then
    printcount "ok"
else
    printcount "not ok"
fi
out=`$inndf -o`
if [ "$out" = "Space used is meaningless for the tradindexed method" ] ; then
    printcount "ok"
else
    printcount "not ok"
fi

# Delete that overview and then create a buffindexed overview, testing both
# inndf -n and inndf -o.
rm -rf ov-tmp
mkdir ov-tmp
dd if=/dev/zero of=ov-tmp/buffer bs=1024k count=1 > /dev/null 2>&1
INNCONF="etc/inn-bfx.conf"; export INNCONF
if ! $makehistory -x -O > /dev/null 2>&1 ; then
    echo "makehistory failed, unable to continue" >&2
    exit 1
fi
out=`$inndf -n`
if [ "$out" = "5 overview records stored" ] ; then
    printcount "ok"
else
    echo "$out"
    printcount "not ok"
fi
out=`$inndf -o`
if [ "$out" = "3.00% overview space used" ] ; then
    printcount "ok"
else
    echo "$out"
    printcount "not ok"
fi

# Clean up.
rm -rf spool ov-tmp tmp run db/group.index
