#! /bin/sh
# $Id$
#
# Test suite for archive.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Run archive and make sure that it exits successfully.  Purges the archive in
# advance to make sure that we have fresh data.
archive () {
    rm -rf archive
    mkdir -p archive
    "$archive" -r "$@" < spool/tokens
    if [ $? = 0 ] ; then
        printcount "ok"
    else
        printcount "not ok"
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
sm="../../frontends/sm"
archive="../../backends/archive"
dirs='../data data data/storage'
for dir in $dirs ; do
    if [ -r "$dir/articles/1" ] ; then
        cd $dir
        break
    fi
done
if [ ! -x "$archive" ] ; then
    echo "Could not find archive" >&2
    exit 1
fi

# Print out the count of tests.
echo 17

# Point programs at the appropriate inn.conf file and create our required
# directory structure.
INNCONF=etc/inn.conf; export INNCONF
mkdir -p spool

# Store the articles so that archive can read them using the strorage API.
"$sm" -s < articles/1         >  spool/tokens
"$sm" -s < articles/2         >> spool/tokens
echo '# This is a comment'    >> spool/tokens
"$sm" -s < articles/3         >> spool/tokens
"$sm" -s < articles/bad-subj  >> spool/tokens
echo ''                       >> spool/tokens
echo '# Another comment'      >> spool/tokens
"$sm" -s < articles/bad-msgid >> spool/tokens

# Archive those messages with the default options.
archive

# Make sure they were written properly.
compare archive/example/test/1   articles/1
compare archive/example/config/1 articles/2
compare archive/example/test/2   articles/3
compare archive/example/config/2 articles/3
compare archive/example/test/3   articles/bad-subj
compare archive/example/test/4   articles/bad-msgid

# Archive the same messages using a flat directory structure and writing an
# index.
archive -f -i archive/INDEX

# Make sure they were written properly.
compare archive/example.test/1   articles/1
compare archive/example.config/1 articles/2
compare archive/example.test/2   articles/3
compare archive/example.config/2 articles/3
compare archive/example.test/3   articles/bad-subj
compare archive/example.test/4   articles/bad-msgid

# Check the index.
cat > archive/INDEX.right <<EOF
example.test/1 Test article <example-1@example.com>
example.config/1 Test article #2 <example-2@example.com>
example.test/2 Test article #3 with a | | tab <example-3@example.com>
example.config/2 Test article #3 with a | | tab <example-3@example.com>
example.test/3 <none> <example-bad-subj@example.com>
example.test/4 Test article with stray CR | | and no message ID <none>
EOF
compare archive/INDEX archive/INDEX.right

# Archive the same messages using per-date collections of messages.  Only
# archive messages to example.config.
archive -c -p '*.config'

# Make sure they were written properly.
( echo '-----------' ; cat articles/2 ;
  echo '-----------' ; cat articles/3 ) > archive/check
date=`date +%Y%m`
compare "archive/example.config/$date" archive/check

# All done.  Clean up.
rm -rf archive spool
