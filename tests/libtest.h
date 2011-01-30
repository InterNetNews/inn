/* $Id$
 *
 * Basic utility routines for the TAP protocol.
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

#ifndef LIBTEST_H
#define LIBTEST_H 1

/* These inclusions contain va_list, pid_t, __attribute__, BEGIN_DECLS,
 * END_DECLS, ARRAY_SIZE and ARRAY_END.  All used by C TAP Harness.
 */
#include "config.h"
#include "clibrary.h"
                           
#include <inn/defines.h>

BEGIN_DECLS

/*
 * The test count.  Always contains the number that will be used for the next
 * test status.
 */
extern unsigned long testnum;

/* Print out the number of tests and set standard output to line buffered. */
void plan(unsigned long count);

/*
 * Prepare for lazy planning, in which the plan will be  printed automatically
 * at the end of the test program.
 */
void plan_lazy(void);

/* Skip the entire test suite.  Call instead of plan. */
void skip_all(const char *format, ...)
    __attribute__((__noreturn__, __format__(printf, 1, 2)));

/*
 * Basic reporting functions.  The okv() function is the same as ok() but
 * takes the test description as a va_list to make it easier to reuse the
 * reporting infrastructure when writing new tests.
 */
#ifndef LIBTEST_NEW_FORMAT
void ok(int n, int success);
void new_ok(int success, const char *format, ...)
        __attribute__((__format__(printf, 2, 3)));
void ok_int(int n, int wanted, int seen);
void ok_double(int n, double wanted, double seen);
void ok_string(int n, const char *wanted, const char *seen);
void skip(int n, const char *reason);
void new_skip(const char *reason, ...)
        __attribute__((__format__(printf, 1, 2)));
void ok_block(int n, int count, int success);
void new_ok_block(unsigned long count, int success, const char *format, ...)
        __attribute__((__format__(printf, 3, 4)));
void skip_block(int n, int count, const char *reason);
void new_skip_block(unsigned long count, const char *reason, ...)
        __attribute__((__format__(printf, 2, 3)));

void test_init(int count);

/* A global buffer into which errors_capture stores errors. */
extern char *errors;

/* Turn on capturing of errors with errors_capture.  Errors reported by warn
 * will be stored in the global errors variable.  Turn this off again with
 * errors_uncapture.  Caller is responsible for freeing errors when done.
 */
void errors_capture(void);
void errors_uncapture(void);
#else
void ok(int success, const char *format, ...)
    __attribute__((__format__(printf, 2, 3)));
void skip(const char *reason, ...)
    __attribute__((__format__(printf, 1, 2)));

/* Report the same status on, or skip, the next count tests. */
void ok_block(unsigned long count, int success, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void skip_block(unsigned long count, const char *reason, ...)
    __attribute__((__format__(printf, 2, 3)));
#endif

void okv(int success, const char *format, va_list args);

/* Check an expected value against a seen value. */
void is_int(long wanted, long seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void is_double(double wanted, double seen, double epsilon,
               const char *format, ...)
    __attribute__((__format__(printf, 4, 5)));
void is_string(const char *wanted, const char *seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));
void is_hex(unsigned long wanted, unsigned long seen, const char *format, ...)
    __attribute__((__format__(printf, 3, 4)));

/* Bail out with an error.  sysbail appends strerror(errno). */
void bail(const char *format, ...)
    __attribute__((__noreturn__, __nonnull__, __format__(printf, 1, 2)));
void sysbail(const char *format, ...)
    __attribute__((__noreturn__, __nonnull__, __format__(printf, 1, 2)));

/* Report a diagnostic to stderr prefixed with #. */
void diag(const char *format, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void sysdiag(const char *format, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));

/*
 * Find a test file under BUILD or SOURCE, returning the full path.  The
 * returned path should be freed with test_file_path_free().
 */
char *test_file_path(const char *file)
    __attribute__((__malloc__, __nonnull__));
void test_file_path_free(char *path);

END_DECLS

#endif /* LIBTEST_H */
