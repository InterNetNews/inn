/*
 * asprintf and vasprintf test suite.
 * 
 * $Id$
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"

#include "libtest.h"

int test_asprintf(char **, const char *, ...)
    __attribute__((__format__(printf, 2, 3)));
int test_vasprintf(char **, const char *, va_list);

static int
vatest(char **result, const char *format, ...)
{
    va_list args;
    int status;

    va_start(args, format);
    status = test_vasprintf(result, format, args);
    va_end(args);
    return status;
}

int
main(void)
{
    char *result = NULL;

    plan(12);

    is_int(7, test_asprintf(&result, "%s", "testing"), "asprintf length");
    is_string("testing", result, "asprintf result");
    free(result);
    ok(3, "free asprintf");
    is_int(0, test_asprintf(&result, "%s", ""), "asprintf empty length");
    is_string("", result, "asprintf empty string");
    free(result);
    ok(6, "free asprintf of empty string");

    is_int(6, vatest(&result, "%d %s", 2, "test"), "vasprintf length");
    is_string("2 test", result, "vasprintf result");
    free(result);
    ok(9, "free vasprintf");
    is_int(0, vatest(&result, "%s", ""), "vasprintf empty length");
    is_string("", result, "vasprintf empty string");
    free(result);
    ok(12, "free vasprintf of empty string");

    return 0;
}
