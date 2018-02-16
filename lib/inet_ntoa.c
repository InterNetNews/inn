/* $Id$
 *
 * Replacement for a missing or broken inet_ntoa.
 *
 * Provides the same functionality as the standard library routine inet_ntoa
 * for those platforms that don't have it or where it doesn't work right (such
 * as on IRIX when using gcc to compile).  inet_ntoa is not thread-safe since
 * it uses static storage (inet_ntop should be used instead when available).
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2000-2001, 2017 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

/*
 * If we're running the test suite, rename inet_ntoa to avoid conflicts with
 * the system version.
 */
#if TESTING
# undef inet_ntoa
# define inet_ntoa test_inet_ntoa
char *test_inet_ntoa(struct in_addr);
#endif

char *
inet_ntoa(struct in_addr in)
{
    static char buf[16];
    const unsigned char *p;

    p = (const unsigned char *) &in.s_addr;
    sprintf(buf, "%u.%u.%u.%u",
            (unsigned int) (p[0] & 0xff), (unsigned int) (p[1] & 0xff),
            (unsigned int) (p[2] & 0xff), (unsigned int) (p[3] & 0xff));
    return buf;
}
