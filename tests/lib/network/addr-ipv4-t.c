/* $Id$
 *
 * Test network address functions for IPv4.
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
    struct addrinfo *ai, *ai2;
    struct addrinfo hints;
    char addr[INET6_ADDRSTRLEN];
    static const char *port = "119";

    /* Set up the plan. */
    plan(29);

    /* Get a sockaddr to use for subsequent tests. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    status = getaddrinfo("127.0.0.1", port, &hints, &ai);
    if (status != 0)
        bail("getaddrinfo on 127.0.0.1 failed: %s", gai_strerror(status));

    /* Test network_sockaddr_sprint. */
    ok(network_sockaddr_sprint(addr, sizeof(addr), ai->ai_addr),
       "sprint of 127.0.0.1");
    is_string("127.0.0.1", addr, "...with right results");

    /* Test network_sockaddr_port. */
    is_int(119, network_sockaddr_port(ai->ai_addr), "sockaddr_port");

    /* Test network_sockaddr_equal. */
    ok(network_sockaddr_equal(ai->ai_addr, ai->ai_addr), "sockaddr_equal");
    status = getaddrinfo("127.0.0.2", NULL, &hints, &ai2);
    if (status != 0)
        bail("getaddrinfo on 127.0.0.2 failed: %s", gai_strerror(status));
    ok(!network_sockaddr_equal(ai->ai_addr, ai2->ai_addr),
       "sockaddr_equal of unequal addresses");
    ok(!network_sockaddr_equal(ai2->ai_addr, ai->ai_addr),
       "...and the other way around");
    freeaddrinfo(ai2);

    /* Check the domains of functions and their error handling. */
    ai->ai_addr->sa_family = AF_UNIX;
    ok(!network_sockaddr_equal(ai->ai_addr, ai->ai_addr),
       "network_sockaddr_equal returns false for equal AF_UNIX addresses");
    is_int(0, network_sockaddr_port(ai->ai_addr),
           "port meaningless for AF_UNIX");
    freeaddrinfo(ai);

    /* Tests for network_addr_compare. */
    is_addr_compare(1, "127.0.0.1", "127.0.0.1",   NULL);
    is_addr_compare(0, "127.0.0.1", "127.0.0.2",   NULL);
    is_addr_compare(1, "127.0.0.1", "127.0.0.0",   "31");
    is_addr_compare(0, "127.0.0.1", "127.0.0.0",   "32");
    is_addr_compare(0, "127.0.0.1", "127.0.0.0",   "255.255.255.255");
    is_addr_compare(1, "127.0.0.1", "127.0.0.0",   "255.255.255.254");
    is_addr_compare(1, "10.10.4.5", "10.10.4.255", "24");
    is_addr_compare(0, "10.10.4.5", "10.10.4.255", "25");
    is_addr_compare(1, "10.10.4.5", "10.10.4.255", "255.255.255.0");
    is_addr_compare(0, "10.10.4.5", "10.10.4.255", "255.255.255.128");
    is_addr_compare(0, "129.0.0.0", "1.0.0.0",     "1");
    is_addr_compare(1, "129.0.0.0", "1.0.0.0",     "0");
    is_addr_compare(1, "129.0.0.0", "1.0.0.0",     "0.0.0.0");

    /* Test some invalid addresses. */
    is_addr_compare(0, "fred",      "fred",        NULL);
    is_addr_compare(0, "",          "",            NULL);
    is_addr_compare(0, "",          "",            "0");
    is_addr_compare(0, "127.0.0.1", "127.0.0.1",   "pete");
    is_addr_compare(0, "127.0.0.1", "127.0.0.1",   "1p");
    is_addr_compare(0, "127.0.0.1", "127.0.0.1",   "1p");
    is_addr_compare(0, "127.0.0.1", "127.0.0.1",   "-1");
    is_addr_compare(0, "127.0.0.1", "127.0.0.1",   "33");
    return 0;
}
