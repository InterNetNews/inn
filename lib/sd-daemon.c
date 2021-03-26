/* $Id$
 *
 * Stubs for systemd library functions.
 *
 * Provides empty stubs for sd_notify and sd_notifyf that do nothing and
 * return 0, similar to the behavior of the proper library functions if not
 * run under systemd.  Unsetting the environment (the first argument) is not
 * supported.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2021 Russ Allbery <eagle@eyrie.org>
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include "portable/macros.h"
#include "portable/sd-daemon.h"

/* Used for unused parameters to silence gcc warnings. */
#define UNUSED __attribute__((__unused__))


int
sd_notify(int unset_environment UNUSED, const char *state UNUSED)
{
    return 0;
}


int
sd_notifyf(int unset_environment UNUSED, const char *state UNUSED, ...)
{
    return 0;
}
