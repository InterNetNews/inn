/* $Id$
 *
 * setenv test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
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

#include "config.h"
#include "clibrary.h"

#include <errno.h>

#include "tap/basic.h"

int test_setenv(const char *name, const char *value, int overwrite);

static const char test_var[] = "SETENV_TEST";
static const char test_value1[] = "Do not taunt Happy Fun Ball.";
static const char test_value2[] = "Do not use Happy Fun Ball on concrete.";


int
main(void)
{
    plan(8);

    if (getenv(test_var))
        bail("%s already in the environment!", test_var);

    ok(test_setenv(test_var, test_value1, 0) == 0, "set string 1");
    is_string(test_value1, getenv(test_var), "...and getenv correct");
    ok(test_setenv(test_var, test_value2, 0) == 0, "set string 2");
    is_string(test_value1, getenv(test_var), "...and getenv unchanged");
    ok(test_setenv(test_var, test_value2, 1) == 0, "overwrite string 2");
    is_string(test_value2, getenv(test_var), "...and getenv changed");
    ok(test_setenv(test_var, "", 1) == 0, "overwrite with empty string");
    is_string("", getenv(test_var), "...and getenv correct");

    return 0;
}
