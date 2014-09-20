/* $Id$
 *
 * Test suite for network client and read/write functions.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2005, 2013, 2014 Russ Allbery <eagle@eyrie.org>
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
#include <sys/wait.h>
#include <signal.h>

#include "tap/basic.h"
#include "inn/macros.h"
#include "inn/messages.h"
#include "inn/network.h"


/*
 * A client writer to test network_client_create.  Connects to IPv4 localhost,
 * and expects to always succeed on the connection, taking the source address
 * to pass into network_client_create.
 */
static void
client_create_writer(const char *source)
{
    socket_type fd;
    struct sockaddr_in sin;
    FILE *out;

    fd = network_client_create(PF_INET, SOCK_STREAM, source);
    if (fd == INVALID_SOCKET)
        _exit(1);
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(11119);
    sin.sin_addr.s_addr = htonl(0x7f000001UL);
    if (connect(fd, (struct sockaddr *) &sin, sizeof(sin)) < 0)
        _exit(1);
    out = fdopen(fd, "w");
    if (out == NULL)
        _exit(1);
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(0);
}


/*
 * Used to test network_read.  Connects, sends a couple of strings, then
 * sleeps for 10 seconds before sending another string so that timeouts can be
 * tested.  Meant to be run in a child process.
 */
static void
client_delay_writer(const char *host)
{
    socket_type fd;

    fd = network_connect_host(host, 11119, NULL, 0);
    if (fd == INVALID_SOCKET)
        _exit(1);
    if (socket_write(fd, "one\n", 4) != 4)
        _exit(1);
    if (socket_write(fd, "two\n", 4) != 4)
        _exit(1);
    sleep(10);
    if (socket_write(fd, "three\n", 6) != 6)
        _exit(1);
    _exit(0);
}


/*
 * Used to test network_write.  Connects, reads 64KB from the network, then
 * sleeps before reading another 64KB.  Meant to be run in a child process.
 */
static void
client_delay_reader(const char *host)
{
    char *buffer;
    socket_type fd;

    fd = network_connect_host(host, 11119, NULL, 0);
    if (fd == INVALID_SOCKET)
        _exit(1);
    buffer = malloc(64 * 1024);
    if (buffer == NULL)
        _exit(1);
    if (!network_read(fd, buffer, 64 * 1024, 0))
        _exit(1);
    sleep(10);
    if (!network_read(fd, buffer, 64 * 1024, 0))
        _exit(1);
    free(buffer);
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
 * Bring up a server on port 11119 on the loopback address and test connecting
 * to it via IPv4 using network_client_create.  Takes an optional source
 * address to use for client connections.
 */
static void
test_create_ipv4(const char *source)
{
    socket_type fd;
    pid_t child;
    int status;

    /* Create the socket and listen to it. */
    fd = network_bind_ipv4(SOCK_STREAM, "127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    ok(fd != INVALID_SOCKET, "IPv4 network client");
    if (listen(fd, 1) < 0)
        sysbail("cannot listen to socket");

    /* Fork off a child that uses network_client_create. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0)
        client_create_writer(source);
    else {
        test_server_accept(fd);
        waitpid(child, &status, 0);
        is_int(0, status, "client made correct connections");
    }
}


/*
 * Test connect timeouts using IPv4.  Bring up a server on port 11119 on the
 * loopback address and test connections to it.  The server only accepts one
 * connection at a time, so a subsequent connection will time out.
 */
static void
test_timeout_ipv4(void)
{
    socket_type fd, c;
    pid_t child;
    socket_type block[20];
    int i, err;

    /*
     * Create the listening socket.  We set the listening queue size to 1,
     * but some operating systems, including Linux, will allow more
     * connection attempts to succeed than the backlog size.  We'll therefore
     * have to hammer this server with connections to try to get it to fail.
     */
    fd = network_bind_ipv4(SOCK_STREAM, "127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    if (listen(fd, 1) < 0)
        sysbail("cannot listen to socket");

    /* Fork off a child that just runs accept once and then sleeps. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        alarm(10);
        c = accept(fd, NULL, 0);
        if (c == INVALID_SOCKET)
            _exit(1);
        sleep(9);
        _exit(0);
    }

    /* In the parent.  Open that first connection. */
    socket_close(fd);
    c = network_connect_host("127.0.0.1", 11119, NULL, 1);
    ok(c != INVALID_SOCKET, "Timeout: first connection worked");

    /*
     * It can take up to fifteen connections on Linux before connections start
     * actually timing out, and sometimes they never do.
     */
    alarm(20);
    for (i = 0; i < (int) ARRAY_SIZE(block); i++) {
        block[i] = network_connect_host("127.0.0.1", 11119, NULL, 1);
        if (block[i] == INVALID_SOCKET)
            break;
    }
    err = socket_errno;

    /*
     * If we reached the end of the array, we can't force a connection
     * timeout, so just skip this test.  It's also possible that the
     * connection will fail with ECONNRESET or ECONNREFUSED if the nine second
     * sleep in the child passed, so skip in that case as well.  Otherwise,
     * expect a failure due to timeout in a reasonable amount of time (less
     * than our 20-second alarm).
     */
    if (i == ARRAY_SIZE(block))
        skip_block(2, "short listen queue does not prevent connections");
    else {
        diag("Finally timed out on socket %d", i);
        ok(block[i] == INVALID_SOCKET, "Later connection timed out");
        if (err == ECONNRESET || err == ECONNREFUSED)
            skip("connections rejected without timeout");
        else
            is_int(ETIMEDOUT, err, "...with correct error code");
    }
    alarm(0);

    /* Shut down the client and clean up resources. */
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);
    socket_close(c);
    for (i--; i >= 0; i--)
        if (block[i] != INVALID_SOCKET)
            socket_close(block[i]);
    socket_close(fd);
}


/*
 * Test the network read function with a timeout.  We fork off a child process
 * that runs delay_writer, and then we read from the network twice, once with
 * a timeout and once without, and then try a third time when we should time
 * out.
 */
static void
test_network_read(void)
{
    socket_type fd, c;
    pid_t child;
    struct sockaddr_in sin;
    socklen_t slen;
    char buffer[4];

    /* Create the listening socket. */
    fd = network_bind_ipv4(SOCK_STREAM, "127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    if (listen(fd, 1) < 0)
        sysbail("cannot listen to socket");

    /* Fork off a child process that writes some data with delays. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        socket_close(fd);
        client_delay_writer("127.0.0.1");
    }

    /* Set an alarm just in case our timeouts don't work. */
    alarm(10);

    /* Accept the client connection. */
    slen = sizeof(sin);
    c = accept(fd, &sin, &slen);
    if (c == INVALID_SOCKET)
        sysbail("cannot accept on socket");

    /* Now test a couple of simple reads, with and without timeout. */
    socket_set_errno(0);
    ok(network_read(c, buffer, sizeof(buffer), 0), "network_read");
    ok(memcmp("one\n", buffer, sizeof(buffer)) == 0, "...with good data");
    ok(network_read(c, buffer, sizeof(buffer), 1),
       "network_read with timeout");
    ok(memcmp("two\n", buffer, sizeof(buffer)) == 0, "...with good data");

    /*
     * The third read should abort with a timeout, since the writer is writing
     * with a ten second delay.
     */
    ok(!network_read(c, buffer, sizeof(buffer), 1),
       "network_read aborted with timeout");
    is_int(ETIMEDOUT, socket_errno, "...with correct error");
    ok(memcmp("two\n", buffer, sizeof(buffer)) == 0, "...and data unchanged");
    alarm(0);

    /* Clean up. */
    socket_close(c);
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);
    socket_close(fd);
}


/*
 * Test the network write function with a timeout.  We fork off a child
 * process that runs delay_reader, and then we write 64KB to the network in
 * two chunks, once with a timeout and once without, and then try a third time
 * when we should time out.
 */
static void
test_network_write(void)
{
    socket_type fd, c;
    pid_t child;
    struct sockaddr_in sin;
    socklen_t slen;
    char *buffer;

    /* Create the data that we're going to send. */
    buffer = bmalloc(8192 * 1024);
    memset(buffer, 'a', 8192 * 1024);

    /* Create the listening socket. */
    fd = network_bind_ipv4(SOCK_STREAM, "127.0.0.1", 11119);
    if (fd == INVALID_SOCKET)
        sysbail("cannot create or bind socket");
    if (listen(fd, 1) < 0)
        sysbail("cannot listen to socket");

    /* Create the child, which will connect and then read data with delay. */
    child = fork();
    if (child < 0)
        sysbail("cannot fork");
    else if (child == 0) {
        socket_close(fd);
        client_delay_reader("127.0.0.1");
    }

    /* Set an alarm just in case our timeouts don't work. */
    alarm(10);

    /* Accept the client connection. */
    slen = sizeof(struct sockaddr_in);
    c = accept(fd, &sin, &slen);
    if (c == INVALID_SOCKET)
        sysbail("cannot accept on socket");

    /* Test some successful writes with and without a timeout. */
    socket_set_errno(0);
    ok(network_write(c, buffer, 32 * 1024, 0), "network_write");
    ok(network_write(c, buffer, 32 * 1024, 1),
       "network_write with timeout");

    /*
     * A longer write cannot be completely absorbed before the client sleep,
     * so should fail with a timeout.
     */
    ok(!network_write(c, buffer, 8192 * 1024, 1),
       "network_write aborted with timeout");
    is_int(ETIMEDOUT, socket_errno, "...with correct error");
    alarm(0);

    /* Clean up. */
    socket_close(c);
    kill(child, SIGTERM);
    waitpid(child, NULL, 0);
    socket_close(fd);
    free(buffer);
}


int
main(void)
{
    /* Set up the plan. */
    plan(22);

    /* Test network_client_create. */
    test_create_ipv4(NULL);
    test_create_ipv4("127.0.0.1");

    /* Test network_connect with a timeout. */
    test_timeout_ipv4();

    /* Test network_read and network_write. */
    test_network_read();
    test_network_write();
    return 0;
}
