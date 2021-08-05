/*
 * inet_aton test suite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2000-2001, 2004, 2017, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2007-2009, 2011
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
#include "clibrary.h"

#include "tap/basic.h"

int test_inet_aton(const char *, struct in_addr *);


static void
test_addr(const char *string, uint32_t addr)
{
    int success;
    struct in_addr in;

    success = test_inet_aton(string, &in);
    ok(success, "inet_aton on %s", string);
    is_hex(htonl(addr), in.s_addr, "...matches expected value");
}


static void
test_fail(const char *string)
{
    struct in_addr in;
    int success;

    in.s_addr = htonl(0x01020304UL);
    success = test_inet_aton(string, &in);
    ok(success == 0, "inet_aton on %s fails", string);
    is_hex(htonl(0x01020304UL), in.s_addr, "...and leaves in unchanged");
}


int
main(void)
{
    plan(92);

    /* clang-format off */

    test_addr(            "0.0.0.0", 0);
    test_addr(     "127.0.0.000000", 0x7f000000U);
    test_addr(    "255.255.255.255", 0xffffffffU);
    test_addr(    "172.200.232.199", 0xacc8e8c7U);
    test_addr(            "1.2.3.4", 0x01020304U);

    test_addr(    "0x0.0x0.0x0.0x0", 0);
    test_addr("0x7f.0x000.0x0.0x00", 0x7f000000U);
    test_addr("0xff.0xFf.0xFF.0xff", 0xffffffffU);
    test_addr("0xAC.0xc8.0xe8.0xC7", 0xacc8e8c7U);
    test_addr("0xAa.0xbB.0xCc.0xdD", 0xaabbccddU);
    test_addr("0xEe.0xfF.0.0x00000", 0xeeff0000U);
    test_addr("0x1.0x2.0x00003.0x4", 0x01020304U);

    test_addr(   "000000.00.000.00", 0);
    test_addr(             "0177.0", 0x7f000000U);
    test_addr("0377.0377.0377.0377", 0xffffffffU);
    test_addr("0254.0310.0350.0307", 0xacc8e8c7U);
    test_addr("00001.02.3.00000004", 0x01020304U);

    test_addr(           "16909060", 0x01020304U);
    test_addr(      "172.062164307", 0xacc8e8c7U);
    test_addr(    "172.0xc8.0xe8c7", 0xacc8e8c7U);
    test_addr(              "127.1", 0x7f000001U);
    test_addr(         "0xffffffff", 0xffffffffU);
    test_addr(       "127.0xffffff", 0x7fffffffU);
    test_addr(     "127.127.0xffff", 0x7f7fffffU);

    test_fail(                 "");
    test_fail(     "Donald Duck!");
    test_fail(       "a127.0.0.1");
    test_fail(         "aaaabbbb");
    test_fail(      "0x100000000");
    test_fail(      "0xfffffffff");
    test_fail(    "127.0xfffffff");
    test_fail(    "127.376926742");
    test_fail( "127.127.01452466");
    test_fail("127.127.127.0x100");
    test_fail(            "256.0");
    test_fail( "127.0378.127.127");
    test_fail("127.127.0x100.127");
    test_fail(        "127.0.o.1");
    test_fail( "127.127.127.127v");
    test_fail(   "ef.127.127.127");
    test_fail( "0128.127.127.127");
    test_fail(         "0xeg.127");
    test_fail(         ".127.127");
    test_fail(         "127.127.");
    test_fail(         "127..127");
    test_fail(      "de.ad.be.ef");

    /* clang-format on */

    return 0;
}
