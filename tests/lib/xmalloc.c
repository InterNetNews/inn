/* $Id$
 *
 * Test suite for xmalloc and family.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2000, 2001, 2006 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2012, 2013, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define LIBTEST_NEW_FORMAT 1

#line 1 "xmalloc.c"

#include "config.h"
#include "clibrary.h"

#include <ctype.h>
#include <errno.h>
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

/* Linux requires sys/time.h be included before sys/resource.h. */
#include <sys/resource.h>

#include "inn/messages.h"
#include "inn/xmalloc.h"


/*
 * A customized error handler for checking xmalloc's support of them.  Prints
 * out the error message and exits with status 1.
 */
static void
test_handler(const char *function, size_t size, const char *file, int line)
{
    die("%s %lu %s %d", function, (unsigned long) size, file, line);
}


/*
 * Allocate the amount of memory given and write to all of it to make sure we
 * can, returning true if that succeeded and false on any sort of detectable
 * error.
 */
static int
test_malloc(size_t size)
{
    char *buffer;
    size_t i;

    buffer = xmalloc(size);
    if (buffer == NULL)
        return 0;
    if (size > 0)
        memset(buffer, 1, size);
    for (i = 0; i < size; i++)
        if (buffer[i] != 1)
            return 0;
    free(buffer);
    return 1;
}


/*
 * Allocate 10 bytes of memory given, write to it, then reallocate to the
 * desired size, writing to the rest and then checking it all.  Returns true
 * on success, false on any failure.
 */
static int
test_realloc(size_t size)
{
    char *buffer;
    size_t i;

    buffer = xmalloc(10);
    if (buffer == NULL)
        return 0;
    memset(buffer, 1, 10);
    buffer = xrealloc(buffer, size);
    if (buffer == NULL)
        return 0;
    if (size > 0)
        memset(buffer + 10, 2, size - 10);
    for (i = 0; i < 10; i++)
        if (buffer[i] != 1)
            return 0;
    for (i = 10; i < size; i++)
        if (buffer[i] != 2)
            return 0;
    free(buffer);
    return 1;
}


/*
 * Like test_realloc, but test allocating an array instead.  Returns true on
 * success, false on any failure.
 */
static int
test_reallocarray(size_t n, size_t size)
{
    char *buffer;
    size_t i;

    buffer = xmalloc(10);
    if (buffer == NULL)
        return 0;
    memset(buffer, 1, 10);
    buffer = xreallocarray(buffer, n, size);
    if (buffer == NULL)
        return 0;
    if (n > 0 && size > 0)
        memset(buffer + 10, 2, (n * size) - 10);
    for (i = 0; i < 10; i++)
        if (buffer[i] != 1)
            return 0;
    for (i = 10; i < n * size; i++)
        if (buffer[i] != 2)
            return 0;
    free(buffer);
    return 1;
}


/*
 * Generate a string of the size indicated, call xstrdup on it, and then
 * ensure the result matches.  Returns true on success, false on any failure.
 */
static int
test_strdup(size_t size)
{
    char *string, *copy;
    int match;

    string = xmalloc(size);
    if (string == NULL)
        return 0;
    memset(string, 1, size - 1);
    string[size - 1] = '\0';
    copy = xstrdup(string);
    if (copy == NULL)
        return 0;
    match = strcmp(string, copy);
    free(string);
    free(copy);
    return (match == 0);
}


/*
 * Generate a string of the size indicated plus some, call xstrndup on it, and
 * then ensure the result matches.  Also test xstrdup on a string that's
 * shorter than the specified size and ensure that we don't copy too much, and
 * on a string that's not nul-terminated.  Returns true on success, false on
 * any failure.
 */
static int
test_strndup(size_t size)
{
    char *string, *copy;
    int shortmatch, nonulmatch, match, toomuch;

    /* Copy a short string. */
    string = xmalloc(5);
    memcpy(string, "test", 5);
    copy = xstrndup(string, size);
    shortmatch = strcmp(string, copy);
    free(string);
    free(copy);

    /* Copy a string that's not nul-terminated. */
    string = xmalloc(4);
    memcpy(string, "test", 4);
    copy = xstrndup(string, 4);
    nonulmatch = strcmp(copy, "test");
    free(string);
    free(copy);

    /* Now the test of running out of memory. */
    string = xmalloc(size + 1);
    if (string == NULL)
        return 0;
    memset(string, 1, size - 1);
    string[size - 1] = 2;
    string[size] = '\0';
    copy = xstrndup(string, size - 1);
    if (copy == NULL)
        return 0;
    match = strncmp(string, copy, size - 1);
    toomuch = strcmp(string, copy);
    free(string);
    free(copy);
    return (shortmatch == 0 && nonulmatch == 0 && match == 0 && toomuch != 0);
}


/*
 * Allocate the amount of memory given and check that it's all zeroed,
 * returning true if that succeeded and false on any sort of detectable error.
 */
static int
test_calloc(size_t size)
{
    char *buffer;
    size_t i, nelems;

    nelems = size / 4;
    if (nelems * 4 != size)
        return 0;
    buffer = xcalloc(nelems, 4);
    if (buffer == NULL)
        return 0;
    for (i = 0; i < size; i++)
        if (buffer[i] != 0)
            return 0;
    free(buffer);
    return 1;
}


/*
 * Test asprintf with a large string (essentially using it as strdup).
 * Returns true if successful, false otherwise.
 */
static int
test_asprintf(size_t size)
{
    char *copy, *string;
    size_t i;

    string = xmalloc(size);
    memset(string, 42, size - 1);
    string[size - 1] = '\0';
    xasprintf(&copy, "%s", string);
    free(string);
    for (i = 0; i < size - 1; i++)
        if (copy[i] != 42)
            return 0;
    if (copy[size - 1] != '\0')
        return 0;
    free(copy);
    return 1;
}


/* Wrapper around vasprintf to do the va_list stuff. */
static void
xvasprintf_wrapper(char **strp, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    xvasprintf(strp, format, args);
    va_end(args);
}


/*
 * Test vasprintf with a large string (essentially using it as strdup).
 * Returns true if successful, false otherwise.
 */
static int
test_vasprintf(size_t size)
{
    char *copy, *string;
    size_t i;

    string = xmalloc(size);
    memset(string, 42, size - 1);
    string[size - 1] = '\0';
    xvasprintf_wrapper(&copy, "%s", string);
    free(string);
    for (i = 0; i < size - 1; i++)
        if (copy[i] != 42)
            return 0;
    if (copy[size - 1] != '\0')
        return 0;
    free(copy);
    return 1;
}


/*
 * Take the amount of memory to allocate in bytes as a command-line argument
 * and call test_malloc with that amount of memory.
 */
int
main(int argc, char *argv[])
{
    size_t size, max;
    size_t limit = 0;
    int willfail = 0;
    unsigned char code;

    if (argc < 3)
        die("Usage error.  Type, size, and limit must be given.");
    errno = 0;
    size = strtol(argv[2], 0, 10);
    if (size == 0 && errno != 0)
        sysdie("Invalid size");
    errno = 0;
    limit = strtol(argv[3], 0, 10);
    if (limit == 0 && errno != 0)
        sysdie("Invalid limit");

    /* If the code is capitalized, install our customized error handler. */
    code = argv[1][0];
    if (isupper(code)) {
        xmalloc_error_handler = test_handler;
        code = tolower(code);
    }

    /*
     * Decide if the allocation should fail.  If it should, set willfail to 2,
     * so that if it unexpectedly succeeds, we exit with a status indicating
     * that the test should be skipped.
     */
    max = size;
    if (code == 's' || code == 'n' || code == 'a' || code == 'v') {
        max += size;
        if (limit > 0)
            limit += size;
    }
    if (limit > 0 && max > limit)
        willfail = 2;

    /*
     * If a memory limit was given and we can set memory limits, set it.
     * Otherwise, exit 2, signalling to the driver that the test should be
     * skipped.  We do this here rather than in the driver due to some
     * pathological problems with Linux (setting ulimit in the shell caused
     * the shell to die).
     */
    if (limit > 0) {
#if HAVE_SETRLIMIT && defined(RLIMIT_AS)
        struct rlimit rl;
        void *tmp;
        size_t test_size;

        rl.rlim_cur = limit;
        rl.rlim_max = limit;
        if (setrlimit(RLIMIT_AS, &rl) < 0) {
            syswarn("Can't set data limit to %lu", (unsigned long) limit);
            exit(2);
        }
        if (size < limit || code == 'r' || code == 'y') {
            test_size = (code == 'r' || code == 'y') ? 10 : size;
            if (test_size == 0)
                test_size = 1;
            tmp = malloc(test_size);
            if (tmp == NULL) {
                syswarn("Can't allocate initial memory of %lu (limit %lu)",
                        (unsigned long) test_size, (unsigned long) limit);
                exit(2);
            }
            free(tmp);
        }
#else
        warn("Data limits aren't supported.");
        exit(2);
#endif
    }

    switch (code) {
    case 'c': exit(test_calloc(size) ? willfail : 1);
    case 'm': exit(test_malloc(size) ? willfail : 1);
    case 'r': exit(test_realloc(size) ? willfail : 1);
    case 'y': exit(test_reallocarray(4, size / 4) ? willfail : 1);
    case 's': exit(test_strdup(size) ? willfail : 1);
    case 'n': exit(test_strndup(size) ? willfail : 1);
    case 'a': exit(test_asprintf(size) ? willfail : 1);
    case 'v': exit(test_vasprintf(size) ? willfail : 1);
    default:
        die("Unknown mode %c", argv[1][0]);
        break;
    }
    exit(1);
}
