/*
 * inet_ntop test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2005, 2017, 2020 Russ Allbery <eagle@eyrie.org>
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
#include "portable/socket.h"
#include "portable/system.h"

#include <errno.h>

#include "tap/basic.h"

/* Some systems too old to have inet_ntop don't have EAFNOSUPPORT. */
#ifndef EAFNOSUPPORT
#    define EAFNOSUPPORT EDOM
#endif

const char *test_inet_ntop(int, const void *, char *, socklen_t);


static void
test_addr(const char *expected, uint32_t addr)
{
    struct in_addr in;
    char result[INET_ADDRSTRLEN];

    in.s_addr = htonl(addr);
    if (test_inet_ntop(AF_INET, &in, result, sizeof(result)) == NULL) {
        printf("# cannot convert %u: %s", addr, strerror(errno));
        ok(0, "converting %s", expected);
    } else
        ok(1, "converting %s", expected);
    is_string(expected, result, "...with correct result");
}


int
main(void)
{
    plan(6 + 5 * 2);

    ok(test_inet_ntop(AF_UNIX, NULL, NULL, 0) == NULL, "AF_UNIX failure");
    is_int(EAFNOSUPPORT, errno, "...with right errno");
    ok(test_inet_ntop(AF_INET, NULL, NULL, 0) == NULL, "empty buffer");
    is_int(ENOSPC, errno, "...with right errno");
    ok(test_inet_ntop(AF_INET, NULL, NULL, 11) == NULL, "NULL buffer");
    is_int(ENOSPC, errno, "...with right errno");

    /* clang-format off */
    test_addr(        "0.0.0.0", 0x0);
    test_addr(      "127.0.0.0", 0x7f000000U);
    test_addr("255.255.255.255", 0xffffffffU);
    test_addr("172.200.232.199", 0xacc8e8c7U);
    test_addr(        "1.2.3.4", 0x01020304U);
    /* clang-format on */

    return 0;
}
