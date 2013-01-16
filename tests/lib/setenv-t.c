/* $Id$ */
/* setenv test suite. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/messages.h"
#include "inn/libinn.h"
#include "libtest.h"

int test_setenv(const char *name, const char *value, int overwrite);

static const char test_var[] = "SETENV_TEST";
static const char test_value1[] = "Do not taunt Happy Fun Ball.";
static const char test_value2[] = "Do not use Happy Fun Ball on concrete.";

int
main(void)
{
    if (getenv(test_var))
        die("%s already in the environment!", test_var);

    test_init(8);

    ok(1, test_setenv(test_var, test_value1, 0) == 0);
    ok_string(2, test_value1, getenv(test_var));
    ok(3, test_setenv(test_var, test_value2, 0) == 0);
    ok_string(4, test_value1, getenv(test_var));
    ok(5, test_setenv(test_var, test_value2, 1) == 0);
    ok_string(6, test_value2, getenv(test_var));
    ok(7, test_setenv(test_var, "", 1) == 0);
    ok_string(8, "", getenv(test_var));

    return 0;
}
