/*
 * network test suite.
 *
 * $Id$
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <rra@stanford.edu>
 * Copyright 2005 Russ Allbery <rra@stanford.edu>
 * Copyright 2009, 2010, 2011
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
#include "portable/wait.h"
#include <ctype.h>
#include <errno.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "inn/libinn.h"
#include "libtest.h"

/* Set this globally to 0 if IPv6 is available but doesn't work. */
static int ipv6 = 1;

/*
 * The core of the listener.  Takes the return value of accept and handles our
 * test protocol.
 */
static void
listener_handler(socket_type client)
{
    FILE *out;
    char buffer[512];

    if (client == INVALID_SOCKET) {
        sysdiag("cannot accept connection from socket");
        ok_block(2, 0, "...socket read test");
        return;
    }
    ok(1, "...socket accept");
    out = fdopen(client, "r");
    if (fgets(buffer, sizeof(buffer), out) == NULL) {
        sysdiag("cannot read from socket");
        ok(0, "...socket read");
    }
    is_string("socket test\r\n", buffer, "...socket read");
    fclose(out);
}


/*
 * The server portion of the test.  Listens to a socket and accepts a
 * connection, making sure what is printed on that connection matches what the
 * client is supposed to print.
 */
static void
listener(socket_type fd)
{
    socket_type client;

    client = accept(fd, NULL, NULL);
    listener_handler(client);
    close(fd);
}


/*
 * A varient version of the server portion of the test.  Takes an array of
 * sockets and the size of the sockets and accepts a connection on any of
 * those sockets.
 *
 * saddr is allocated from the heap instead of using a local struct
 * sockaddr_storage to work around a misdiagnosis of strict aliasing
 * violations from gcc 4.4 (fixed in later versions).
 */
static void
listener_any(socket_type fds[], unsigned int count)
{
    socket_type client;
    unsigned int i;
    struct sockaddr *saddr;
    socklen_t slen;

    slen = sizeof(struct sockaddr_storage);
    saddr = xmalloc(slen);
    client = network_accept_any(fds, count, saddr, &slen);
    listener_handler(client);
    is_int(AF_INET, saddr->sa_family, "...address family is IPv4");
    is_int(htonl(INADDR_LOOPBACK),
           ((struct sockaddr_in *) saddr)->sin_addr.s_addr,
           "...and client address is 127.0.0.1");
    free(saddr);
    for (i = 0; i < count; i++)
        close(fds[i]);
}


/*
 * Connect to the given host on port 11119 and send a constant string to a
 * socket, used to do the client side of the testing.  Takes the source
 * address as well to pass into network_connect_host.
 */
static void
client(const char *host, const char *source)
{
    socket_type fd;
    FILE *out;

    fd = network_connect_host(host, 11119, source);
    if (fd == INVALID_SOCKET)
        sysdie("connect failed");
    out = fdopen(fd, "w");
    if (out == NULL)
        sysdie("fdopen failed");
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(0);
}


/*
 * Bring up a server on port 11119 on the loopback address and test connecting
 * to it via IPv4.  Takes an optional source address to use for client
 * connections.
 */
static void
test_ipv4(const char *source)
{
    socket_type fd;
    pid_t child;

    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    if (listen(fd, 1) < 0) {
        sysdiag("cannot listen to socket");
        ok_block(3, 0, "IPv4 server test");
    } else {
        ok(1, "IPv4 server test");
        child = fork();
        if (child < 0)
            sysbail("cannot fork");
        else if (child == 0) {
            close(fd);
            client("127.0.0.1", source);
        } else {
            listener(fd);
            waitpid(child, NULL, 0);
        }
    }
}


/*
 * Bring up a server on port 11119 on the loopback address and test connecting
 * to it via IPv6.  Takes an optional source address to use for client
 * connections.
 */
#ifdef HAVE_INET6
static void
test_ipv6(const char *source)
{
    socket_type fd;
    pid_t child;

    fd = network_bind_ipv6("::1", 11119);
    if (fd == INVALID_SOCKET) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT
            || errno == EADDRNOTAVAIL) {
            ipv6 = 0;
            skip_block(3, "IPv6 not supported");
            return;
        } else
            sysbail("cannot create socket");
    }
    if (listen(fd, 1) < 0) {
        sysdiag("cannot listen to socket");
        ok_block(3, 0, "IPv6 server test");
    } else {
        ok(1, "IPv6 server test");
        child = fork();
        if (child < 0)
            sysbail("cannot fork");
        else if (child == 0) {
            close(fd);
            client("::1", source);
        } else {
            listener(fd);
            waitpid(child, NULL, 0);
        }
    }
}
#else /* !HAVE_INET6 */
static void
test_ipv6(const char *source UNUSED)
{
    skip_block(3, "IPv6 not supported");
}
#endif /* !HAVE_INET6 */


/*
 * Bring up a server on port 11119 on all addresses and try connecting to it
 * via all of the available protocols.  Takes an optional source address to
 * use for client connections.
 */
static void
test_all(const char *source_ipv4, const char *source_ipv6 UNUSED)
{
    socket_type *fds, fd;
    unsigned int count, i;
    pid_t child;
    struct sockaddr_storage saddr;
    socklen_t size = sizeof(saddr);

    network_bind_all(11119, &fds, &count);
    if (count == 0)
        sysbail("cannot create or bind socket");
    if (count > 2) {
        diag("got more than two sockets, using just the first two");
        count = 2;
    }
    for (i = 0; i < count; i++) {
        fd = fds[i];
        if (listen(fd, 1) < 0) {
            sysdiag("cannot listen to socket %d", fd);
            ok_block(3, 0, "all address server test");
        } else {
            ok(1, "all address server test (part %d)", i);
            child = fork();
            if (child < 0)
                sysbail("cannot fork");
            else if (child == 0) {
                if (getsockname(fd, (struct sockaddr *) &saddr, &size) < 0)
                    sysbail("cannot getsockname");
                if (saddr.ss_family == AF_INET)
                    client("127.0.0.1", source_ipv4);
#ifdef HAVE_INET6
                else if (saddr.ss_family == AF_INET6)
                    client("::1", source_ipv6);
#endif
                else
                    skip_block(2, "unknown socket family %d", saddr.ss_family);
                size = sizeof(saddr);
            } else {
                listener(fd);
                waitpid(child, NULL, 0);
            }
        }
    }
    if (count == 1)
        skip_block(3, "only one listening socket");
}


/*
 * Bring up a server on port 11119 on all addresses and try connecting to it
 * via 127.0.0.1, using network_accept_any underneath.
 */
static void
test_any(void)
{
    socket_type *fds;
    unsigned int count, i;
    pid_t child;

    network_bind_all(11119, &fds, &count);
    if (count == 0)
        sysbail("cannot create or bind socket");
    for (i = 0; i < count; i++)
        if (listen(fds[i], 1) < 0) {
            sysdiag("cannot listen to socket %d", fds[i]);
            ok_block(2, 0, "accept any server test");
        }
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0)
        client("127.0.0.1", NULL);
    else {
        listener_any(fds, count);
        waitpid(child, NULL, 0);
    }
}


/*
 * Bring up a server on port 11119 on the loopback address and test connecting
 * to it via IPv4 using network_client_create.  Takes an optional source
 * address to use for client connections.
 */
static void
test_create_ipv4(const char *source)
{
    socket_type fd;
    pid_t child;

    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    if (listen(fd, 1) < 0) {
        sysdiag("cannot listen to socket");
        ok_block(3, 0, "IPv4 network client");
    } else {
        ok(1, "IPv4 network client");
        child = fork();
        if (child < 0)
            sysbail("cannot fork");
        else if (child == 0) {
            struct sockaddr_in sin;
            FILE *out;

            fd = network_client_create(PF_INET, SOCK_STREAM, source);
            if (fd == INVALID_SOCKET)
                _exit(1);
            memset(&sin, 0, sizeof(sin));
            sin.sin_family = AF_INET;
            sin.sin_port = htons(11119);
            sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
                _exit(1);
            out = fdopen(fd, "w");
            if (out == NULL)
                _exit(1);
            fputs("socket test\r\n", out);
            fclose(out);
            _exit(0);
        } else {
            listener(fd);
            waitpid(child, NULL, 0);
        }
    }
}


/*
 * Tests network_addr_compare.  Takes the expected result, the two addresses,
 * and the mask.
 */
static void
ok_addr(int expected, const char *a, const char *b, const char *mask)
{
    if (expected)
        ok(network_addr_match(a, b, mask), "compare %s %s %s", a, b, mask);
    else
        ok(!network_addr_match(a, b, mask), "compare %s %s %s", a, b, mask);
}


int
main(void)
{
    int status;
    struct addrinfo *ai, *ai4;
    struct addrinfo hints;
    char addr[INET6_ADDRSTRLEN];
    static const char *port = "119";

#ifdef HAVE_INET6
    struct addrinfo *ai6;
    char *p;
    static const char *ipv6_addr = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";
#endif

    plan(121);

    /*
     * If IPv6 support appears to be available but doesn't work, we have to
     * skip the test_all tests since they'll create a socket that we then
     * can't connect to.  This is the case on Solaris 8 without IPv6
     * configured.
     */
    test_ipv4(NULL);
    test_ipv6(NULL);
    if (ipv6)
        test_all(NULL, NULL);
    else
        skip_block(6, "IPv6 not configured");
    test_create_ipv4(NULL);

    /* This won't make a difference for loopback connections. */
    test_ipv4("127.0.0.1");
    test_ipv6("::1");
    if (ipv6)
        test_all("127.0.0.1", "::1");
    else
        skip_block(6, "IPv6 not configured");
    test_create_ipv4("127.0.0.1");

    /* We need an initialized innconf struct, but it doesn't need to contain
     * anything interesting. */
    innconf = xcalloc(1, sizeof(struct innconf));

    /* This should be equivalent to the previous tests. */
    innconf->sourceaddress = xstrdup("all");
    innconf->sourceaddress6 = xstrdup("all");

    test_ipv4(NULL);
    test_ipv6(NULL);
    if (ipv6)
        test_all(NULL, NULL);
    else
        skip_block(6, "IPv6 not configured");
    test_create_ipv4(NULL);
                            
    /* This won't make a difference for loopback connections. */
    free(innconf->sourceaddress);
    free(innconf->sourceaddress6);
    innconf->sourceaddress = xstrdup("127.0.0.1");
    innconf->sourceaddress6 = xstrdup("::1");
    test_ipv4(NULL);
    test_ipv6(NULL);
    if (ipv6)
        test_all(NULL, NULL);
    else
        skip_block(6, "IPv6 not configured");
    test_create_ipv4(NULL);

    /* Test network_accept_any. */
    test_any();

    /*
     * Now, test network_sockaddr_sprint, network_sockaddr_equal, and
     * network_sockaddr_port.
     */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    status = getaddrinfo("127.0.0.1", port, &hints, &ai4);
    if (status != 0)
        bail("getaddrinfo on 127.0.0.1 failed: %s", gai_strerror(status));
    ok(network_sockaddr_sprint(addr, sizeof(addr), ai4->ai_addr),
       "sprint of 127.0.0.1");
    is_string("127.0.0.1", addr, "...with right results");
    is_int(119, network_sockaddr_port(ai4->ai_addr),
           "sockaddr_port");
    ok(network_sockaddr_equal(ai4->ai_addr, ai4->ai_addr), "sockaddr_equal");
    status = getaddrinfo("127.0.0.2", NULL, &hints, &ai);
    if (status != 0)
        bail("getaddrinfo on 127.0.0.2 failed: %s", gai_strerror(status));
    ok(!network_sockaddr_equal(ai->ai_addr, ai4->ai_addr),
       "sockaddr_equal of unequal addresses");
    ok(!network_sockaddr_equal(ai4->ai_addr, ai->ai_addr),
       "...and the other way around");

    /* The same for IPv6. */
#ifdef HAVE_INET6
    status = getaddrinfo(ipv6_addr, port, &hints, &ai6);
    if (status != 0)
        bail("getaddr on %s failed: %s", ipv6_addr, gai_strerror(status));
    ok(network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr),
       "sprint of IPv6 address");
    for (p = addr; *p != '\0'; p++)
        if (islower((unsigned char) *p))
            *p = toupper((unsigned char) *p);
    is_string(ipv6_addr, addr, "...with right results");
    is_int(119, network_sockaddr_port(ai6->ai_addr), "sockaddr_port IPv6");
    ok(network_sockaddr_equal(ai6->ai_addr, ai6->ai_addr),
       "sockaddr_equal IPv6");
    ok(!network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr),
       "...and not equal to IPv4");
    ok(!network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr),
       "...other way around");

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
    ok(!network_sockaddr_equal(ai->ai_addr, ai6->ai_addr),
       "...but not some other address");
    ok(!network_sockaddr_equal(ai6->ai_addr, ai->ai_addr),
       "...and the other way around");
    freeaddrinfo(ai6);
#else
    skip_block(12, "IPv6 not supported");
#endif

    /* Check the domains of functions and their error handling. */
    ai4->ai_addr->sa_family = AF_UNIX;
    ok(!network_sockaddr_equal(ai4->ai_addr, ai4->ai_addr),
       "equal not equal with address mismatches");
    is_int(0, network_sockaddr_port(ai4->ai_addr),
           "port meaningless for AF_UNIX");

    /* Tests for network_addr_compare. */
    ok_addr(1, "127.0.0.1", "127.0.0.1",   NULL);
    ok_addr(0, "127.0.0.1", "127.0.0.2",   NULL);
    ok_addr(1, "127.0.0.1", "127.0.0.0",   "31");
    ok_addr(0, "127.0.0.1", "127.0.0.0",   "32");
    ok_addr(0, "127.0.0.1", "127.0.0.0",   "255.255.255.255");
    ok_addr(1, "127.0.0.1", "127.0.0.0",   "255.255.255.254");
    ok_addr(1, "10.10.4.5", "10.10.4.255", "24");
    ok_addr(0, "10.10.4.5", "10.10.4.255", "25");
    ok_addr(1, "10.10.4.5", "10.10.4.255", "255.255.255.0");
    ok_addr(0, "10.10.4.5", "10.10.4.255", "255.255.255.128");
    ok_addr(0, "129.0.0.0", "1.0.0.0",     "1");
    ok_addr(1, "129.0.0.0", "1.0.0.0",     "0");
    ok_addr(1, "129.0.0.0", "1.0.0.0",     "0.0.0.0");

    /* Try some IPv6 addresses. */
#ifdef HAVE_INET6
    ok_addr(1, ipv6_addr,   ipv6_addr,     NULL);
    ok_addr(1, ipv6_addr,   ipv6_addr,     "128");
    ok_addr(1, ipv6_addr,   ipv6_addr,     "60");
    ok_addr(1, "::127",     "0:0::127",    "128");
    ok_addr(1, "::127",     "0:0::128",    "120");
    ok_addr(0, "::127",     "0:0::128",    "128");
    ok_addr(0, "::7fff",    "0:0::8000",   "113");
    ok_addr(1, "::7fff",    "0:0::8000",   "112");
    ok_addr(0, "::3:ffff",  "::2:ffff",    "120");
    ok_addr(0, "::3:ffff",  "::2:ffff",    "119");
    ok_addr(0, "ffff::1",   "7fff::1",     "1");
    ok_addr(1, "ffff::1",   "7fff::1",     "0");
    ok_addr(0, "fffg::1",   "fffg::1",     NULL);
    ok_addr(0, "ffff::1",   "7fff::1",     "-1");
    ok_addr(0, "ffff::1",   "ffff::1",     "-1");
    ok_addr(0, "ffff::1",   "ffff::1",     "129");
#else
    skip_block(16, "IPv6 not supported");
#endif

    /* Test some invalid addresses. */
    ok_addr(0, "fred",      "fred",        NULL);
    ok_addr(0, "",          "",            NULL);
    ok_addr(0, "",          "",            "0");
    ok_addr(0, "127.0.0.1", "127.0.0.1",   "pete");
    ok_addr(0, "127.0.0.1", "127.0.0.1",   "1p");
    ok_addr(0, "127.0.0.1", "127.0.0.1",   "1p");
    ok_addr(0, "127.0.0.1", "127.0.0.1",   "-1");
    ok_addr(0, "127.0.0.1", "127.0.0.1",   "33");

    return 0;
}
