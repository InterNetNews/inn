#! /bin/sh
# $Id$
#
# Wrapper around the setenv test suite to set a resource limit low enough
# that two strings over 100KB can't both be allocated, allowing the memory
# allocation failure code in setenv to be exercised.  Done with this
# wrapper because ulimit is more easily portable than the corresponding C
# code.

# Find where the test suite is.
setenv=setenv.tr
for file in ./setenv.tr lib/setenv.tr tests/lib/setenv.tr ; do
    [ -x $file ] && setenv=$file
done

ulimit -d 150
exec $setenv
