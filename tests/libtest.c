/*  $Id$
**
**  Some utility routines for writing tests.
**
**  Herein are a variety of utility routines for writing tests.  All
**  routines of the form ok*() take a test number and some number of
**  appropriate arguments, check to be sure the results match the expected
**  output using the arguments, and print out something appropriate for that
**  test number.  Other utility routines help in constructing more complex
**  tests.
*/

#include <stdio.h>
#include <string.h>
#include "libtest.h"

/*
**  Takes a boolean success value and assumes the test passes if that value
**  is true and fails if that value is false.
*/
void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}


/*
**  Takes a string and what the string should be, and assumes the test
**  passes if those strings match (using strcmp).
*/
void
ok_string(int n, const char *wanted, const char *seen)
{
    if (wanted == NULL)
        wanted = "(null)";
    if (seen == NULL)
        seen = "(null)";
    if (strcmp(wanted, seen) != 0)
        printf("not ok %d\n  wanted: %s\n    seen: %s\n", n, wanted, seen);
    else
        printf("ok %d\n", n);
}
