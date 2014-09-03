/*
 * concat test suite.
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

#include <stdio.h>
#include <string.h>

#include "inn/libinn.h"
#include "tap/basic.h"

#define END (char *) 0

/*
 * Memory leaks everywhere!  Whoo-hoo!
 */
int
main(void)
{
    plan(13);

    is_string("a",     concat("a",                   END), "concat 1");
    is_string("ab",    concat("a", "b",              END), "concat 2");
    is_string("ab",    concat("ab", "",              END), "concat 3");
    is_string("ab",    concat("", "ab",              END), "concat 4");
    is_string("",      concat("",                    END), "concat 5");
    is_string("abcde", concat("ab", "c", "", "de",   END), "concat 6");
    is_string("abcde", concat("abc", "de", END, "f", END), "concat 7");

    is_string("/foo",             concatpath("/bar", "/foo"),        "path 1");
    is_string("/foo/bar",         concatpath("/foo", "bar"),         "path 2");
    is_string("./bar",            concatpath("/foo", "./bar"),       "path 3");
    is_string("/bar/baz/foo/bar", concatpath("/bar/baz", "foo/bar"), "path 4");
    is_string("./foo",            concatpath(NULL, "foo"),           "path 5");
    is_string("/foo/bar",         concatpath(NULL, "/foo/bar"),      "path 6");

    return 0;
}
