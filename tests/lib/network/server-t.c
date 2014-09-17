/* $Id$
 *
 * Test suite for network server functions.
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

#include <errno.h>
#include "portable/wait.h"
#include <signal.h>

#include "tap/basic.h"
#include "inn/macros.h"
#include "inn/messages.h"
#include "inn/network.h"


/*
 * Check whether IPv6 actually works.  On some systems (such as Solaris 8
 * without IPv6 configured), it's possible to create an IPv6 socket, but
 * binding the socket will fail.  We therefore attempt an IPv6 socket creation
 * and, if it fails, check errno for several errors that indicate that IPv6
 * is supported but doesn't work.  This will also handle the case where IPv6
 * support is not configured, since network_bind_ipv6 will return
 * INVALID_SOCKET.
 */
static bool
ipv6_works(void)
{
    socket_type fd;

    /* Create the socket.  If this works, ipv6 is supported. */
    fd = network_bind_ipv6(SOCK_STREAM, "::1", 11119);
    if (fd != INVALID_SOCKET) {
        close(fd);
        return true;
    }

    /* IPv6 not recognized, indicating no support. */
    if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT)
        return false;

    /* IPv6 is recognized but we can't actually use it. */
    if (errno == EADDRNOTAVAIL)
        return false;

    /*
     * Some other error.  Assume it's not related to IPv6.  We'll probably
     * fail later.
     */
    return true;
}


/*
 * A client writer used to generate data for a server test.  Connect to the
 * given host on port 11119 and send a constant string to a socket.  Takes the
 * source address as well to pass into network_connect_host.  If the flag is
 * true, expects to succeed in connecting; otherwise, fail the test (by
 * exiting with a non-zero status) if the connection is successful.
 *
 * If the succeed argument is true, this is guarateed to never return.
 */
static void
client_writer(const char *host, const char *source, bool succeed)
{
    socket_type fd;
    FILE *out;

    fd = network_connect_host(host, 11119, source, 0);
    if (fd == INVALID_SOCKET) {
        if (succeed)
            _exit(1);
        else
            return;
    }
    out = fdopen(fd, "w");
    if (out == NULL)
        sysdie("fdopen failed");
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(succeed ? 0 : 1);
}


/*
 * A client writer for testing UDP.  Sends a UDP packet to port 11119 on
 * localhost, from the given source address, containing a constant string.
 * This also verifies that network_client_create works properly.
 */
static void
client_udp_writer(const char *source)
{
    socket_type fd;
    struct sockaddr_in sin;

    /* Create and bind the socket. */
    fd = network_client_create(PF_INET, SOCK_DGRAM, source);
    if (fd == INVALID_SOCKET)
        _exit(1);

    /* Connect to localhost port 11119. */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(11119);
    sin.sin_addr.s_addr = htonl(0x7f000001UL);
    if (connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
        _exit(1);

    /* Send our fixed UDP packet. */
    if (send(fd, "socket test\r\n", 13, 0) < 13)
        _exit(1);
    _exit(0);
}


/*
 * When testing the bind (server) functions, we create listening sockets, fork
 * a child process to connect to it, and accept the connection and read the
 * data in the server.  The test reporting is therefore done by the listener.
 * There are two listeners, depending on whether we're listening to a single
 * socket or an array of sockets, both of which invoke this handler when the
 * connection is accepted.
 *
 * Check that the result of accept, read data from the client, and ensure we
 * got the expected data, reporting all results through the normal test
 * reporting mechanism.
 */
static void
test_server_connection(socket_type client)
{
    FILE *out;
    char buffer[512];

    /* Verify that the result of accept is good. */
    if (client == INVALID_SOCKET) {
        sysdiag("cannot accept connection from socket");
        ok_block(2, 0, "...socket read test");
        return;
    }
    ok(1, "...socket accept");

    /* Read data from the client and ensure it matches our expectations. */
    out = fdopen(client, "r");
    if (fgets(buffer, sizeof(buffer), out) == NULL) {
        sysdiag("cannot read from socket");
        ok(0, "...socket read");
    }
    is_string("socket test\r\n", buffer, "...socket read");
    fclose(out);
}


/*
 * Test a single listening socket.  Accepts one connection and invokes
 * test_server_connection.  For skipping purposes, this produces two tests.
 */
static void
test_server_accept(socket_type fd)
{
    socket_type client;

    client = accept(fd, NULL, NULL);
    test_server_connection(client);
    socket_close(fd);
}


/*
 * A varient version of the server portion of the test.  Takes an array of
 * sockets and the size of the sockets and accepts a connection on any of
 * those sockets.  Ensures that the client address information is stored
 * correctly by checking that it is set to IPv4 localhost.  For skipping
 * purposes, this produces four tests.
 *
 * saddr is allocated from the heap instead of using a local struct
 * sockaddr_storage to work around a misdiagnosis of strict aliasing
 * violations from gcc 4.4 (fixed in later versions).
 */
static void
test_server_accept_any(socket_type fds[], unsigned int count)
{
    socket_type client;
    unsigned int i;
    struct sockaddr *saddr;
    socklen_t slen;

    slen = sizeof(struct sockaddr_storage);
    saddr = bcalloc(1, slen);
    client = network_accept_any(fds, count, saddr, &slen);
    test_server_connection(client);
    is_int(AF_INET, saddr->sa_family, "...address family is IPv4");
    is_int(htonl(0x7f000001UL),
           ((struct sockaddr_in *) (void *) saddr)->sin_addr.s_addr,
           "...and client address is 127.0.0.1");
    free(saddr);
    for (i = 0; i < count; i++)
        socket_close(fds[i]);
}


/*
 * Bring up a server on port 11119 on the loopback address and test connecting
 * to it via IPv4.  Takes an optional source address to use for client
 * connections.  For skipping purposes, this produces four tests.
 */
static void
test_ipv4(const char *source)
{
    socket_type fd;
    pid_t child;
    int status;

    /* Set up the server socket. */
    fd = network_bind_ipv4(SOCK_STREAM, "127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    ok(fd != INVALID_SOCKET, "IPv4 server test");
    if (listen(fd, 1) < 0)
        sysbail("cannot listen to socket");

    /* Fork off a child writer and test the server accept. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        socket_close(fd);
        client_writer("127.0.0.1", source, true);
    } else {
        test_server_accept(fd);
        waitpid(child, &status, 0);
        is_int(0, status, "client made correct connections");
    }
}


/*
 * Bring up a server on port 11119 on the loopback address and test connecting
 * to it via IPv6.  Takes an optional source address to use for client
 * connections.  For skipping purposes, this produces four tests.
 */
static void
test_ipv6(const char *source)
{
    socket_type fd;
    pid_t child;
    int status;

    /* Set up the server socket. */
    fd = network_bind_ipv6(SOCK_STREAM, "::1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create socket");
    ok(fd != INVALID_SOCKET, "IPv6 server test");
    if (listen(fd, 1) < 0)
        sysbail("cannot listen to socket");

    /*
     * Fork off a child writer and test the server accept.  If IPV6_V6ONLY is
     * supported, we can also check that connecting to 127.0.0.1 will fail.
     */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        socket_close(fd);
#ifdef IPV6_V6ONLY
        client_writer("127.0.0.1", NULL, false);
#endif
        client_writer("::1", source, true);
    } else {
        test_server_accept(fd);
        waitpid(child, &status, 0);
        is_int(0, status, "client made correct connections");
    }
}


/*
 * Returns the struct sockaddr * corresponding to a local socket.  Handles the
 * initial allocation being too small and dynamically increasing it.  Caller
 * is responsible for freeing the allocated sockaddr.
 */
static struct sockaddr *
get_sockaddr(socket_type fd)
{
    struct sockaddr *saddr;
    socklen_t size;

    saddr = bmalloc(sizeof(struct sockaddr_storage));
    size = sizeof(struct sockaddr_storage);
    if (getsockname(fd, saddr, &size) < 0)
        sysbail("cannot getsockname");
    if (size > sizeof(struct sockaddr)) {
        free(saddr);
        saddr = bmalloc(size);
        if (getsockname(fd, saddr, &size) < 0)
            sysbail("cannot getsockname");
    }
    return saddr;
}


/*
 * Bring up a server on port 11119 on all addresses and try connecting to it
 * via all of the available protocols.  Takes an optional source address to
 * use for client connections.  For skipping purposes, this produces eight
 * tests.
 */
static void
test_all(const char *source_ipv4, const char *source_ipv6 UNUSED)
{
    socket_type *fds, fd;
    unsigned int count, i;
    pid_t child;
    struct sockaddr *saddr;
    int status;

    /* Bind sockets for all available local addresses. */
    if (!network_bind_all(SOCK_STREAM, 11119, &fds, &count))
        sysbail("cannot create or bind socket");

    /*
     * There should be at most two, one for IPv4 and one for IPv6, but allow
     * for possible future weirdness in networking.
     */
    if (count > 2) {
        diag("got more than two sockets, using just the first two");
        count = 2;
    }

    /* We'll test each socket in turn by listening and trying to connect. */
    for (i = 0; i < count; i++) {
        fd = fds[i];
        if (listen(fd, 1) < 0)
            sysbail("cannot listen to socket %d", fd);
        ok(fd != INVALID_SOCKET, "all address server test (part %d)", i + 1);

        /* Get the socket type to determine what type of client to run. */
        saddr = get_sockaddr(fd);

        /*
         * Fork off a child writer and test the server accept.  If IPV6_V6ONLY
         * is supported, we can also check that connecting to 127.0.0.1 will
         * fail.
         */
        child = fork();
        if (child < 0)
            sysbail("cannot fork");
        else if (child == 0) {
            if (saddr->sa_family == AF_INET) {
                client_writer("::1", source_ipv6, false);
                client_writer("127.0.0.1", source_ipv4, true);
#ifdef HAVE_INET6
            } else if (saddr->sa_family == AF_INET6) {
# ifdef IPV6_V6ONLY
                client_writer("127.0.0.1", source_ipv4, false);
# endif
                client_writer("::1", source_ipv6, true);
#endif
            }
        } else {
            test_server_accept(fd);
            waitpid(child, &status, 0);
            is_int(0, status, "client made correct connections");
        }
        free(saddr);
    }
    network_bind_all_free(fds);

    /* If we only got one listening socket, skip for consistent test count. */
    if (count == 1)
        skip_block(4, "only one listening socket");
}


/*
 * Bring up a server on port 11119 on all addresses and try connecting to it
 * via 127.0.0.1, using network_accept_any underneath.  For skipping purposes,
 * this runs three tests.
 */
static void
test_any(void)
{
    socket_type *fds;
    unsigned int count, i;
    pid_t child;
    int status;

    if (!network_bind_all(SOCK_STREAM, 11119, &fds, &count))
        sysbail("cannot create or bind socket");
    for (i = 0; i < count; i++)
        if (listen(fds[i], 1) < 0)
            sysbail("cannot listen to socket %d", fds[i]);
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0)
        client_writer("127.0.0.1", NULL, true);
    else {
        test_server_accept_any(fds, count);
        waitpid(child, &status, 0);
        is_int(0, status, "client made correct connections");
    }
    network_bind_all_free(fds);
}


/*
 * Bring up a UDP server on port 11119 on all addresses and try connecting to
 * it via 127.0.0.1, using network_wait_any underneath.  This tests the bind
 * functions for UDP sockets, network_client_create for UDP addresses, and
 * network_wait_any.
 */
static void
test_any_udp(void)
{
    socket_type *fds, fd;
    unsigned int count, i;
    pid_t child;
    char buffer[BUFSIZ];
    ssize_t length;
    int status;
    struct sockaddr_storage addr;
    struct sockaddr *saddr;
    struct sockaddr_in sin;
    socklen_t addrlen;

    /* Bind our UDP socket. */
    if (!network_bind_all(SOCK_DGRAM, 11119, &fds, &count))
        sysbail("cannot create or bind socket");

    /* Create a child that writes a single UDP packet to the server. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0)
        client_udp_writer("127.0.0.1");

    /* Set an alarm, since if the client malfunctions, nothing happens. */
    alarm(5);

    /* Wait for the UDP packet and then read and confirm it. */
    fd = network_wait_any(fds, count);
    ok(fd != INVALID_SOCKET, "network_wait_any found UDP message");
    if (fd == INVALID_SOCKET)
        ok_block(3, false, "could not accept client");
    else {
        saddr = (struct sockaddr *) &addr;
        addrlen = sizeof(addr);
        length = recvfrom(fd, buffer, sizeof(buffer), 0, saddr, &addrlen);
        is_int(13, length, "...of correct length");
        sin.sin_family = AF_INET;
        sin.sin_port = htons(11119);
        sin.sin_addr.s_addr = htonl(0x7f000001UL);
        ok(network_sockaddr_equal((struct sockaddr *) &sin, saddr),
           "...from correct address");
        buffer[13] = '\0';
        is_string("socket test\r\n", buffer, "...and correct contents");
    }

    /* Wait for the child and be sure it exited successfully. */
    waitpid(child, &status, 0);
    is_int(0, status, "client made correct connections");

    /* Clean up. */
    for (i = 0; i < count; i++)
        socket_close(fds[i]);
    network_bind_all_free(fds);
}


int
main(void)
{
    /* Set up the plan. */
    plan(42);

    /* Test network_bind functions. */
    test_ipv4(NULL);
    test_ipv4("127.0.0.1");

    /*
     * Optionally test IPv6 support.  If IPv6 support appears to be available
     * but doesn't work, we have to explicitly skip test_all, since it will
     * create a socket that we then can't connect to.
     */
    if (ipv6_works()) {
        test_ipv6(NULL);
        test_ipv6("::1");
        test_all(NULL, NULL);
        test_all("127.0.0.1", "::1");
    } else {
        skip_block(24, "IPv6 not configured");
    }

    /* Test network_accept_any. */
    test_any();

    /* Test UDP socket handling and network_wait_any. */
    test_any_udp();
    return 0;
}
