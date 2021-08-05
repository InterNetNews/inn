/*
 * setenv test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2000-2006, 2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2006-2009, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
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
