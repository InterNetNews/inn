/* $Id$ */
/* Test suite for xmalloc and family. */

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <unistd.h>

/* Linux requires sys/time.h be included before sys/resource.h. */
#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <sys/resource.h>

#include "inn/messages.h"
#include "libinn.h"

/* A customized error handler for checking xmalloc's support of them.
   Prints out the error message and exits with status 1. */
static void
test_handler(const char *function, size_t size, const char *file, int line)
{
    die("%s %lu %s %d", function, (unsigned long) size, file, line);
}

/* Allocate the amount of memory given and write to all of it to make sure
   we can, returning true if that succeeded and false on any sort of
   detectable error. */
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

/* Allocate half the memory given, write to it, then reallocate to the
   desired size, writing to the rest and then checking it all.  Returns true
   on success, false on any failure. */
static int
test_realloc(size_t size)
{
    char *buffer;
    size_t i;

    buffer = xmalloc(size / 2);
    if (buffer == NULL)
        return 0;
    if (size / 2 > 0)
        memset(buffer, 1, size / 2);
    buffer = xrealloc(buffer, size);
    if (buffer == NULL)
        return 0;
    if (size > 0)
        memset(buffer + size / 2, 2, size - size / 2);
    for (i = 0; i < size / 2; i++)
        if (buffer[i] != 1)
            return 0;
    for (i = size / 2; i < size; i++)
        if (buffer[i] != 2)
            return 0;
    free(buffer);
    return 1;
}

/* Generate a string of the size indicated, call xstrdup on it, and then
   ensure the result matches.  Returns true on success, false on any
   failure. */
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

/* Generate a string of the size indicated plus some, call xstrndup on it, and
   then ensure the result matches.  Returns true on success, false on any
   failure. */
static int
test_strndup(size_t size)
{
    char *string, *copy;
    int match, toomuch;

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
    return (match == 0 && toomuch != 0);
}

/* Allocate the amount of memory given and check that it's all zeroed,
   returning true if that succeeded and false on any sort of detectable
   error. */
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

/* Take the amount of memory to allocate in bytes as a command-line argument
   and call test_malloc with that amount of memory. */
int
main(int argc, char *argv[])
{
    size_t size, max;
    size_t limit = 0;
    int willfail = 0;
    unsigned char code;
    struct rlimit rl;

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

    /* If a memory limit was given and we can set memory limits, set it.
       Otherwise, exit 2, signalling to the driver that the test should be
       skipped.  We do this here rather than in the driver due to some
       pathological problems with Linux (setting ulimit in the shell caused
       the shell to die). */
    if (limit > 0) {
#if HAVE_SETRLIMIT && defined(RLIMIT_DATA)
        rl.rlim_cur = limit;
        rl.rlim_max = limit;
        if (setrlimit(RLIMIT_DATA, &rl) < 0) {
            syswarn("Can't set data limit to %lu", (unsigned long) limit);
            exit(2);
        }
#else
        warn("Data limits aren't supported.");
        exit(2);
#endif
    }

    /* If the code is capitalized, install our customized error handler. */
    code = argv[1][0];
    if (isupper(code)) {
        xmalloc_error_handler = test_handler;
        code = tolower(code);
    }

    /* Decide if the allocation should fail.  If it should, set willfail to
       2, so that if it unexpectedly succeeds, we exit with a status
       indicating that the test should be skipped. */
    max = size;
    if (code == 's' || code == 'n')
        max *= 2;
    if (limit > 0 && max > limit)
        willfail = 2;

    switch (code) {
    case 'c': exit(test_calloc(size) ? willfail : 1);
    case 'm': exit(test_malloc(size) ? willfail : 1);
    case 'r': exit(test_realloc(size) ? willfail : 1);
    case 's': exit(test_strdup(size) ? willfail : 1);
    case 'n': exit(test_strndup(size) ? willfail : 1);
    default:
        die("Unknown mode %c", argv[1][0]);
        break;
    }
    exit(1);
}
