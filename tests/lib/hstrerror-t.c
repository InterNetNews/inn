/* $Id$ */
/* hstrerror test suite. */

#include "config.h"
#include <netdb.h>
#include <stdio.h>

const char *test_hstrerror(int);

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    puts("7");
    ok(1, !strcmp(test_hstrerror(-1), "Internal resolver error"));
    ok(2, !strcmp(test_hstrerror(0), "No resolver error"));
    ok(3, !strcmp(test_hstrerror(NO_ADDRESS),
                  "No address associated with name"));
    ok(4, !strcmp(test_hstrerror(77777), "Resolver error 77777"));
    ok(5, !strcmp(test_hstrerror(-99999), "Resolver error -99999"));
    ok(6, !strcmp(test_hstrerror(1000000), ""));
    ok(7, !strcmp(test_hstrerror(-100000), ""));
    return 0;
}
