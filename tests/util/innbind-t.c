/* $Id$ */
/* innbind test suite. */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include <errno.h>

#include "inn/messages.h"
#include "libtest.h"

/* The server portion of the test.  Listens to a socket and accepts a
   connection, making sure what is printed on that connection matches what the
   client is supposed to print. */
static void
listener(int fd, int n)
{
    int client;
    FILE *out;
    char buffer[512];

    client = accept(fd, NULL, NULL);
    close(fd);
    if (client < 0) {
        syswarn("cannot accept connection from socket");
        ok(n, false);
        return;
    }
    ok(n, true);
    out = fdopen(client, "r");
    if (fgets(buffer, sizeof(buffer), out) == NULL) {
        syswarn("cannot read from socket");
        ok(n + 1, false);
        return;
    }
    ok_string(n + 1, "socket test\r\n", buffer);
    fclose(out);
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

/* The server portion of the exec test.  Accept a connection on the provided
   file descriptor and then send one line and close the connection. */
static void
responder(int fd)
{
    int client;

    if (listen(fd, 1) < 0)
        _exit(1);
    client = accept(fd, NULL, NULL);
    close(fd);
    if (client < 0)
        _exit(1);
    client_send(client);
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
        _exit(1);
    client_send(fd);
}
#endif

/* Run a test of binding for IPv4.  Create a socket, use innbind to bind it to
   port 11119 on the loopback address, and then fork a child to connect to it.
   Make sure that connecting to it works correctly. */
static void
test_ipv4(int n)
{
    int fd;
    char command[128];
    pid_t child;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
        sysdie("cannot create socket");
    snprintf(command, sizeof(command),
             "../../backends/innbind %d,%d,127.0.0.1,11119", fd,
             AF_INET);
    if (system(command) == 0)
        ok(n, true);
    else {
        warn("cannot bind socket");
        ok(n, false);
        return;
    }
    if (listen(fd, 1) < 0)
        sysdie("cannot listen to socket");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0)
        client_ipv4();
    else
        listener(fd, n + 1);
}

/* Run a test of binding for IPv6.  Create a socket, use innbind to bind it to
   port 11119 on the loopback address, and then fork a child to connect to it.
   Make sure that connecting to it works correctly. */
#ifdef HAVE_INET6
static void
test_ipv6(int n)
{
    int fd, i;
    char command[128];
    pid_t child;

    fd = socket(AF_INET6, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        if (errno == EAFNOSUPPORT || errno == EPROTONOSUPPORT) {
            for (i = n; i < n + 3; i++)
                printf("ok %d # skip\n", i);
            return;
        } else
            sysdie("cannot create socket");
    }
    snprintf(command, sizeof(command),
             "../../backends/innbind %d,%d,::1,11119", fd, AF_INET);
    if (system(command) == 0)
        ok(n, true);
    else {
        warn("cannot bind socket");
        ok(n, false);
        return;
    }
    if (listen(fd, 1) < 0)
        sysdie("cannot listen to socket");
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0)
        client_ipv6();
    else
        listener(fd, n + 1);
}
#else
static void
test_ipv6(int n)
{
    int i;

    for (i = n; i < n + 3; i++)
        printf("ok %d # skip\n", i);
}
#endif

/* Test the exec mode of innbind.  We're going to exec ourselves again, with
   an IPv4 socket open on 11119.  That socket will just be a responder, so
   we'll then connect to it and make sure that it returns what we want. */
static void
test_exec(int n)
{
    char command[128], buffer[512];
    pid_t child;
    struct sockaddr_in server;
    int fd;
    FILE *out;

    /* Run the innbind as a child process. */
    snprintf(command, sizeof(command),
             "../../backends/innbind -e 0,%d,127.0.0.1,11119 -- ./innbind.t",
             AF_INET);
    child = fork();
    if (child < 0)
        sysdie("cannot fork");
    else if (child == 0) {
        system(command);
        _exit(0);
    }

    /* We're in the parent.  Try to connect to the server that we should have
       just created and make sure that it returns the appropriate string. */
    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0)
        sysdie("cannot create socket");
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(11119);
    if (inet_aton("127.0.0.1", &server.sin_addr) == 0)
        die("cannot parse 127.0.0.1");
    sleep(1);
    if (connect(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        syswarn("connect to exec client failed");
        ok(n++, false);
        ok(n++, false);
    } else {
        ok(n++, true);
        out = fdopen(fd, "r");
        if (out == NULL)
            sysdie("cannot fdopen %d", fd);
        if (fgets(buffer, sizeof(buffer), out) == NULL) {
            syswarn("cannot read from socket");
            ok(n, false);
        } else
            ok_string(n, "socket test\r\n", buffer);
        fclose(out);
    }
    close(fd);
}

int
main(int argc, char *argv[])
{
    int fd;

    /* If the first argument is -r, we're the server part of the exec test.
       Figure out our open file descriptor and then run responder on it. */
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        fd = strtol(argv[2], NULL, 10);
        responder(fd);
        _exit(0);
    }

    if (access("innbind.t", F_OK) < 0)
        if (access("util/innbind.t", F_OK) == 0)
            chdir("util");

    test_init(8);

    test_ipv4(1);
    test_ipv6(4);
    test_exec(7);

    return 0;
}
