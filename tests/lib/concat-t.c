/* $Id$ */
/* concat() test suite. */

#include <stdio.h>
#include <string.h>

#include "libinn.h"

#define END     (char *) 0

void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

int
main(void)
{
    printf("7\n");
    ok(1, !strcmp("a",     concat("a",                   END)));
    ok(2, !strcmp("ab",    concat("a", "b",              END)));
    ok(3, !strcmp("ab",    concat("ab", "",              END)));
    ok(4, !strcmp("ab",    concat("", "ab",              END)));
    ok(5, !strcmp("",      concat("",                    END)));
    ok(6, !strcmp("abcde", concat("ab", "c", "", "de",   END)));
    ok(7, !strcmp("abcde", concat("abc", "de", END, "f", END)));
    return 0;
}
