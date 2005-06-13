/* $Id$ */
/* network test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <ctype.h>
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

/* Connect to the given host on port 11119 and send a constant string to a
   socket, used to do the client side of the testing.  Takes the source
   address as well to pass into network_connect_host. */
static void
client(const char *host, const char *source)
{
    int fd;
    FILE *out;

    fd = network_connect_host(host, 11119, source);
    if (fd < 0)
        _exit(1);
    out = fdopen(fd, "w");
    if (out == NULL)
        _exit(1);
    fputs("socket test\r\n", out);
    fclose(out);
    _exit(0);
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv4.  Takes an optional source address to use for client
   connections. */
static int
test_ipv4(int n, const char *source)
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
            client("127.0.0.1", source);
        else
            n = listener(fd, n);
    }
    return n;
}

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv6.  Takes an optional source address to use for client
   connections. */
#ifdef HAVE_INET6
static int
test_ipv6(int n, const char *source)
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
            client("::1", source);
        else
            n = listener(fd, n);
    }
    return n;
}
#else /* !HAVE_INET6 */
static int
test_ipv6(int n, const char *source UNUSED)
{
    skip_block(n, 3, "IPv6 not supported");
    return n + 3;
}
#endif /* !HAVE_INET6 */

/* Bring up a server on port 11119 on all addresses and try connecting to it
   via all of the available protocols.  Takes an optional source address to
   use for client connections. */
static int
test_all(int n, const char *source_ipv4, const char *source_ipv6)
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
                    client("127.0.0.1", source_ipv4);
                else if (saddr.ss_family == AF_INET6)
                    client("::1", source_ipv6);
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

/* Bring up a server on port 11119 on the loopback address and test connecting
   to it via IPv4 using network_connect_sockaddr.  Takes an optional source
   address to use for client connections. */
static int
test_create_ipv4(int n, const char *source)
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
        else if (child == 0) {
            struct sockaddr_in sin;
            FILE *out;

            fd = network_client_create(PF_INET, SOCK_STREAM, source);
            if (fd < 0)
                _exit(1);
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
        } else
            n = listener(fd, n);
    }
    return n;
}

/* Tests network_addr_compare.  Takes the test number, the expected result,
   the two addresses, and the mask. */
static void
ok_addr(int n, bool expected, const char *a, const char *b, const char *mask)
{
    if (expected)
        ok(n, network_addr_match(a, b, mask));
    else
        ok(n, !network_addr_match(a, b, mask));
}

int
main(void)
{
    int n, status;
    struct addrinfo *ai, *ai4, *ai6;
    struct addrinfo hints;
    char addr[INET6_ADDRSTRLEN];
    char *p;
    static const char *port = "119";
    static const char *ipv6_addr = "FEDC:BA98:7654:3210:FEDC:BA98:7654:3210";

    test_init(117);

    n = test_ipv4(1, NULL);                     /* Tests  1-3.  */
    n = test_ipv6(n, NULL);                     /* Tests  4-6.  */
    n = test_all(n, NULL, NULL);                /* Tests  7-12. */
    n = test_create_ipv4(n, NULL);              /* Tests 13-15. */

    /* This won't make a difference for loopback connections. */
    n = test_ipv4(n, "127.0.0.1");              /* Tests 16-18. */
    n = test_ipv6(n, "::1");                    /* Tests 19-21. */
    n = test_all(n, "127.0.0.1", "::1");        /* Tests 22-27. */
    n = test_create_ipv4(n, "127.0.0.1");       /* Tests 28-30. */

    /* We need an initialized innconf struct, but it doesn't need to contain
       anything interesting. */
    innconf = xcalloc(1, sizeof(struct innconf));

    /* This should be equivalent to the previous tests. */
    innconf->sourceaddress = xstrdup("all");
    innconf->sourceaddress6 = xstrdup("all");
    n = test_ipv4(n, NULL);                     /* Tests 31-33. */
    n = test_ipv6(n, NULL);                     /* Tests 34-36. */
    n = test_all(n, NULL, NULL);                /* Tests 37-42. */
    n = test_create_ipv4(n, NULL);              /* Tests 43-45. */

    /* This won't make a difference for loopback connections. */
    free(innconf->sourceaddress);
    free(innconf->sourceaddress6);
    innconf->sourceaddress = xstrdup("127.0.0.1");
    innconf->sourceaddress6 = xstrdup("::1");
    n = test_ipv4(n, NULL);                     /* Tests 46-48. */
    n = test_ipv6(n, NULL);                     /* Tests 49-51. */
    n = test_all(n, NULL, NULL);                /* Tests 52-57. */
    n = test_create_ipv4(n, NULL);              /* Tests 58-60. */

    /* Now, test network_sockaddr_sprint, network_sockaddr_equal, and
       network_sockaddr_port. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICHOST;
    hints.ai_socktype = SOCK_STREAM;
    status = getaddrinfo("127.0.0.1", port, &hints, &ai4);
    if (status != 0)
        die("getaddrinfo on 127.0.0.1 failed: %s", gai_strerror(status));
    ok(61, network_sockaddr_sprint(addr, sizeof(addr), ai4->ai_addr));
    ok_string(62, "127.0.0.1", addr);
    ok_int(63, 119, network_sockaddr_port(ai4->ai_addr));
    ok(64, network_sockaddr_equal(ai4->ai_addr, ai4->ai_addr));
    status = getaddrinfo("127.0.0.2", NULL, &hints, &ai);
    if (status != 0)
        die("getaddrinfo on 127.0.0.2 failed: %s", gai_strerror(status));
    ok(65, !network_sockaddr_equal(ai->ai_addr, ai4->ai_addr));
    ok(66, !network_sockaddr_equal(ai4->ai_addr, ai->ai_addr));

    /* The same for IPv6. */
#ifdef HAVE_INET6
    status = getaddrinfo(ipv6_addr, port, &hints, &ai6);
    if (status != 0)
        sysdie("getaddr on %s failed", ipv6_addr);
    ok(67, network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr));
    for (p = addr; *p != '\0'; p++)
        if (islower((unsigned char) *p))
            *p = toupper((unsigned char) *p);
    ok_string(68, ipv6_addr, addr);
    ok_int(69, 119, network_sockaddr_port(ai6->ai_addr));
    ok(70, network_sockaddr_equal(ai6->ai_addr, ai6->ai_addr));
    ok(71, !network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr));
    ok(72, !network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr));

    /* Test IPv4 mapped addresses. */
    status = getaddrinfo("::ffff:7f00:1", NULL, &hints, &ai6);
    if (status != 0)
        sysdie("getaddr on ::ffff:7f00:1 failed");
    ok(73, network_sockaddr_sprint(addr, sizeof(addr), ai6->ai_addr));
    ok_string(74, "127.0.0.1", addr);
    ok(75, network_sockaddr_equal(ai4->ai_addr, ai6->ai_addr));
    ok(76, network_sockaddr_equal(ai6->ai_addr, ai4->ai_addr));
    ok(77, !network_sockaddr_equal(ai->ai_addr, ai6->ai_addr));
    ok(78, !network_sockaddr_equal(ai6->ai_addr, ai->ai_addr));
    freeaddrinfo(ai6);
#else
    skip_block(67, 12, "IPv6 not supported");
#endif

    /* Check the domains of functions and their error handling. */
    ai4->ai_addr->sa_family = AF_UNIX;
    ok(79, !network_sockaddr_equal(ai4->ai_addr, ai4->ai_addr));
    ok_int(80, 0, network_sockaddr_port(ai4->ai_addr));

    /* Tests for network_addr_compare. */
    ok_addr( 81, true,  "127.0.0.1", "127.0.0.1",   NULL);
    ok_addr( 82, false, "127.0.0.1", "127.0.0.2",   NULL);
    ok_addr( 83, true,  "127.0.0.1", "127.0.0.0",   "31");
    ok_addr( 84, false, "127.0.0.1", "127.0.0.0",   "32");
    ok_addr( 85, false, "127.0.0.1", "127.0.0.0",   "255.255.255.255");
    ok_addr( 86, true,  "127.0.0.1", "127.0.0.0",   "255.255.255.254");
    ok_addr( 87, true,  "10.10.4.5", "10.10.4.255", "24");
    ok_addr( 88, false, "10.10.4.5", "10.10.4.255", "25");
    ok_addr( 89, true,  "10.10.4.5", "10.10.4.255", "255.255.255.0");
    ok_addr( 90, false, "10.10.4.5", "10.10.4.255", "255.255.255.128");
    ok_addr( 91, false, "129.0.0.0", "1.0.0.0",     "1");
    ok_addr( 92, true,  "129.0.0.0", "1.0.0.0",     "0");
    ok_addr( 93, true,  "129.0.0.0", "1.0.0.0",     "0.0.0.0");

    /* Try some IPv6 addresses. */
#ifdef HAVE_INET6
    ok_addr( 94, true,  ipv6_addr,   ipv6_addr,     NULL);
    ok_addr( 95, true,  ipv6_addr,   ipv6_addr,     "128");
    ok_addr( 96, true,  ipv6_addr,   ipv6_addr,     "60");
    ok_addr( 97, true,  "::127",     "0:0::127",    "128");
    ok_addr( 98, true,  "::127",     "0:0::128",    "120");
    ok_addr( 99, false, "::127",     "0:0::128",    "128");
    ok_addr(100, false, "::7fff",    "0:0::8000",   "113");
    ok_addr(101, true,  "::7fff",    "0:0::8000",   "112");
    ok_addr(102, false, "::3:ffff",  "::2:ffff",    "120");
    ok_addr(103, false, "::3:ffff",  "::2:ffff",    "119");
    ok_addr(104, false, "ffff::1",   "7fff::1",     "1");
    ok_addr(105, true,  "ffff::1",   "7fff::1",     "0");
    ok_addr(106, false, "fffg::1",   "fffg::1",     NULL);
    ok_addr(107, false, "ffff::1",   "7fff::1",     "-1");
    ok_addr(108, false, "ffff::1",   "ffff::1",     "-1");
    ok_addr(109, false, "ffff::1",   "ffff::1",     "129");
#else
    skip_block(94, 16, "IPv6 not supported");
#endif

    /* Test some invalid addresses. */
    ok_addr(110, false, "fred",      "fred",        NULL);
    ok_addr(111, false, "",          "",            NULL);
    ok_addr(112, false, "",          "",            "0");
    ok_addr(113, false, "127.0.0.1", "127.0.0.1",   "pete");
    ok_addr(114, false, "127.0.0.1", "127.0.0.1",   "1p");
    ok_addr(115, false, "127.0.0.1", "127.0.0.1",   "1p");
    ok_addr(116, false, "127.0.0.1", "127.0.0.1",   "-1");
    ok_addr(117, false, "127.0.0.1", "127.0.0.1",   "33");

    return 0;
}
