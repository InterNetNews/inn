/* $Id$ */
/* strerror test suite. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "libtest.h"

const char *test_strerror(int);

int
main(void)
{
    puts("5");

#if HAVE_STRERROR
    ok_string(1, strerror(EACCES), test_strerror(EACCES));
    ok_string(2, strerror(0), test_strerror(0));
#else
    ok(1, strerror(EACCES) != NULL);
    ok(2, strerror(0) != NULL);
#endif
    ok_string(3, "Error code 77777", test_strerror(77777));
    ok_string(4, "Error code -4000", test_strerror(-4000));
    ok_string(5, "", test_strerror(-100000));

    return 0;
}
