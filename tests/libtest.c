/* $Id$
 *
 * Some utility routines for writing tests.
 *
 * Here are a variety of utility routines for writing tests compatible with
 * the TAP protocol.  All routines of the form ok() or is*() take a test
 * number and some number of appropriate arguments, check to be sure the
 * results match the expected output using the arguments, and print out
 * something appropriate for that test number.  Other utility routines help in
 * constructing more complex tests, skipping tests, or setting up the TAP
 * output format.
 *
 * This file is part of C TAP Harness.  The current version plus supporting
 * documentation is at <http://www.eyrie.org/~eagle/software/c-tap-harness/>.
 *
 * Copyright 2009, 2010 Russ Allbery <rra@stanford.edu>
 * Copyright 2006, 2007, 2008
 *     Board of Trustees, Leland Stanford Jr. University
 * Copyright (c) 2004, 2005, 2006
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * This code is derived from software contributed to the Internet Software
 * Consortium by Rich Salz.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include "clibrary.h"

#include "inn/messages.h"
#include "inn/libinn.h"
#include <errno.h>
#include <math.h>
#include "libtest.h"

/*
 * The test count.  Always contains the number that will be used for the next
 * test status.
 */
unsigned long testnum = 1;

/*
 * Status information stored so that we can give a test summary at the end of
 * the test case.  We store the planned final test and the count of failures.
 * We can get the highest test count from testnum.
 *
 * We also store the PID of the process that called plan() and only summarize
 * results when that process exits, so as to not misreport results in forked
 * processes.
 *
 * If _lazy is true, we're doing lazy planning and will print out the plan
 * based on the last test number at the end of testing.
 */
static unsigned long _planned = 0;
static unsigned long _failed  = 0;
static pid_t _process = 0;
static int _lazy = 0;


/*
 * Our exit handler.  Called on completion of the test to report a summary of
 * results provided we're still in the original process.
 */
static void
finish(void)
{
    unsigned long highest = testnum - 1;

    if (_planned == 0 && !_lazy)
        return;
    fflush(stderr);
    if (_process != 0 && getpid() == _process) {
        if (_lazy) {
            printf("1..%lu\n", highest);
            _planned = highest;
        }
        if (_planned > highest)
            printf("# Looks like you planned %lu test%s but only ran %lu\n",
                   _planned, (_planned > 1 ? "s" : ""), highest);
        else if (_planned < highest)
            printf("# Looks like you planned %lu test%s but ran %lu extra\n",
                   _planned, (_planned > 1 ? "s" : ""), highest - _planned);
        else if (_failed > 0)
            printf("# Looks like you failed %lu test%s of %lu\n", _failed,
                   (_failed > 1 ? "s" : ""), _planned);
        else if (_planned > 1)
            printf("# All %lu tests successful or skipped\n", _planned);
        else
            printf("# %lu test successful or skipped\n", _planned);
    }
}


/*
 * Initialize things.  Turns on line buffering on stdout and then prints out
 * the number of tests in the test suite.
 */
#ifndef LIBTEST_NEW_FORMAT
void
test_init(int count)
{
    plan(count);
}
#endif
void
plan(unsigned long count)
{
    if (setvbuf(stdout, NULL, _IOLBF, BUFSIZ) != 0)
        fprintf(stderr, "# cannot set stdout to line buffered: %s\n",
                strerror(errno));
    fflush(stderr);
    printf("1..%lu\n", count);
    testnum = 1;
    _planned = count;
    _process = getpid();
    atexit(finish);
}


/*
 * Initialize things for lazy planning, where we'll automatically print out a
 * plan at the end of the program.  Turns on line buffering on stdout as well.
 */
void
plan_lazy(void)
{
    if (setvbuf(stdout, NULL, _IOLBF, BUFSIZ) != 0)
        fprintf(stderr, "# cannot set stdout to line buffered: %s\n",
                strerror(errno));
    testnum = 1;
    _process = getpid();
    _lazy = 1;
    atexit(finish);
}


/*
 * Skip the entire test suite and exits.  Should be called instead of plan(),
 * not after it, since it prints out a special plan line.
 */
void
skip_all(const char *format, ...)
{
    fflush(stderr);
    printf("1..0 # skip");
    if (format != NULL) {
        va_list args;

        putchar(' ');
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    }
    putchar('\n');
    exit(0);
}


/*
 * Print the test description.
 */
static void
print_desc(const char *format, va_list args)
{
    printf(" - ");
    vprintf(format, args);
}


/*
 * Takes a boolean success value and assumes the test passes if that value
 * is true and fails if that value is false.
 */
#if defined LIBTEST_NEW_FORMAT
void
ok(int success, const char *format, ...)
#else
void
ok(int n UNUSED, int success) {
    new_ok(success, NULL);
}

void
new_ok(int success, const char *format, ...)
#endif
{
    fflush(stderr);
    printf("%sok %lu", success ? "" : "not ", testnum++);
    if (!success)
        _failed++;
    if (format != NULL) {
        va_list args;

        va_start(args, format);
        print_desc(format, args);
        va_end(args);
    }
    putchar('\n');
}


/*
 * Same as ok(), but takes the format arguments as a va_list.
 */
void
okv(int success, const char *format, va_list args)
{
    fflush(stderr);
    printf("%sok %lu", success ? "" : "not ", testnum++);
    if (!success)
        _failed++;
    if (format != NULL)
        print_desc(format, args);
    putchar('\n');
}


/*
 * Skip a test.
 */
#if defined LIBTEST_NEW_FORMAT
void
skip(const char *reason, ...)
#else
void
skip(int n UNUSED, const char *reason) {
    new_skip(reason);
}

void
new_skip(const char *reason, ...)
#endif
{
    fflush(stderr);
    printf("ok %lu # skip", testnum++);
    if (reason != NULL) {
        va_list args;

        va_start(args, reason);
        putchar(' ');
        vprintf(reason, args);
        va_end(args);
    }
    putchar('\n');
}


/*
 * Report the same status on the next count tests.
 */
#if defined LIBTEST_NEW_FORMAT
void
ok_block(unsigned long count, int status, const char *format, ...)
#else
void
ok_block(int n UNUSED, int count, int success) {
    new_ok_block(count, success, NULL);
}

void
new_ok_block(unsigned long count, int status, const char *format, ...)
#endif
{
    unsigned long i;

    fflush(stderr);
    for (i = 0; i < count; i++) {
        printf("%sok %lu", status ? "" : "not ", testnum++);
        if (!status)
            _failed++;
        if (format != NULL) {
            va_list args;

            va_start(args, format);
            print_desc(format, args);
            va_end(args);
        }
        putchar('\n');
    }
}


/*
 * Skip the next count tests.
 */
#if defined LIBTEST_NEW_FORMAT
void
skip_block(unsigned long count, const char *reason, ...)
#else
void
skip_block(int n UNUSED, int count, const char *reason) {
    new_skip_block(count, reason);
}
  
void
new_skip_block(unsigned long count, const char *reason, ...)
#endif
{
    unsigned long i;

    fflush(stderr);
    for (i = 0; i < count; i++) {
        printf("ok %lu # skip", testnum++);
        if (reason != NULL) {
            va_list args;

            va_start(args, reason);
            putchar(' ');
            vprintf(reason, args);
            va_end(args);
        }
        putchar('\n');
    }
}


/*
 * Takes an expected integer and a seen integer and assumes the test passes
 * if those two numbers match.
 */
#ifndef LIBTEST_NEW_FORMAT
void
ok_int(int n UNUSED, int wanted, int seen)
{
    is_int(wanted, seen, NULL);
}
#endif
void
is_int(long wanted, long seen, const char *format, ...)
{
    fflush(stderr);
    if (wanted == seen)
        printf("ok %lu", testnum++);
    else {
        printf("# wanted: %ld\n#   seen: %ld\n", wanted, seen);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    if (format != NULL) {
        va_list args;

        va_start(args, format);
        print_desc(format, args);
        va_end(args);
    }
    putchar('\n');
}


/*
 * Takes a string and what the string should be, and assumes the test passes
 * if those strings match (using strcmp).
 */
#ifndef LIBTEST_NEW_FORMAT
void
ok_string(int n UNUSED, const char *wanted, const char *seen)
{
    is_string(wanted, seen, NULL);
}   
#endif
void
is_string(const char *wanted, const char *seen, const char *format, ...)
{
    if (wanted == NULL)
        wanted = "(null)";
    if (seen == NULL)
        seen = "(null)";
    fflush(stderr);
    if (strcmp(wanted, seen) == 0)
        printf("ok %lu", testnum++);
    else {
        printf("# wanted: %s\n#   seen: %s\n", wanted, seen);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    if (format != NULL) {
        va_list args;

        va_start(args, format);
        print_desc(format, args);
        va_end(args);
    }
    putchar('\n');
}


/*
 * Takes an expected double and a seen double and assumes the test passes if
 * those two numbers are within delta of each other.
 */
#ifndef LIBTEST_NEW_FORMAT
void
ok_double(int n UNUSED, double wanted, double seen)
{
    is_double(wanted, seen, 0.01, NULL);
}   
#endif
void
is_double(double wanted, double seen, double epsilon, const char *format, ...)
{
    fflush(stderr);
    if ((isnan(wanted) && isnan(seen))
        || (isinf(wanted) && isinf(seen) && wanted == seen)
        || fabs(wanted - seen) <= epsilon)
        printf("ok %lu", testnum++);
    else {
        printf("# wanted: %g\n#   seen: %g\n", wanted, seen);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    if (format != NULL) {
        va_list args;

        va_start(args, format);
        print_desc(format, args);
        va_end(args);
    }
    putchar('\n');
}


/*
 * Takes an expected unsigned long and a seen unsigned long and assumes the
 * test passes if the two numbers match.  Otherwise, reports them in hex.
 */
void
is_hex(unsigned long wanted, unsigned long seen, const char *format, ...)
{
    fflush(stderr);
    if (wanted == seen)
        printf("ok %lu", testnum++);
    else {
        printf("# wanted: %lx\n#   seen: %lx\n", (unsigned long) wanted,
               (unsigned long) seen);
        printf("not ok %lu", testnum++);
        _failed++;
    }
    if (format != NULL) {
        va_list args;

        va_start(args, format);
        print_desc(format, args);
        va_end(args);
    }
    putchar('\n');
}


/*
 * Bail out with an error.
 */
void
bail(const char *format, ...)
{
    va_list args;

    fflush(stderr);
    fflush(stdout);
    printf("Bail out! ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
    exit(1);
}


/*
 * Bail out with an error, appending strerror(errno).
 */
void
sysbail(const char *format, ...)
{
    va_list args;
    int oerrno = errno;

    fflush(stderr);
    fflush(stdout);
    printf("Bail out! ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(": %s\n", strerror(oerrno));
    exit(1);
}


/*
 * Report a diagnostic to stderr.
 */
void
diag(const char *format, ...)
{
    va_list args;

    fflush(stderr);
    fflush(stdout);
    printf("# ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}


/*
 * Report a diagnostic to stderr, appending strerror(errno).
 */
void
sysdiag(const char *format, ...)
{
    va_list args;
    int oerrno = errno;

    fflush(stderr);
    fflush(stdout);
    printf("# ");
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(": %s\n", strerror(oerrno));
}


/*
 * Locate a test file.  Given the partial path to a file, look under BUILD and
 * then SOURCE for the file and return the full path to the file.  Returns
 * NULL if the file doesn't exist.  A non-NULL return should be freed with
 * test_file_path_free().
 *
 * This function uses sprintf because it attempts to be independent of all
 * other portability layers.  The use immediately after a memory allocation
 * should be safe without using snprintf or strlcpy/strlcat.
 */
char *
test_file_path(const char *file)
{
    char *base;
    char *path = NULL;
    size_t length;
    const char *envs[] = { "BUILD", "SOURCE", NULL };
    int i;

    for (i = 0; envs[i] != NULL; i++) {
        base = getenv(envs[i]);
        if (base == NULL)
            continue;
        length = strlen(base) + 1 + strlen(file) + 1;
        path = malloc(length);
        if (path == NULL)
            sysbail("cannot allocate memory");
        sprintf(path, "%s/%s", base, file);
        if (access(path, R_OK) == 0)
            break;
        free(path);
        path = NULL;
    }
    return path;
}


/*
 * Free a path returned from test_file_path().  This function exists primarily
 * for Windows, where memory must be freed from the same library domain that
 * it was allocated from.
 */
void
test_file_path_free(char *path)
{
    if (path != NULL)
        free(path);
}


#ifndef LIBTEST_NEW_FORMAT
/* A global buffer into which message_log_buffer stores error messages. */
char *errors = NULL;

/*
 *  An error handler that appends all errors to the errors global.  Used by
 *  error_capture.
 */
static void
message_log_buffer(int len, const char *fmt, va_list args, int error UNUSED)
{
    char *message;

    message = xmalloc(len + 1);
    vsnprintf(message, len + 1, fmt, args);
    if (errors == NULL) {
        errors = concat(message, "\n", (char *) 0);
    } else {
        char *new_errors;

        new_errors = concat(errors, message, "\n", (char *) 0);
        free(errors);
        errors = new_errors;
    }
    free(message);
}


/*
 *  Turn on the capturing of errors.  Errors will be stored in the global
 *  errors variable where they can be checked by the test suite.  Capturing is
 *  turned off with errors_uncapture.
 */
void
errors_capture(void)
{
    if (errors != NULL) {
        free(errors);
        errors = NULL;
    }
    message_handlers_warn(1, message_log_buffer);
    message_handlers_notice(1, message_log_buffer);
}


/*
 *  Turn off the capturing of errors again.
 */
void
errors_uncapture(void)
{
    message_handlers_warn(1, message_log_stderr);
    message_handlers_notice(1, message_log_stdout);
}
#endif
