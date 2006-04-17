/* $Id$ */
/* asprintf and vasprintf test suite. */

#include "config.h"
#include "clibrary.h"

#include "libtest.h"

static int
vatest(char **result, const char *format, ...)
{
    va_list args;
    int status;

    va_start(args, format);
    status = vasprintf(result, format, args);
    va_end(args);
    return status;
}

int
main(void)
{
    char *result = NULL;

    test_init(12);

    ok_int(1, 7, asprintf(&result, "%s", "testing"));
    ok_string(2, "testing", result);
    free(result);
    ok(3, true);
    ok_int(4, 0, asprintf(&result, "%s", ""));
    ok_string(5, "", result);
    free(result);
    ok(6, true);

    ok_int(7, 6, vatest(&result, "%d %s", 2, "test"));
    ok_string(8, "2 test", result);
    free(result);
    ok(9, true);
    ok_int(10, 0, vatest(&result, "%s", ""));
    ok_string(11, "", result);
    free(result);
    ok(12, true);

    return 0;
}
