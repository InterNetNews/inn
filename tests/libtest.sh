# $Id$
#
# Shell function library for test cases.
#
# This file provides a TAP-compatible shell function library useful for
# writing test cases.  It is part of C TAP Harness, which can be found at
# <http://www.eyrie.org/~eagle/software/c-tap-harness/>.
#
# Written by Russ Allbery <rra@stanford.edu>
# Copyright 2009, 2010 Russ Allbery <rra@stanford.edu>
# Copyright 2006, 2007, 2008 Board of Trustees, Leland Stanford Jr. University
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

# Print out the number of test cases we expect to run.
plan () {
    count=1
    planned="$1"
    failed=0
    echo "1..$1"
    trap finish 0
}

# Prepare for lazy planning.
plan_lazy () {
    count=1
    planned=0
    failed=0
    trap finish 0
}

# Report the test status on exit.
finish () {
    local highest looks
    highest=`expr "$count" - 1`
    if [ "$planned" = 0 ] ; then
        echo "1..$highest"
        planned="$highest"
    fi
    looks='# Looks like you'
    if [ "$planned" -gt 0 ] ; then
        if [ "$planned" -gt "$highest" ] ; then
            if [ "$planned" -gt 1 ] ; then
                echo "$looks planned $planned tests but only ran $highest"
            else
                echo "$looks planned $planned test but only ran $highest"
            fi
        elif [ "$planned" -lt "$highest" ] ; then
            local extra
            extra=`expr "$highest" - "$planned"`
            if [ "$planned" -gt 1 ] ; then
                echo "$looks planned $planned tests but ran $extra extra"
            else
                echo "$looks planned $planned test but ran $extra extra"
            fi
        elif [ "$failed" -gt 0 ] ; then
            if [ "$failed" -gt 1 ] ; then
                echo "$looks failed $failed tests of $planned"
            else
                echo "$looks failed $failed test of $planned"
            fi
        elif [ "$planned" -gt 1 ] ; then
            echo "# All $planned tests successful or skipped"
        else
            echo "# $planned test successful or skipped"
        fi
    fi
}

# Skip the entire test suite.  Should be run instead of plan.
skip_all () {
    local desc
    desc="$1"
    if [ -n "$desc" ] ; then
        echo "1..0 # skip $desc"
    else
        echo "1..0 # skip"
    fi
    exit 0
}

# ok takes a test description and a command to run and prints success if that
# command is successful, false otherwise.  The count starts at 1 and is
# updated each time ok is printed.
ok () {
    local desc
    desc="$1"
    if [ -n "$desc" ] ; then
        desc=" - $desc"
    fi
    shift
    if "$@" ; then
        echo ok $count$desc
    else
        echo not ok $count$desc
        failed=`expr $failed + 1`
    fi
    count=`expr $count + 1`
}

# Skip the next test.  Takes the reason why the test is skipped.
skip () {
    echo "ok $count # skip $*"
    count=`expr $count + 1`
}

# Report the same status on a whole set of tests.  Takes the count of tests,
# the description, and then the command to run to determine the status.
ok_block () {
    local end i desc
    i=$count
    end=`expr $count + $1`
    shift
    desc="$1"
    shift
    while [ "$i" -lt "$end" ] ; do
        ok "$desc" "$@"
        i=`expr $i + 1`
    done
}

# Skip a whole set of tests.  Takes the count and then the reason for skipping
# the test.
skip_block () {
    local i end
    i=$count
    end=`expr $count + $1`
    shift
    while [ "$i" -lt "$end" ] ; do
        skip "$@"
        i=`expr $i + 1`
    done
}

# Portable variant of printf '%s\n' "$*".  In the majority of cases, this
# function is slower than printf, because the latter is often implemented
# as a builtin command.  The value of the variable IFS is ignored.
puts () {
    cat << EOH
$@
EOH
}

# Run a program expected to succeed, and print ok if it does and produces the
# correct output.  Takes the description, expected exit status, the expected
# output, the command to run, and then any arguments for that command.
# Standard output and standard error are combined when analyzing the output of
# the command.
#
# If the command may contain system-specific error messages in its output,
# add strip_colon_error before the command to post-process its output.
ok_program () {
    local desc w_status w_output output status
    desc="$1"
    shift
    w_status="$1"
    shift
    w_output="$1"
    shift
    output=`"$@" 2>&1`
    status=$?
    if [ $status = $w_status ] && [ x"$output" = x"$w_output" ] ; then
        ok "$desc" true
    else
        echo "#  saw: ($status) $output"
        echo "#  not: ($w_status) $w_output"
        ok "$desc" false
    fi
}

# Strip a colon and everything after it off the output of a command, as long
# as that colon comes after at least one whitespace character.  (This is done
# to avoid stripping the name of the program from the start of an error
# message.)  This is used to remove system-specific error messages (coming
# from strerror, for example).
strip_colon_error() {
    local output status
    output=`"$@" 2>&1`
    status=$?
    output=`puts "$output" | sed 's/^\([^ ]* [^:]*\):.*/\1/'`
    puts "$output"
    return $status
}

# Bail out with an error message.
bail () {
    echo 'Bail out!' "$@"
    exit 1
}

# Output a diagnostic on standard error, preceded by the required # mark.
diag () {
    echo '#' "$@"
}

# Search for the given file first in $BUILD and then in $SOURCE and echo the
# path where the file was found, or the empty string if the file wasn't
# found.
test_file_path () {
    if [ -n "$BUILD" ] && [ -f "$BUILD/$1" ] ; then
        puts "$BUILD/$1"
    elif [ -n "$SOURCE" ] && [ -f "$SOURCE/$1" ] ; then
        puts "$SOURCE/$1"
    else
        echo ''
    fi
}
