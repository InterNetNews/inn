#! /bin/sh
# $Id$
#
# Test suite for innupgrade.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Run innupgrade with the given arguments and expect it to succeed.
run () {
    perl -Tw ${innupgrade} "$@"
    if [ $? = 0 ] ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Make sure that a file does not exist.
notexists () {
    if [ -r "$1" ] ; then
        printcount "not ok"
    else
        printcount "ok"
    fi
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
innupgrade="../../scripts/innupgrade"
dirs='../data data tests/data'
for dir in $dirs ; do
    if [ -r "$dir/upgrade/inn.conf" ] ; then
        cd $dir
        break
    fi
done
if [ ! -x "$innupgrade" ] ; then
    echo "Could not find innupgrade" >&2
    exit 1
fi

# Print out the number of tests
echo 13

# Save backup copies.
cp "upgrade/inn.conf" "upgrade/inn.conf.bad"
cp "upgrade/newsfeeds" "upgrade/newsfeeds.bad"

# Upgrade everything in the directory.
run "upgrade"
compare "upgrade/inn.conf.OLD" "upgrade/inn.conf.bad"
compare "upgrade/newsfeeds.OLD" "upgrade/newsfeeds.bad"
compare "upgrade/inn.conf" "upgrade/inn.conf.ok"
compare "upgrade/newsfeeds" "upgrade/newsfeeds.ok"

if [ ! -e "upgrade/overview.fmt" ] && [ -e "upgrade/overview.fmt.OLD" ] ; then
    printcount "ok"
else
    printcount "not ok"
fi

# Move inn.conf back and run it again in file mode.
# Also move obsolete files back.
mv -f "upgrade/inn.conf.OLD" "upgrade/inn.conf"
mv -f "upgrade/overview.fmt.OLD" "upgrade/overview.fmt"
mv -f "upgrade/sasl.conf.OLD" "upgrade/sasl.conf"
run "-f" "upgrade/inn.conf"
compare "upgrade/inn.conf.OLD" "upgrade/inn.conf.bad"
compare "upgrade/inn.conf" "upgrade/inn.conf.ok"

if [ -e "upgrade/overview.fmt" ] && [ ! -e "upgrade/overview.fmt.OLD" ] ; then
    printcount "ok"
else
    printcount "not ok"
fi

# Run innupgrade on the good file and make sure that it doesn't change
# anything.
run "-f" "upgrade/inn.conf"
compare "upgrade/inn.conf.OLD" "upgrade/inn.conf.bad"
compare "upgrade/inn.conf" "upgrade/inn.conf.ok"

# Clean up.
mv -f "upgrade/inn.conf.bad" "upgrade/inn.conf"
mv -f "upgrade/newsfeeds.bad" "upgrade/newsfeeds"
rm -f "upgrade/inn.conf.OLD" "upgrade/newsfeeds.OLD"
