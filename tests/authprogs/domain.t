#! /bin/sh
# $Id$
#
# Test suite for domain.

# The count starts at 1 and is updated each time ok is printed.  printcount
# takes "ok" or "not ok".
count=1
printcount () {
    echo "$1 $count $2"
    count=`expr $count + 1`
}

# Run domain, expecting it to succeed.  Feed it the client host the way that
# nnrpd would.  Takes the client host, the domain to check it against, and the
# user expected.
runsuccess () {
    output=`( echo ClientHost: $1 ; echo ClientIP: 127.0.0.1 ; \
              echo ClientPort: 0 ; echo LocalIP: 127.0.0.1 ; \
              echo LocalPort: 119) | $domain $2 2>&1`
    status=$?
    if test $status = 0 && test x"$output" = x"$3" ; then
        printcount "ok"
    else
        printcount "not ok"
        echo "  saw: $output"
        echo "  not: $3"
    fi
}

# Run domain, expecting it to fail, and make sure it fails with status 1 and
# prints out the right error message.  Takes the client host, the domain to
# check it against, and the expected output string.
runfailure () {
    output=`( echo ClientHost: $1 ; echo ClientIP: 127.0.0.1 ; \
              echo ClientPort: 0 ; echo LocalIP: 127.0.0.1 ; \
              echo LocalPort: 119) | $domain $2 2>&1`
    status=$?
    if test $status = 1 && test x"$output" = x"$3" ; then
        printcount "ok"
    else
        printcount "not ok"
        echo "  saw: $output"
        echo "  not: $3"
    fi
}

# Make sure we're in the right directory.
domain=domain
for dir in authprogs ../authprogs ../../authprogs ; do
    test -x "$dir/domain" && domain="$dir/domain"
done

# Print the test count.
echo 8

# First, run the tests that we expect to succeed.
runsuccess "foo.example.com"     ".example.com" "User:foo"
runsuccess "foo.example.com"     "example.com"  "User:foo"
runsuccess "foo.bar.example.com" ".example.com" "User:foo.bar"
runsuccess "foo.bar.example.com" "example.com"  "User:foo.bar"
runsuccess "foo.example.com"     "com"          "User:foo.example"

# Now, run the tests that we expect to fail.
runfailure "example.com"     "example.com" \
    "domain: host example.com matches the domain exactly"
runfailure "foo.example.com" "example.net" \
    "domain: host foo.example.com didn't match domain example.net"
runfailure "fooexample.com"  "example.com" \
    "domain: host fooexample.com didn't match domain example.com"
