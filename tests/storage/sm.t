#! /bin/sh
# $Id$
#
# Test suite for sm.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Store an article and make sure that sm succeeds.
store () {
    token=`$sm -s < $1`
    if [ $? = 0 ] ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Make sure that a file exists.
exists () {
    if [ -r "$1" ] ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check that an article retrieved via sm is the same as the article stored in
# the article spool.  Takes the token and the path to the actual article.
retrieve () {
    "$sm" "$1" > spool/test
    if [ $? = 0 ] && diff "$2" spool/test ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check the article information returned against the information in the actual
# article.
info () {
    real=`grep ^Xref $2 | sed -e 's/.*\.example\.com //' -e 's/ .*//'`
    real=`echo "$real" | sed -e 's/:/: /'`
    info=`$sm -i $1`
    if [ $? = 0 ] && [ "$info" = "$real" ] ; then
        printcount "ok"
    else
        echo "Want: $real"
        echo "Have: $info"
        printcount "not ok"
    fi
}

# Given a token and a path in the spool, make sure that article removal works.
remove () {
    "$sm" -r "$1"
    if [ $? = 0 ] && [ ! -r "$2" ] ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check retrieval of a raw article, given a token and a path in the spool.
raw () {
    "$sm" -R "$1" > spool/test
    if [ $? = 0 ] && diff "$2" spool/test ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check retrieval of only the headers, given a token and a path to the real
# article.
headers () {
    sed '/^$/q' "$2" > spool/real
    "$sm" -H "$1" > spool/test
    if [ $? = 0 ] && diff spool/real spool/test ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check the rnews batch format.  Takes the token and the original article
# path.  Use Perl to get the size of the article, since I can't find a really
# good way to do this in shell.
rnews () {
    size=`perl -e "print -s \"$2\""`
    ( echo "#! rnews $size" ; cat "$2" ) > spool/real
    "$sm" -S "$1" > spool/test
    if [ $? = 0 ] && diff spool/real spool/test ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check retrieval of multiple articles at once.  We do this both via passing
# multiple arguments to sm and passing multiple arguments on stdin.  Takes two
# tokens and two article paths.
multiple () {
    cat "$3" "$4" > spool/real
    "$sm" "$1" "$2" > spool/test
    if [ $? = 0 ] && diff spool/real spool/test ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
    ( echo "$1" ; echo "$2" ) | "$sm" > spool/test
    if [ $? = 0 ] && diff spool/real spool/test ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Check the quieting of error messages.
quiet () {
    output=`$sm -q @BADTOKEN@ 2>&1`
    if [ $? = 1 ] && [ -z "$output" ] ; then
        printcount "ok"
    else
        printcount "not ok"
    fi
}

# Find the right directory.
sm="../../frontends/sm"
dirs='../data data tests/data'
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

# Print out the count of tests.
echo 23

# Point sm at the appropriate inn.conf file and create our required directory
# structure.
INNCONF=etc/inn.conf; export INNCONF
mkdir -p spool

# First, store the articles.
store articles/1
token1="$token"
store articles/2
token2="$token"
store articles/3
token3="$token"

# Make sure they were written properly.
exists spool/example/test/1
exists spool/example/test/2
exists spool/example/config/1
exists spool/example/config/2

# Make sure that the articles are correct.
retrieve "$token1" articles/1
retrieve "$token2" articles/2
retrieve "$token3" articles/3

# Check the article information.
info "$token1" articles/1
info "$token2" articles/2
info "$token3" articles/3

# Delete an article and make sure that this works.
remove "$token1" spool/example/test/1

# Check raw article retrieval.
raw "$token2" spool/example/config/1
raw "$token3" spool/example/config/2

# Check retrieval of only the headers.
headers "$token2" articles/2
headers "$token3" articles/3

# Check rnews format messages.
rnews "$token2" articles/2
rnews "$token3" articles/3

# Check retrieval of multiple articles.
multiple "$token2" "$token3" articles/2 articles/3

# Check silencing of error messages.
quiet

# All done.  Clean up.
rm -rf spool
