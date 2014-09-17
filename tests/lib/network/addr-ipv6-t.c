/* $Id$
 *
 * Test network address functions for IPv6.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2005, 2013 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2010, 2011, 2012, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include <ctype.h>

#include "tap/basic.h"
#include "inn/network.h"


/*
 * Tests network_addr_compare.  Takes the expected result, the two addresses,
 * and the mask.
 */
static void
is_addr_compare(bool expected, const char *a, const char *b, const char *mask)
{
    const char *smask = (mask == NULL) ? "(null)" : mask;

    if (expected)
        ok(network_addr_match(a, b, mask), "compare %s %s %s", a, b, smask);
    else
        ok(!network_addr_match(a, b, mask), "compare %s %s %s", a, b, smask);
}


int
main(void)
{
    int status;
    struct addrinfo *ai4, *ai6;
    struct addrinfo hints;
    char addr[INET6_ADDRSTRLEN];
    char *p;
    static const char *port = "119";
    static const char *ipv6_addr = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";

#ifndef HAVE_INET6
    skip_all("IPv6 not supported");
#endif

    /* Set up the plan. */
    plan(28);

    /* Get IPv4 and IPv6 sockaddrs to use for subsequent tests. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    status = getaddrinfo("127.0.0.1", port, &hints, &ai4);
    if (status != 0)
        bail("getaddrinfo on 127.0.0.1 failed: %s", gai_strerror(status));
    status = getaddrinfo(ipv6_addr, port, &hints, &ai6);
    if (status != 0)
        bail("getaddr on %s failed: %s", ipv6_addr, gai_strerror(status));

    /* Test network_sockaddr_sprint. */
    ok(network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr),
       "sprint of IPv6 address");
    for (p = addr; *p != '\0'; p++)
        if (islower((unsigned char) *p))
            *p = toupper((unsigned char) *p);
    is_string(ipv6_addr, addr, "...with right results");

    /* Test network_sockaddr_port. */
    is_int(119, network_sockaddr_port(ai6->ai_addr), "sockaddr_port IPv6");

    /* Test network_sockaddr_equal. */
    ok(network_sockaddr_equal(ai6->ai_addr, ai6->ai_addr),
       "sockaddr_equal IPv6");
    ok(!network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr),
       "...and not equal to IPv4");
    ok(!network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr),
       "...other way around");
    freeaddrinfo(ai6);

    /* Test IPv4 mapped addresses. */
    status = getaddrinfo("::ffff:7f00:1", NULL, &hints, &ai6);
    if (status != 0)
        bail("getaddr on ::ffff:7f00:1 failed: %s", gai_strerror(status));
    ok(network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr),
       "sprint of IPv4-mapped address");
    is_string("127.0.0.1", addr, "...with right IPv4 result");
    ok(network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr),
       "sockaddr_equal of IPv4-mapped address");
    ok(network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr),
       "...and other way around");
    freeaddrinfo(ai4);
    status = getaddrinfo("127.0.0.2", NULL, &hints, &ai4);
    if (status != 0)
        bail("getaddrinfo on 127.0.0.2 failed: %s", gai_strerror(status));
    ok(!network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr),
       "...but not some other address");
    ok(!network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr),
       "...and the other way around");
    freeaddrinfo(ai6);
    freeaddrinfo(ai4);

    /* Tests for network_addr_compare. */
    is_addr_compare(1, ipv6_addr,   ipv6_addr,     NULL);
    is_addr_compare(1, ipv6_addr,   ipv6_addr,     "128");
    is_addr_compare(1, ipv6_addr,   ipv6_addr,     "60");
    is_addr_compare(1, "::127",     "0:0::127",    "128");
    is_addr_compare(1, "::127",     "0:0::128",    "120");
    is_addr_compare(0, "::127",     "0:0::128",    "128");
    is_addr_compare(0, "::7fff",    "0:0::8000",   "113");
    is_addr_compare(1, "::7fff",    "0:0::8000",   "112");
    is_addr_compare(0, "::3:ffff",  "::2:ffff",    "120");
    is_addr_compare(0, "::3:ffff",  "::2:ffff",    "119");
    is_addr_compare(0, "ffff::1",   "7fff::1",     "1");
    is_addr_compare(1, "ffff::1",   "7fff::1",     "0");
    is_addr_compare(0, "fffg::1",   "fffg::1",     NULL);
    is_addr_compare(0, "ffff::1",   "7fff::1",     "-1");
    is_addr_compare(0, "ffff::1",   "ffff::1",     "-1");
    is_addr_compare(0, "ffff::1",   "ffff::1",     "129");
    return 0;
}
