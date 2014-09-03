#! /bin/sh
#
# Test suite for xmalloc and friends.
#
# $Id$
#
# The canonical version of this file is maintained in the rra-c-util package,
# which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2000, 2001, 2006 Russ Allbery <rra@stanford.edu>
# Copyright 2008, 2009, 2010
#     The Board of Trustees of the Leland Stanford Junior University
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

. "$SOURCE/tap/libtap.sh"
cd "$BUILD/lib"

# Run an xmalloc test.  Takes the description, the expectd exit status, the
# output, and the arguments.
ok_xmalloc () {
    local desc w_status w_output output status
    desc="$1"
    shift
    w_status="$1"
    shift
    w_output="$1"
    shift
    output=`./xmalloc "$@" 2>&1`
    status=$?
    if [ "$w_status" -ne 0 ] ; then
        output=`echo "$output" | sed 's/:.*//'`
    fi
    if [ $status = $w_status ] && [ x"$output" = x"$w_output" ] ; then
        ok "$desc" true
    elif [ $status = 2 ] ; then
        skip "no data limit support"
    else
        echo "#  saw: ($status) $output"
        echo "#  not: ($w_status) $w_output"
        ok "$desc" false
    fi
}

# Skip this test suite unless maintainer-mode tests are enabled.  All of the
# failures in automated testing have been problems with the assumptions around
# memory allocation or problems with the test suite, not problems with the
# underlying xmalloc code.
#if [ -z "$RRA_MAINTAINER_TESTS" ] ; then
#    skip_all 'xmalloc tests only run for maintainer'
#fi

# Total tests.
plan 36

# First run the tests expected to succeed.
ok_xmalloc "malloc small"    0 "" "m" "21"      "0"
ok_xmalloc "malloc large"    0 "" "m" "3500000" "0"
ok_xmalloc "malloc zero"     0 "" "m" "0"       "0"
ok_xmalloc "realloc small"   0 "" "r" "21"      "0"
ok_xmalloc "realloc large"   0 "" "r" "3500000" "0"
ok_xmalloc "strdup small"    0 "" "s" "21"      "0"
ok_xmalloc "strdup large"    0 "" "s" "3500000" "0"
ok_xmalloc "strndup small"   0 "" "n" "21"      "0"
ok_xmalloc "strndup large"   0 "" "n" "3500000" "0"
ok_xmalloc "calloc small"    0 "" "c" "24"      "0"
ok_xmalloc "calloc large"    0 "" "c" "3500000" "0"
ok_xmalloc "asprintf small"  0 "" "a" "24"      "0"
ok_xmalloc "asprintf large"  0 "" "a" "3500000" "0"
ok_xmalloc "vasprintf small" 0 "" "v" "24"      "0"
ok_xmalloc "vasprintf large" 0 "" "v" "3500000" "0"

# Now limit our memory to 3.5MB and then try the large ones again, all of
# which should fail.
#
# The exact memory limits used here are essentially black magic.  They need to
# be large enough to allow the program to be loaded and do small allocations,
# but not so large that we can't reasonably expect to allocate that much
# memory normally.  3.5MB seems to work reasonably well on both Solaris and
# Linux.
#
# We assume that there are enough miscellaneous allocations that an allocation
# exactly as large as the limit will always fail.
ok_xmalloc "malloc fail" 1 \
    "failed to malloc 3500000 bytes at xmalloc.c line 38" \
    "m" "3500000" "3500000"
ok_xmalloc "realloc fail" 1 \
    "failed to realloc 3500000 bytes at xmalloc.c line 66" \
    "r" "3500000" "3500000"
ok_xmalloc "strdup fail" 1 \
    "failed to strdup 3500000 bytes at xmalloc.c line 97" \
    "s" "3500000" "3500000"
ok_xmalloc "strndup fail" 1 \
    "failed to strndup 3500000 bytes at xmalloc.c line 124" \
    "n" "3500000" "3500000"
ok_xmalloc "calloc fail" 1 \
    "failed to calloc 3500000 bytes at xmalloc.c line 148" \
    "c" "3500000" "3500000"
ok_xmalloc "asprintf fail" 1 \
    "failed to asprintf 3500000 bytes at xmalloc.c line 173" \
    "a" "3500000" "3500000"
ok_xmalloc "vasprintf fail" 1 \
    "failed to vasprintf 3500000 bytes at xmalloc.c line 195" \
    "v" "3500000" "3500000"

# Check our custom error handler.
ok_xmalloc "malloc custom"    1 "malloc 3500000 xmalloc.c 38" \
    "M" "3500000" "3500000"
ok_xmalloc "realloc custom"   1 "realloc 3500000 xmalloc.c 66" \
    "R" "3500000" "3500000"
ok_xmalloc "strdup custom"    1 "strdup 3500000 xmalloc.c 97" \
    "S" "3500000" "3500000"
ok_xmalloc "strndup custom"   1 "strndup 3500000 xmalloc.c 124" \
    "N" "3500000" "3500000"
ok_xmalloc "calloc custom"    1 "calloc 3500000 xmalloc.c 148" \
    "C" "3500000" "3500000"
ok_xmalloc "asprintf custom"  1 "asprintf 3500000 xmalloc.c 173" \
    "A" "3500000" "3500000"
ok_xmalloc "vasprintf custom" 1 "vasprintf 3500000 xmalloc.c 195" \
    "V" "3500000" "3500000"

# Check the smaller ones again just for grins.
ok_xmalloc "malloc retry"    0 "" "m" "21" "3500000"
ok_xmalloc "realloc retry"   0 "" "r" "32" "3500000"
ok_xmalloc "strdup retry"    0 "" "s" "64" "3500000"
ok_xmalloc "strndup retry"   0 "" "n" "20" "3500000"
ok_xmalloc "calloc retry"    0 "" "c" "24" "3500000"
ok_xmalloc "asprintf retry"  0 "" "a" "30" "3500000"
ok_xmalloc "vasprintf retry" 0 "" "v" "35" "3500000"
