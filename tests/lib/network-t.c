/* $Id$ */
/* network test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "libinn.h"
#include "libtest.h"

/* The server portion of the test.  Listens to a socket and accepts a
   connection, making sure what is printed on that connection matches what the
   client is supposed to print. */
static int
listener(int fd, int n)
{
    int client;
    FILE *out;
    char buffer[512];

    alarm(1);
    client = accept(fd, NULL, NULL);
    close(fd);
    if (client < 0) {
        syswarn("cannot accept connection from socket");
        ok_block(n, 2, false);
        return n + 2;
    }
    ok(n++, true);
    out = fdopen(client, "r");
    if (fgets(buffer, sizeof(buffer), out) == NULL) {
        syswarn("cannot read from socket");
        ok(n++, false);
        return n;
    }
    ok_string(n++, "socket test\r\n", buffer);
    fclose(out);
    return n;
}

/* Send a constant string to a socket, used to finish the client side of the
   testing. */
static void
client_send(int fd)
{
    FILE *out;

    out = fdopen(fd, "w");
    if (out == NULL)
        _exit(1);
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(0);
}

/* Create a client IPv4 connection to the local server and then send a static
   string to that connection. */
static void
client_ipv4(void)
{
    struct addrinfo hints, *ai;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("127.0.0.1", "11119", &hints, &ai) != 0)
        _exit(1);
    fd = network_connect(ai);
    freeaddrinfo(ai);
    if (fd < 0)
        _exit(1);
    client_send(fd);
}

/* Create a client IPv6 connection to the local server and then send a static
   string to that connection. */
#ifdef HAVE_INET6
static void
client_ipv6(void)
{
    struct addrinfo hints, *ai;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo("::1", "11119", &hints, &ai) != 0)
        _exit(1);
    fd = network_connect(ai);
    freeaddrinfo(ai);
    if (fd < 0)
        _exit(1);
    client_send(fd);
}
#endif

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv4. */
static int
test_ipv4(int n)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv4("127.0.0.1", 11119);
    if (fd < 0)
        sysdie("cannot create or bind socket");
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok(n++, false);
        ok(n++, false);
        ok(n++, false);
    } else {
        ok(n++, true);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            client_ipv4();
        else
            n = listener(fd, n);
    }
    return n;
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv6. */
#ifdef HAVE_INET6
static int
test_ipv6(int n)
{
    int fd;
    pid_t child;

    fd = network_bind_ipv6("::1", 11119);
    if (fd < 0) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT) {
            skip_block(n, 3, "IPv6 not supported");
            return n + 3;
        } else
            sysdie("cannot create socket");
    }
    if (listen(fd, 1) < 0) {
        syswarn("cannot listen to socket");
        ok_block(n, 3, false);
        n += 3;
    } else {
        ok(n++, true);
        child = fork();
        if (child < 0)
            sysdie("cannot fork");
        else if (child == 0)
            client_ipv6();
        else
            n = listener(fd, n);
    }
    return n;
}
#else /* !HAVE_INET6 */
static int
test_ipv6(int n)
{
    int i;

    skip_block(n, 3, "IPv6 not supported");
    return n + 3;
}
#endif /* !HAVE_INET6 */

/* Bring up a server on port 11119 on all addresses and try connecting to it
   via all of the available protocols. */
static int
test_all(int n)
{
    int *fds, count, fd, i;
    pid_t child;
    struct sockaddr_storage saddr;
    size_t size = sizeof(saddr);

    network_bind_all(11119, &fds, &count);
    if (count == 0)
        sysdie("cannot create or bind socket");
    if (count > 2) {
        warn("got more than two sockets, using just the first two");
        count = 2;
    }
    for (i = 0; i < count; i++) {
        fd = fds[i];
        if (listen(fd, 1) < 0) {
            syswarn("cannot listen to socket %d", fd);
            ok_block(n, 3, false);
            n += 3;
        } else {
            ok(n++, true);
            child = fork();
            if (child < 0)
                sysdie("cannot fork");
            else if (child == 0) {
                if (getsockname(fd, (struct sockaddr *) &saddr, &size) < 0)
                    sysdie("cannot getsockname");
                if (saddr.ss_family == AF_INET)
                    client_ipv4();
                else if (saddr.ss_family == AF_INET6)
                    client_ipv6();
                else {
                    warn("unknown socket family %d", saddr.ss_family);
                    skip_block(n, 2, "unknown socket family");
                    n += 2;
                }
                size = sizeof(saddr);
            } else
                n = listener(fd, n);
        }
    }
    if (count == 1) {
        skip_block(n, 3, "only one listening socket");
        n += 3;
    }
    return n;
}

int
main(void)
{
    int n;

    /* We need an initialized innconf struct, but it doesn't need to contain
       anything interesting. */
    innconf = xcalloc(1, sizeof(struct innconf));

    test_init(36);

    n = test_ipv4(1);           /* Tests  1-3.  */
    n = test_ipv6(n);           /* Tests  4-6.  */
    n = test_all(n);            /* Tests  7-12. */

    /* This should be equivalent to the previous tests. */
    innconf->sourceaddress = xstrdup("all");
    innconf->sourceaddress6 = xstrdup("all");
    n = test_ipv4(n);           /* Tests 13-15. */
    n = test_ipv6(n);           /* Tests 16-18. */
    n = test_all(n);            /* Tests 19-24. */

    /* This won't make a difference for loopback connections, but it will
       exercise the code in lib/network.c. */
    free(innconf->sourceaddress);
    free(innconf->sourceaddress6);
    innconf->sourceaddress = xstrdup("127.0.0.1");
    innconf->sourceaddress6 = xstrdup("::1");
    n = test_ipv4(n);           /* Tests 25-27. */
    n = test_ipv6(n);           /* Tests 28-30. */
    n = test_all(n);            /* Tests 31-36. */

    return 0;
}
