#! /bin/sh
# $Id$
#
# Test suite for makehistory.

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
sm="../../frontends/sm"
makehistory="../../expire/makehistory"
dirs='. storage tests/storage'
for dir in $dirs ; do
    if [ -r "$dir/articles/1" ] ; then
        cd $dir
        break
    fi
done
if [ ! -x "$sm" ] ; then
    echo "Could not find sm" >&2
    exit 1
fi
if [ ! -x "$makehistory" ] ; then
    echo "Could not find makehistory" >&2
    exit 1
fi

# Point sm at the appropriate inn.conf file and create our required directory
# structure.
INNCONF=etc/inn.conf; export INNCONF
mkdir -p spool

# Print out the number of tests
echo 3

# First, store the articles.
$sm -s < articles/1  > spool/tokens
$sm -s < articles/2 >> spool/tokens
$sm -s < articles/3 >> spool/tokens
$sm -s < articles/4 >> spool/tokens
sort < spool/tokens > spool/tokens.new
mv spool/tokens.new spool/tokens

# Run makehistory on the spool to generate only the overview information and
# make sure the correct number of lines were generated.
$makehistory -x -O -S > spool/overview
lines=`wc -l spool/overview | sed -e 's/^ *//' -e 's/ .*//'`
if [ "$lines" = 4 ] ; then
    printcount "ok"
else
    printcount "not ok"
fi

# Check the tokens to be sure they're correct.
sed 's/ .*//' spool/overview | sort > spool/tokens-test
compare spool/tokens-test spool/tokens

# Compare the overview data and make sure that it's correct.  We trim off the
# arrival date since we can't really check it, but we leave the expires time.
sed 's/^[^ ]* [^ ]* //' < spool/overview > spool/stripped
compare spool/stripped overview/1-4

# All done.  Clean up.
rm -rf spool
