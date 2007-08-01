/* $Id$ */
/* hstrerror test suite. */

#include "config.h"
#include <netdb.h>
#include <stdio.h>

#include "libtest.h"

const char *test_hstrerror(int);

static void
test_error(int n, const char *expected, int error)
{
    ok_string(n, expected, test_hstrerror(error));
}

int
main(void)
{
    test_init(7);

    test_error(1, "Internal resolver error", -1);
    test_error(2, "No resolver error", 0);
    test_error(3, "No address associated with name", NO_ADDRESS);
    test_error(4, "Resolver error 777777", 777777);
    test_error(5, "Resolver error -99999", -99999);
    test_error(6, "Resolver error 1000000", 1000000);
    test_error(7, "Resolver error -100000", -100000);

    return 0;
}
