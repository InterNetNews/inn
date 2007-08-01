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
    test_init(5);

    test_error(1, "Internal resolver error", -1);
    test_error(2, "No resolver error", 0);
    test_error(3, "No address associated with name", NO_ADDRESS);
    test_error(4, "Resolver error 777777", 777777);
    test_error(5, "Resolver error -99999", -99999);

    return 0;
}
