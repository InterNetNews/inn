/* $Id$
 *
 * inet_ntoa test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2000-2001, 2004, 2016-2017 Russ Allbery <eagle@eyrie.org>
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
#include "portable/socket.h"

#include "tap/basic.h"

char *test_inet_ntoa(struct in_addr);


static void
test_addr(const char *expected, uint32_t addr)
{
    struct in_addr in;

    in.s_addr = htonl(addr);
    is_string(expected, test_inet_ntoa(in), "address %s", expected);
}


int
main(void)
{
    plan(5);

    test_addr(        "0.0.0.0", 0x0);
    test_addr(      "127.0.0.0", 0x7f000000U);
    test_addr("255.255.255.255", 0xffffffffU);
    test_addr("172.200.232.199", 0xacc8e8c7U);
    test_addr(        "1.2.3.4", 0x01020304U);

    return 0;
}
