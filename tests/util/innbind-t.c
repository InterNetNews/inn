/* $Id$ */
/* innbind test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/wait.h"
#include <errno.h>
#ifdef HAVE_STREAMS_SENDFD
# include <stropts.h>
#endif

#include "inn/messages.h"
#include "libtest.h"

/* If SO_REUSEADDR isn't available, make calls to set_reuseaddr go away. */
#ifndef SO_REUSEADDR
# define set_reuseaddr(fd)      /* empty */
#endif

/* The path to the uninstalled innbind helper program. */
static const char innbind[] = "../../backends/innbind";

/* Set SO_REUSEADDR on a socket if possible. */
#ifdef SO_REUSEADDR
static void
set_reuseaddr(int fd)
{
    int flag = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
        syswarn("cannot mark bind address reusable");
}
#endif

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
    struct sockaddr_in server;
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
        _exit(1);
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(11119);
    if (inet_aton("127.0.0.1", &server.sin_addr) == 0)
        _exit(1);
    if (connect(fd, (struct sockaddr *) &server, sizeof(server)) < 0)
        _exit(1);
    client_send(fd);
}

/* Create a client IPv6 connection to the local server and then send a static
   string to that connection. */
#ifdef HAVE_INET6
static void
client_ipv6(void)
{
    struct sockaddr_in6 server;
    struct in6_addr addr;
    int fd;

    fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
        _exit(1);
    memset(&server, 0, sizeof(server));
    server.sin6_family = AF_INET6;
    server.sin6_port = htons(11119);
    if (inet_pton(AF_INET6, "::1", &addr) < 1)
        _exit(1);
    server.sin6_addr = addr;
    if (connect(fd, (struct sockaddr *) &server, sizeof(server)) < 0)
        sysdie("connect failed");
    client_send(fd);
}
#endif

/* Run innbind.  Takes the current test number, the file descriptor, the
   address family, and the address to bind to.  Returns the new test
   number. */
static int
run_innbind(int n, int fd, int family, const char *addr)
{
    int pipefds[2];
    char buffer[128];
    pid_t child, result;
    int status;

    if (pipe(pipefds) < 0)
        sysdie("cannot create pipe");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        close(1);
        if (dup2(pipefds[1], 1) < 0)
            _exit(1);
        close(pipefds[0]);
        snprintf(buffer, sizeof(buffer), "%d,%d,%s,11119", fd, family, addr);
        if (execl(innbind, innbind, buffer, (char *) 0) < 0)
            _exit(1);
    } else {
        close(pipefds[1]);
        status = read(pipefds[0], buffer, 3);
        if (status < 3) {
            syswarn("read failed (return %d)", status);
            ok(n++, false);
        } else {
            buffer[3] = '\0';
            ok_string(n++, "ok\n", buffer);
        }
        result = waitpid(child, &status, 0);
        if (result != child)
            die("cannot wait for innbind");
        ok(n++, WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    return n;
}

/* Run a test of binding for IPv4.  Create a socket, use innbind to bind it to
   port 11119 on the loopback address, and then fork a child to connect to it.
   Make sure that connecting to it works correctly. */
static int
test_ipv4(int n)
{
    int fd;
    pid_t child;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
        sysdie("cannot create socket");
    set_reuseaddr(fd);
    n = run_innbind(n, fd, AF_INET, "127.0.0.1");
    if (listen(fd, 1) < 0) {
        ok_block(n, 3, false);
        n += 3;
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

/* Run a test of binding for IPv6.  Create a socket, use innbind to bind it to
   port 11119 on the loopback address, and then fork a child to connect to it.
   Make sure that connecting to it works correctly. */
#ifdef HAVE_INET6
static int
test_ipv6(int n)
{
    int fd;
    pid_t child;

    fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT) {
            skip_block(n, 5, "IPv6 not supported");
            return n + 5;
        } else
            sysdie("cannot create socket");
    }
    set_reuseaddr(fd);
    n = run_innbind(n, fd, AF_INET6, "::");
    if (listen(fd, 1) < 0) {
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
    skip_block(n, 5, "IPv6 not supported");
    return n + 5;
}
#endif /* !HAVE_INET6 */

/* Test file descriptor passing on systems that support it.  Use innbind to
   create a socket bound to port 11119, receive the socket, and then fork a
   child to connect to it.  Make sure that connecting works correctly. */
#ifdef HAVE_STREAMS_SENDFD
static int
test_sendfd(int n)
{
    int pipefds[2], fd;
    char buffer[128];
    struct strrecvfd fdrec;
    pid_t child, result;
    int status;

    if (pipe(pipefds) < 0)
        sysdie("cannot create pipe");
    child = fork();
    fd = -1;
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        close(1);
        if (dup2(pipefds[1], 1) < 0)
            _exit(1);
        close(pipefds[0]);
        snprintf(buffer, sizeof(buffer), "0,%d,127.0.0.1,11119", AF_INET);
        if (execl(innbind, innbind, "-p", buffer, (char *) 0) < 0)
            _exit(1);
    } else {
        close(pipefds[1]);
        status = read(pipefds[0], buffer, 3);
        if (status < 3) {
            syswarn("read failed (return %d)", status);
            ok(n++, false);
        } else {
            buffer[3] = '\0';
            ok_string(n++, "no\n", buffer);
        }
        if (ioctl(pipefds[0], I_RECVFD, &fdrec) < 0)
            ok(n++, false);
        else
            ok(n++, true);
        fd = fdrec.fd;
        result = waitpid(child, &status, 0);
        if (result != child)
            die("cannot wait for innbind");
        ok(n++, WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }
    if (fd < 0 || listen(fd, 1) < 0) {
        ok(n++, false);
        ok(n++, false);
    } else {
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
#else /* !HAVE_STREAMS_SENDFD */
static int
test_sendfd(int n)
{
    skip_block(n, 5, "SENDFD not supported");
    return n;
}
#endif /* !HAVE_INET6 */

int
main(void)
{
    int n;

    if (access("innbind.t", F_OK) < 0)
        if (access("util/innbind.t", F_OK) == 0)
            chdir("util");

    test_init(15);

    n = test_ipv4(1);           /* Tests  1-5.  */
    n = test_ipv6(n);           /* Tests  6-10. */
    test_sendfd(n);         /* Tests 11-15. */

    return 0;
}
