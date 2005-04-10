/* $Id$ */
/* setenv test suite. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/messages.h"
#include "libinn.h"
#include "libtest.h"

int test_setenv(const char *name, const char *value, int overwrite);

static const char test_var[] = "SETENV_TEST";
static const char test_value1[] = "Do not taunt Happy Fun Ball.";
static const char test_value2[] = "Do not use Happy Fun Ball on concrete.";

int
main(void)
{
    char *value;
    int status;

    if (getenv(test_var))
        die("%s already in the environment!", test_var);

    test_init(12);

    ok(1, test_setenv(test_var, test_value1, 0) == 0);
    ok_string(2, test_value1, getenv(test_var));
    ok(3, test_setenv(test_var, test_value2, 0) == 0);
    ok_string(4, test_value1, getenv(test_var));
    ok(5, test_setenv(test_var, test_value2, 1) == 0);
    ok_string(6, test_value2, getenv(test_var));
    ok(7, test_setenv(test_var, "", 1) == 0);
    ok_string(8, "", getenv(test_var));

    /* We're run by a shell script wrapper that sets resource limits such
       that we can allocate one string of this size but not two.  Note that
       Linux doesn't support data limits, so skip if we get an unexpected
       success here. */
    value = xmalloc(100 * 1024);
    memset(value, 'A', 100 * 1024 - 1);
    value[100 * 1024 - 1] = 0;
    ok(9, test_setenv(test_var, value, 0) == 0);
    ok_string(10, "", getenv(test_var));
    status = test_setenv(test_var, value, 1);
    if (status == 0) {
        skip_block(11, 2, "no data limit support");
    } else {
        ok(11, (status == -1) && (errno == ENOMEM));
        ok_string(12, "", getenv(test_var));
    }

    return 0;
}
