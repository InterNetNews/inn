/* $Id$
 *
 * Utility functions for network connections using innbind.
 *
 * This is a collection of utility functions for network connections and
 * socket creation, encapsulating some of the complexities of IPv4 and IPv6
 * support and abstracting operations common to most network code.
 *
 * All of the portability difficulties with supporting IPv4 and IPv6 should be
 * encapsulated in the combination of this code and replacement
 * implementations for functions that aren't found on some pre-IPv6 systems.
 * No other part of the source tree should have to care about IPv4 vs. IPv6.
 *
 * This file is heavily based on lib/network.c.
 */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/wait.h"

#include <errno.h>
#ifdef HAVE_STREAMS_SENDFD
# include <stropts.h>
#endif

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "inn/network-innbind.h"
#include "inn/xmalloc.h"

/* If SO_REUSEADDR isn't available, make calls to set_reuseaddr go away. */
#ifndef SO_REUSEADDR
# define network_set_reuseaddr(fd)      /* empty */
#endif

/* If IPV6_V6ONLY isn't available, make calls to set_v6only go away. */
#ifndef IPV6_V6ONLY
# define network_set_v6only(fd)         /* empty */
#endif

/* If IP_FREEBIND isn't available, make calls to set_freebind go away. */
#ifndef IP_FREEBIND
# define network_set_freebind(fd)       /* empty */
#endif


/*
 * Set SO_REUSEADDR on a socket if possible (so that something new can listen
 * on the same port immediately if the daemon dies unexpectedly).
 */
#ifdef SO_REUSEADDR
static void
network_set_reuseaddr(socket_type fd)
{
    int flag = 1;
    const void *flagaddr = &flag;

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, flagaddr, sizeof(flag)) < 0)
        syswarn("cannot mark bind address reusable");
}
#endif


/*
 * Set IPV6_V6ONLY on a socket if possible, since the IPv6 behavior is more
 * consistent and easier to understand.
 */
#ifdef IPV6_V6ONLY
static void
network_set_v6only(socket_type fd)
{
    int flag = 1;

    if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &flag, sizeof(flag)) < 0)
        syswarn("cannot set IPv6 socket to v6only");
}
#endif


/*
 * Set IP_FREEBIND on a socket if possible, which allows binding servers to
 * IPv6 addresses that may not have been set up yet.
 */
#ifdef IP_FREEBIND
static void
network_set_freebind(socket_type fd)
{
    int flag = 1;

    if (setsockopt(fd, IPPROTO_IP, IP_FREEBIND, &flag, sizeof(flag)) < 0)
        syswarn("cannot set IPv6 socket to free binding");
}
#endif


/*
 * Function used as a die handler in child processes to prevent any atexit
 * functions from being run and any buffers from being flushed twice.
 */
static int
network_child_fatal(void)
{
    _exit(1);
    return 1;
}


/*
 * Receive a file descriptor from a STREAMS pipe if supported and return the
 * file descriptor.  If not supported, return INVALID_SOCKET for failure.
 */
#ifdef HAVE_STREAMS_SENDFD
static socket_type
network_recvfd(int pipe)
{
    struct strrecvfd fdrec;

    if (ioctl(pipe, I_RECVFD, &fdrec) < 0) {
        syswarn("cannot receive file descriptor from innbind");
        return INVALID_SOCKET;
    } else
        return fdrec.fd;
}
#else /* !HAVE_STREAMS_SENDFD */
static socket_type
network_recvfd(int pipe UNUSED)
{
    return INVALID_SOCKET;
}
#endif


/*
 * Call innbind to bind a socket to a privileged port.  Takes the file
 * descriptor, the family, the bind address (as a string), and the port
 * number.  Returns the bound file descriptor, which may be different than
 * the provided file descriptor if the system didn't support binding in a
 * subprocess, or INVALID_SOCKET on error.
 */
static socket_type
network_innbind(int fd, int family, const char *address, unsigned short port)
{
    char *path;
    char buff[128];
    int pipefds[2];
    pid_t child, result;
    int status;

    /* We need innconf in order to find innbind. */
    if (innconf == NULL || innconf->pathbin == NULL)
        return INVALID_SOCKET;

    /* Open a pipe to innbind and run it to bind the socket. */
    if (pipe(pipefds) < 0) {
        syswarn("cannot create pipe");
        return INVALID_SOCKET;
    }
    path = concatpath(innconf->pathbin, "innbind");
    snprintf(buff, sizeof(buff), "%d,%d,%s,%hu", fd, family, address, port);
    child = fork();
    if (child < 0) {
        syswarn("cannot fork innbind for %s, port %hu", address, port);
        return INVALID_SOCKET;
    } else if (child == 0) {
        message_fatal_cleanup = network_child_fatal;
        socket_close(1);
        if (dup2(pipefds[1], 1) < 0)
            sysdie("cannot dup pipe to stdout");
        socket_close(pipefds[0]);
        if (execl(path, path, buff, (char *) 0) < 0)
            sysdie("cannot exec innbind for %s, port %hu", address, port);
    }
    socket_close(pipefds[1]);
    free(path);

    /*
     * Read the results from innbind.  This will either be "ok\n" or "no\n"
     * followed by an attempt to pass a new file descriptor back.
     */
    status = socket_read(pipefds[0], buff, 3);
    buff[3] = '\0';
    if (status == 0) {
        warn("innbind returned no output, assuming failure");
        fd = INVALID_SOCKET;
    } else if (status < 0) {
        syswarn("cannot read from innbind");
        fd = INVALID_SOCKET;
    } else if (strcmp(buff, "no\n") == 0) {
        fd = network_recvfd(pipefds[0]);
    } else if (strcmp(buff, "ok\n") != 0) {
        fd = INVALID_SOCKET;
    }

    /* Wait for the results of the child process. */
    do {
        result = waitpid(child, &status, 0);
    } while (result == -1 && errno == EINTR);
    if (result != child) {
        syswarn("cannot wait for innbind for %s, port %hu", address, port);
        return INVALID_SOCKET;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return fd;
    else {
        warn("innbind failed for %s, port %hu", address, port);
        return INVALID_SOCKET;
    }
}


/*
 * Create an IPv4 socket and bind it, returning the resulting file descriptor
 * (or INVALID_SOCKET on a failure).
 */
socket_type
network_innbind_ipv4(int type, const char *address, unsigned short port)
{
    socket_type fd, bindfd;

    /* Use the generic network function when innbind is not necessary. */
    if (innconf->port >= 1024 || geteuid() == 0) {
        return network_bind_ipv4(type, address, port);
    }

    /* Create the socket. */
    fd = socket(PF_INET, type, IPPROTO_IP);
    if (fd == INVALID_SOCKET) {
        syswarn("cannot create IPv4 socket for %s, port %hu", address, port);
        return INVALID_SOCKET;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "0.0.0.0";

    /* Do the bind. */
    bindfd = network_innbind(fd, AF_INET, address, port);
    if (bindfd != fd)
        socket_close(fd);
    return bindfd;
}


/*
 * Create an IPv6 socket and bind it, returning the resulting file descriptor
 * (or INVALID_SOCKET on a failure).  This socket will be restricted to IPv6
 * only if possible (as opposed to the standard behavior of binding IPv6
 * sockets to both IPv6 and IPv4).
 *
 * Note that we don't warn (but still return failure) if the reason for the
 * socket creation failure is that IPv6 isn't supported; this is to handle
 * systems like many Linux hosts where IPv6 is available in userland but the
 * kernel doesn't support it.
 */
#if HAVE_INET6
socket_type
network_innbind_ipv6(int type, const char *address, unsigned short port)
{
    socket_type fd, bindfd;

    /* Use the generic network function when innbind is not necessary. */
    if (innconf->port >= 1024 || geteuid() == 0) {
        return network_bind_ipv6(type, address, port);
    }

    /* Create the socket. */
    fd = socket(PF_INET6, type, IPPROTO_IP);
    if (fd == INVALID_SOCKET) {
        if (socket_errno != EAFNOSUPPORT && socket_errno != EPROTONOSUPPORT)
            syswarn("cannot create IPv6 socket for %s, port %hu", address,
                    port);
        return INVALID_SOCKET;
    }
    network_set_reuseaddr(fd);

    /*
     * Restrict the socket to IPv6 only if possible.  The default behavior is
     * to bind IPv6 sockets to both IPv6 and IPv4 for backward compatibility,
     * but this causes various other problems (such as with reusing sockets
     * and requiring handling of mapped addresses).  Continue on if this
     * fails, however.
     */
    network_set_v6only(fd);

    /* Accept "any" or "all" in the bind address to mean ::. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "::";

    /*
     * If the address is not ::, use IP_FREEBIND if it's available.  This
     * allows the network stack to bind to an address that isn't configured.
     * We lose diagnosis of errors from specifying bind addresses that don't
     * exist on the system, but we gain the ability to bind to IPv6 addresses
     * that aren't yet configured.  Since IPv6 address configuration can take
     * unpredictable amounts of time during system setup, this is more robust.
     */
    if (strcmp(address, "::") != 0)
        network_set_freebind(fd);

    /* Do the bind. */
    bindfd = network_innbind(fd, AF_INET6, address, port);
    if (bindfd != fd)
        socket_close(fd);
    return bindfd;
}
#else /* HAVE_INET6 */
socket_type
network_innbind_ipv6(int type UNUSED, const char *address, unsigned short port)
{
    warn("cannot bind %s, port %hu: IPv6 not supported", address, port);
    socket_set_errno(EPROTONOSUPPORT);
    return INVALID_SOCKET;
}
#endif /* HAVE_INET6 */


/*
 * Create and bind sockets for every local address, as determined by
 * getaddrinfo if IPv6 is available (otherwise, just use the IPv4 loopback
 * address).  Takes the socket type and port number, and then a pointer to an
 * array of integers and a pointer to a count of them.  Allocates a new array
 * to hold the file descriptors and stores the count in the fourth argument.
 */
#if HAVE_INET6
bool
network_innbind_all(int type, unsigned short port, socket_type **fds,
                    unsigned int *count)
{
    struct addrinfo hints, *addrs, *addr;
    unsigned int size;
    int status;
    socket_type fd;
    char service[16], name[INET6_ADDRSTRLEN];

    /* Use the generic network function when innbind is not necessary. */
    if (innconf->port >= 1024 || geteuid() == 0) {
        return network_bind_all(type, port, fds, count);
    }

    *count = 0;

    /* Do the query to find all the available addresses. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = type;
    status = snprintf(service, sizeof(service), "%hu", port);
    if (status < 0 || (size_t) status > sizeof(service)) {
        warn("cannot convert port %hu to string", port);
        socket_set_errno_einval();
        return false;
    }
    status = getaddrinfo(NULL, service, &hints, &addrs);
    if (status < 0) {
        warn("getaddrinfo for %s failed: %s", service, gai_strerror(status));
        socket_set_errno_einval();
        return false;
    }

    /*
     * Now, try to bind each of them.  Start the fds array at two entries,
     * assuming an IPv6 and IPv4 socket, and grow it by two when necessary.
     */
    size = 2;
    *fds = xcalloc(size, sizeof(socket_type));
    for (addr = addrs; addr != NULL; addr = addr->ai_next) {
        network_sockaddr_sprint(name, sizeof(name), addr->ai_addr);
        if (addr->ai_family == AF_INET)
            fd = network_innbind_ipv4(type, name, port);
        else if (addr->ai_family == AF_INET6)
            fd = network_innbind_ipv6(type, name, port);
        else
            continue;
        if (fd != INVALID_SOCKET) {
            if (*count >= size) {
                size += 2;
                *fds = xreallocarray(*fds, size, sizeof(socket_type));
            }
            (*fds)[*count] = fd;
            (*count)++;
        }
    }
    freeaddrinfo(addrs);
    return (*count > 0);
}
#else /* HAVE_INET6 */
bool
network_innbind_all(int type, unsigned short port, socket_type **fds,
                    unsigned int *count)
{
    socket_type fd;

    /* Use the generic network function when innbind is not necessary. */
    if (innconf->port >= 1024 || geteuid() == 0) {
        return network_bind_all(type, port, fds, count);
    }

    fd = network_innbind_ipv4(type, "0.0.0.0", port);
    if (fd == INVALID_SOCKET) {
        *fds = NULL;
        *count = 0;
        return false;
    }
    *fds = xmalloc(sizeof(socket_type));
    *fds[0] = fd;
    *count = 1;
    return true;
}
#endif /* HAVE_INET6 */
