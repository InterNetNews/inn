/* $Id$ */
/* memcmp test suite. */

#include "config.h"
#include <stdio.h>
#include <sys/types.h>

#include "libtest.h"

int test_memcmp(const void *, const void *, size_t);

int
main(void)
{
    test_init(15);

    ok( 1, test_memcmp("",             "",             0) == 0);
    ok( 2, test_memcmp("",             "",             1) == 0);
    ok( 3, test_memcmp("alpha",        "alpha",        6) == 0);
    ok( 4, test_memcmp("alpha",        "beta",         5)  < 0);
    ok( 5, test_memcmp("beta",         "alpha",        5)  > 0);
    ok( 6, test_memcmp("alpha",        "apple",        1) == 0);
    ok( 7, test_memcmp("alpha",        "apple",        2)  < 0);
    ok( 8, test_memcmp("\0v",          "\0w",          2)  < 0);
    ok( 9, test_memcmp("\200\201\202", "\200\201\202", 4) == 0);
    ok(10, test_memcmp("\200\201\202", "\200\201\203", 4)  < 0);
    ok(11, test_memcmp("\200\201\203", "\200\201\202", 4)  > 0);
    ok(12, test_memcmp("al\0po",       "al\0pha",      6)  > 0);
    ok(13, test_memcmp("\100",         "\201",         1)  < 0);
    ok(14, test_memcmp("\200",         "\201",         1)  < 0);
    ok(15, test_memcmp("a",            "b",            0) == 0);

    return 0;
}
