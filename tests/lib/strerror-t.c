/* $Id$ */
/* strerror test suite. */

#include "config.h"

#include <errno.h>
#include <stdio.h>
#if STDC_HEADERS
# include <string.h>
#endif

const char *test_strerror(int);

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    puts("5");
#if HAVE_STRERROR
    ok(1, !strcmp(strerror(EACCES), test_strerror(EACCES)));
    ok(2, !strcmp(strerror(0), test_strerror(0)));
#else
    ok(1, strerror(EACCES) != NULL);
    ok(2, strerror(0) != NULL);
#endif
    ok(3, !strcmp(test_strerror(77777), "Error code 77777"));
    ok(4, !strcmp(test_strerror(-4000), "Error code -4000"));
    ok(5, !strcmp(test_strerror(-100000), ""));
    return 0;
}
