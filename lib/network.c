/*  $Id$
**
**  Utility functions for network connections.
**
**  This is a collection of utility functions for network connections and
**  socket creation, encapsulating some of the complexities of IPv4 and IPv6
**  support and abstracting operations common to most network code.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/wait.h"
#include <errno.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "libinn.h"

/* If SO_REUSEADDR isn't available, make calls to set_reuseaddr go away. */
#ifndef SO_REUSEADDR
# define network_set_reuseaddr(fd)      /* empty */
#endif


/*
**  Set SO_REUSEADDR on a socket if possible (so that something new can listen
**  on the same port immediately if INN dies unexpectedly).
*/
#ifdef SO_REUSEADDR
static void
network_set_reuseaddr(int fd)
{
    int flag = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0)
        syswarn("cannot mark bind address reusable");
}
#endif


/*
**  Call innbind to bind a socket to a privileged port.  Takes the file
**  descriptor, the family, the bind address (as a string), and the port
**  number, and returns true on success and false on failure.
*/
static bool
network_bind(int fd, int family, const char *address, unsigned short port)
{
    char *path;
    char spec[128];
    pid_t child;
    int result, status;

    /* Run innbind to bind the socket.  A nice optimization would be to only
       run innbind when the port is < 1024 and otherwise bind it directly, but
       this is simple and binding new sockets is rare. */
    path = concatpath(innconf->pathbin, "innbind");
    snprintf(spec, sizeof(spec), "%d,%d,%s,%hu", fd, family, address, port);
    child = fork();
    if (child < 0) {
        syswarn("cannot fork innbind for %s,%hu", address, port);
        return false;
    } else if (child == 0) {
        execl(path, path, spec, NULL);
        sysdie("cannot exec innbind for %s,%hu", address, port);
    }
    free(path);

    /* Wait for the results of the child process. */
    do {
        result = waitpid(child, &status, 0);
    } while (result == -1 && errno == EINTR);
    if (result != child) {
        syswarn("cannot wait for innbind for %s,%hu", address, port);
        return false;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return true;
    else {
        warn("innbind failed for %s,%hu", address, port);
        return false;
    }
}


/*
**  Create an IPv4 socket and start listening on it, returning the resulting
**  file descriptor (or -1 on a failure).
*/
int
network_bind_ipv4(const char *address, unsigned short port)
{
    int fd;

    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        syswarn("cannot create IPv4 socket for %s,%hu", address, port);
        return -1;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "0.0.0.0";
    return network_bind(fd, AF_INET, address, port) ? fd : -1;
}


/*
**  Create an IPv6 socket and start listening on it, returning the resulting
**  file descriptor (or -1 on a failure).
*/
#if HAVE_INET6
int
network_bind_ipv6(const char *address, unsigned short port)
{
    int fd;

    fd = socket(PF_INET6, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        syswarn("cannot create IPv6 socket for %s,%hu", address, port);
        return -1;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "::";
    return network_bind(fd, AF_INET6, address, port) ? fd : -1;
}
#else
int
network_bind_ipv6(const char *address, unsigned short port)
{
    warn("cannot bind %s,%hu: not built with IPv6 support", address, port);
    return -1;
}
#endif


/*
**  Create and bind sockets for every local address, as determined by
**  getaddrinfo if IPv6 is enabled (otherwise, just uses the wildcard IPv4
**  address).  Takes the port number, and then a pointer to an array of
**  integers and a pointer to a count of them.  Allocates a new array to hold
**  the file descriptors and stores the count in the third argument.
*/
#if HAVE_INET6
void
network_bind_all(unsigned short port, int **fds, int *count)
{
    struct addrinfo hints, *addrs, *addr;
    int error, fd, size;
    char service[16], name[128];

    *count = 0;

    /* Do the query to find all the available addresses. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof(service), "%hu", port);
    error = getaddrinfo(NULL, service, &hints, &addrs);
    if (error < 0) {
        warn("getaddrinfo failed: %s", gai_strerror(error));
        return;
    }

    /* Now, try to bind each of them.  Start the fds array at two entries,
       assuming an IPv6 and IPv4 socket, and grow it by two when necessary. */
    size = 2;
    *fds = xmalloc(size * sizeof(int));
    for (addr = addrs; addr != NULL; addr = addr->ai_next) {
        fd = socket(addr->ai_family, SOCK_STREAM, IPPROTO_IP);
        if (fd < 0) {
            if (errno != EAFNOSUPPORT && errno != EPROTONOSUPPORT)
                syswarn("cannot create socket for %s,%hu",
                        sprint_sockaddr(addr->ai_addr), port);
            continue;
        }
        network_set_reuseaddr(fd);
        strlcpy(name, sprint_sockaddr(addr->ai_addr), sizeof(name));
        if (!network_bind(fd, addr->ai_family, name, port))
            close(fd);
        else {
            if (*count >= size) {
                size += 2;
                *fds = xrealloc(*fds, size * sizeof(int));
            }
            *fds[*count] = fd;
            (*count)++;
        }
    }
    freeaddrinfo(addrs);
}
#else
void
network_bind_all(unsigned short port, int **fds, int *count)
{
    *fds = xmalloc(sizeof(int));
    *count = 1;
    *fds[0] = network_bind_ipv4("0.0.0.0", port);
}
#endif
