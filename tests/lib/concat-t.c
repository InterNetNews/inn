/* $Id$ */
/* concat test suite. */

#include <stdio.h>
#include <string.h>

#include "libinn.h"
#include "libtest.h"

#define END     (char *) 0

int
main(void)
{
    test_init(11);

    ok_string( 1, "a",     concat("a",                   END));
    ok_string( 2, "ab",    concat("a", "b",              END));
    ok_string( 3, "ab",    concat("ab", "",              END));
    ok_string( 4, "ab",    concat("", "ab",              END));
    ok_string( 5, "",      concat("",                    END));
    ok_string( 6, "abcde", concat("ab", "c", "", "de",   END));
    ok_string( 7, "abcde", concat("abc", "de", END, "f", END));

    ok_string( 8, "/foo",             concatpath("/bar", "/foo"));
    ok_string( 9, "/foo/bar",         concatpath("/foo", "bar"));
    ok_string(10, "./bar",            concatpath("/foo", "./bar"));
    ok_string(11, "/bar/baz/foo/bar", concatpath("/bar/baz", "foo/bar"));

    return 0;
}
