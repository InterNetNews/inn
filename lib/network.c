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
#else /* HAVE_INET6 */
int
network_bind_ipv6(const char *address, unsigned short port)
{
    warn("cannot bind %s,%hu: not built with IPv6 support", address, port);
    return -1;
}
#endif /* HAVE_INET6 */


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
            (*fds)[*count] = fd;
            (*count)++;
        }
    }
    freeaddrinfo(addrs);
}
#else /* HAVE_INET6 */
void
network_bind_all(unsigned short port, int **fds, int *count)
{
    *fds = xmalloc(sizeof(int));
    *count = 1;
    *fds[0] = network_bind_ipv4("0.0.0.0", port);
}
#endif /* HAVE_INET6 */


/*
**  Binds the given socket to an appropriate source address for its family,
**  using innconf information.  Returns true on success and false on failure.
*/
static bool
network_source(int fd, int family)
{
    if (family == AF_INET && innconf->sourceaddress != NULL) {
        struct sockaddr_in saddr;

        if (strcmp(innconf->sourceaddress, "all") == 0)
            return true;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        if (!inet_aton(innconf->sourceaddress, &saddr.sin_addr))
            return false;
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#ifdef HAVE_INET6
    else if (family == AF_INET6 && innconf->sourceaddress6 != NULL) {
        struct sockaddr_in6 saddr;

        if (strcmp(innconf->sourceaddress6, "all") == 0)
            return true;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, innconf->sourceaddress6, &saddr) < 1)
            return false;
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#endif
    else
        return true;
}


/*
**  Given a linked list of addrinfo structs representing the remote service,
**  try to create a local socket and connect to that service.  Try each
**  address in turn until one of them connects.  Returns the file descriptor
**  of the open socket on success, or -1 on failure.  Tries to leave the
**  reason for the failure in errno.
*/
int
network_connect(struct addrinfo *ai)
{
    int fd = -1;
    int oerrno;
    bool success;

    for (success = false; ai != NULL; ai = ai->ai_next) {
        if (fd >= 0)
            close(fd);
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0)
            continue;
        if (!network_source(fd, ai->ai_family))
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            success = true;
            break;
        }
    }
    if (success)
        return fd;
    else {
        oerrno = errno;
        close(fd);
        errno = oerrno;
        return -1;
    }
}


/*
**  Strip IP options if possible (source routing and similar sorts of things)
**  just out of paranoia.  This function currently only supports IPv4.  If
**  anyone knows for sure whether this is necessary for IPv6 as well and knows
**  how to support it if it is necessary, please submit a patch.  If any
**  options are found, they're reported using notice.
**
**  Based on 4.4BSD rlogind source by way of Wietse Venema (tcp_wrappers),
**  adapted for INN by smd and reworked some by Russ Allbery.
*/
#ifdef IP_OPTIONS
bool
network_kill_options(int fd, struct sockaddr *remote)
{
    int status;
    char options[BUFSIZ / 3];
    socklen_t optsize = sizeof(options);

    if (remote->sa_family != AF_INET)
        return true;
    status = getsockopt(fd, IPPROTO_IP, IP_OPTIONS, options, &optsize);
    if (status == 0 && optsize != 0) {
        char hex[BUFSIZ];
        char *opt, *output;

        output = hex;
        for (opt = options; optsize > 0; opt++, optsize--, output += 3)
            snprintf(output, sizeof(hex) - (output - hex), " %2.2x", *opt);
        notice("connect from %s with IP options (ignored):%s",
               sprint_sockaddr(remote), hex);
        if (setsockopt(fd, IPPROTO_IP, IP_OPTIONS, NULL, 0) != 0) {
            syswarn("setsockopt IP_OPTIONS NULL failed");
            return false;
        }
    }
    return true;
}
#else /* !IP_OPTIONS */
bool
network_kill_options(int fd UNUSED, struct sockaddr *remote UNUSED)
{
    return true;
}
#endif
