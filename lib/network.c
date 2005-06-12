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
#ifdef HAVE_STREAMS_SENDFD
# include <stropts.h>
#endif

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "libinn.h"

/* Macros to set the len attribute of sockaddrs. */
#if HAVE_STRUCT_SOCKADDR_SA_LEN
# define sin_set_length(s)      ((s)->sin_len  = sizeof(struct sockaddr_in))
# define sin6_set_length(s)     ((s)->sin6_len = sizeof(struct sockaddr_in6))
#else
# define sin_set_length(s)      /* empty */
# define sin6_set_length(s)     /* empty */
#endif

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
**  Function used as a die handler in child processes to prevent any atexit
**  functions from being run and any buffers from being flushed twice.
*/
static int
network_child_fatal(void)
{
    _exit(1);
    return 1;
}


/*
**  Receive a file descriptor from a STREAMS pipe if supported and return the
**  file descriptor.  If not supported, return -1 for failure.
*/
#ifdef HAVE_STREAMS_SENDFD
static int
network_recvfd(int pipe)
{
    struct strrecvfd fdrec;

    if (ioctl(pipe, I_RECVFD, &fdrec) < 0) {
        syswarn("cannot receive file descriptor from innbind");
        return -1;
    } else
        return fdrec.fd;
}
#else /* !HAVE_STREAMS_SENDFD */
static int
network_recvfd(int pipe UNUSED)
{
    return -1;
}
#endif


/*
**  Call innbind to bind a socket to a privileged port.  Takes the file
**  descriptor, the family, the bind address (as a string), and the port
**  number.  Returns the bound file descriptor, which may be different than
**  the provided file descriptor if the system didn't support binding in a
**  subprocess, or -1 on error.
*/
static int
network_innbind(int fd, int family, const char *address, unsigned short port)
{
    char *path;
    char buff[128];
    int pipefds[2];
    pid_t child, result;
    int status;

    /* We need innconf in order to find innbind. */
    if (innconf == NULL || innconf->pathbin == NULL)
        return -1;

    /* Open a pipe to innbind and run it to bind the socket. */
    if (pipe(pipefds) < 0) {
        syswarn("cannot create pipe");
        return -1;
    }
    path = concatpath(innconf->pathbin, "innbind");
    snprintf(buff, sizeof(buff), "%d,%d,%s,%hu", fd, family, address, port);
    child = fork();
    if (child < 0) {
        syswarn("cannot fork innbind for %s,%hu", address, port);
        return -1;
    } else if (child == 0) {
        message_fatal_cleanup = network_child_fatal;
        close(1);
        if (dup2(pipefds[1], 1) < 0)
            sysdie("cannot dup pipe to stdout");
        close(pipefds[0]);
        if (execl(path, path, buff, (char *) 0) < 0)
            sysdie("cannot exec innbind for %s,%hu", address, port);
    }
    close(pipefds[1]);
    free(path);

    /* Read the results from innbind.  This will either be ok\n or no\n
       followed by an attempt to pass a new file descriptor back. */
    status = read(pipefds[0], buff, 3);
    buff[3] = '\0';
    if (status == 0) {
        warn("innbind returned no output, assuming failure");
        fd = -1;
    } else if (status < 0) {
        syswarn("cannot read from innbind");
        fd = -1;
    } else if (strcmp(buff, "no\n") == 0) {
        fd = network_recvfd(pipefds[0]);
    } else if (strcmp(buff, "ok\n") != 0) {
        fd = -1;
    }

    /* Wait for the results of the child process. */
    do {
        result = waitpid(child, &status, 0);
    } while (result == -1 && errno == EINTR);
    if (result != child) {
        syswarn("cannot wait for innbind for %s,%hu", address, port);
        return -1;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return fd;
    else {
        warn("innbind failed for %s,%hu", address, port);
        return -1;
    }
}


/*
**  Create an IPv4 socket and bind it, returning the resulting file
**  descriptor (or -1 on a failure).
*/
int
network_bind_ipv4(const char *address, unsigned short port)
{
    int fd, bindfd;
    struct sockaddr_in server;
    struct in_addr addr;

    /* Create the socket. */
    fd = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        syswarn("cannot create IPv4 socket for %s,%hu", address, port);
        return -1;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "0.0.0.0";

    /* Flesh out the socket and do the bind if we can.  Otherwise, call
       network_innbind to do the work. */
    if (port < 1024 && geteuid() != 0) {
        bindfd = network_innbind(fd, AF_INET, address, port);
        if (bindfd != fd)
            close(fd);
        return bindfd;
    } else {
        server.sin_family = AF_INET;
        server.sin_port = htons(port);
        if (!inet_aton(address, &addr)) {
            warn("invalid IPv4 address %s", address);
            return -1;
        }
        server.sin_addr = addr;
        sin_set_length(&server);
        if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
            syswarn("cannot bind socket for %s,%hu", address, port);
            return -1;
        }
        return fd;
    }
}


/*
**  Create an IPv6 socket and bind it, returning the resulting file
**  descriptor (or -1 on a failure).  Note that we don't warn (but still
**  return failure) if the reason for the socket creation failure is that IPv6
**  isn't supported; this is to handle systems like many Linux hosts where
**  IPv6 is available in userland but the kernel doesn't support it.
*/
#if HAVE_INET6
int
network_bind_ipv6(const char *address, unsigned short port)
{
    int fd, bindfd;
    struct sockaddr_in6 server;
    struct in6_addr addr;

    /* Create the socket. */
    fd = socket(PF_INET6, SOCK_STREAM, IPPROTO_IP);
    if (fd < 0) {
        if (errno != EAFNOSUPPORT && errno != EPROTONOSUPPORT)
            syswarn("cannot create IPv6 socket for %s,%hu", address, port);
        return -1;
    }
    network_set_reuseaddr(fd);

    /* Accept "any" or "all" in the bind address to mean 0.0.0.0. */
    if (!strcmp(address, "any") || !strcmp(address, "all"))
        address = "::";

    /* Flesh out the socket and do the bind if we can.  Otherwise, call
       network_innbind to do the work. */
    if (port < 1024 && geteuid() != 0) {
        bindfd = network_innbind(fd, AF_INET6, address, port);
        if (bindfd != fd)
            close(fd);
        return bindfd;
    } else {
        server.sin6_family = AF_INET6;
        server.sin6_port = htons(port);
        if (inet_pton(AF_INET6, address, &addr) < 1) {
            warn("invalid IPv6 address %s", address);
            close(fd);
            return -1;
        }
        server.sin6_addr = addr;
        sin6_set_length(&server);
        if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
            syswarn("cannot bind socket for %s,%hu", address, port);
            close(fd);
            return -1;
        }
        return fd;
    }
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
        strlcpy(name, sprint_sockaddr(addr->ai_addr), sizeof(name));
        if (addr->ai_family == AF_INET)
            fd = network_bind_ipv4(name, port);
        else if (addr->ai_family == AF_INET6)
            fd = network_bind_ipv6(name, port);
        else
            continue;
        if (fd >= 0) {
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
    int fd;

    fd = network_bind_ipv4("0.0.0.0", port);
    if (fd >= 0) {
        *fds = xmalloc(sizeof(int));
        *fds[0] = fd;
        *count = 1;
    } else {
        *fds = NULL;
        *count = 0;
    }
}
#endif /* HAVE_INET6 */


/*
**  Binds the given socket to an appropriate source address for its family,
**  using innconf information or the provided source address.  Returns true on
**  success and false on failure.
*/
static bool
network_source(int fd, int family, const char *source)
{
    if (source == NULL && innconf == NULL)
        return true;
    if (family == AF_INET) {
        struct sockaddr_in saddr;

        if (source == NULL)
            source = innconf->sourceaddress;
        if (source == NULL || strcmp(source, "all") == 0)
            return true;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        if (!inet_aton(source, &saddr.sin_addr))
            return false;
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#ifdef HAVE_INET6
    else if (family == AF_INET6) {
        struct sockaddr_in6 saddr;

        if (source == NULL)
            source = innconf->sourceaddress6;
        if (source == NULL || strcmp(source, "all") == 0)
            return true;
        memset(&saddr, 0, sizeof(saddr));
        saddr.sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, source, &saddr) < 1)
            return false;
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#endif
    else
        return true;
}


/*
**  Given a linked list of addrinfo structs representing the remote service,
**  try to create a local socket and connect to that service.  Takes an
**  optional source address.  Try each address in turn until one of them
**  connects.  Returns the file descriptor of the open socket on success, or
**  -1 on failure.  Tries to leave the reason for the failure in errno.
*/
int
network_connect(struct addrinfo *ai, const char *source)
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
        if (!network_source(fd, ai->ai_family, source))
            continue;
        if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            success = true;
            break;
        }
    }
    if (success)
        return fd;
    else {
        if (fd >= 0) {
            oerrno = errno;
            close(fd);
            errno = oerrno;
        }
        return -1;
    }
}


/*
**  Like network_connect, but takes a host and a port instead of an addrinfo
**  struct list.  Returns the file descriptor of the open socket on success,
**  or -1 on failure.  If getaddrinfo fails, errno may not be set to anything
**  useful.
*/
int
network_connect_host(const char *host, unsigned short port,
                     const char *source)
{
    struct addrinfo hints, *ai;
    char portbuf[16];
    int fd, oerrno;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = NETWORK_AF_HINT;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portbuf, sizeof(portbuf), "%d", port);
    if (getaddrinfo(host, portbuf, &hints, &ai) != 0)
        return -1;
    fd = network_connect(ai, source);
    oerrno = errno;
    freeaddrinfo(ai);
    errno = oerrno;
    return fd;
}


/*
**  Create a new socket of the specified domain and type and do the binding as
**  if we were a regular client socket, but then return before connecting.
**  Returns the file descriptor of the open socket on success, or -1 on
**  failure.  Intended primarily for the use of clients that will then go on
**  to do a non-blocking connect.
*/
int
network_client_create(int domain, int type, const char *source)
{
    int fd, oerrno;

    fd = socket(domain, type, 0);
    if (fd < 0)
        return -1;
    if (!network_source(fd, domain, source)) {
        oerrno = errno;
        close(fd);
        errno = oerrno;
        return -1;
    }
    return fd;
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


/*
**  Print an ASCII representation of the address of the given sockaddr into
**  the provided buffer.  This buffer must hold at least INET_ADDRSTRLEN
**  characters for IPv4 addresses and INET6_ADDRSTRLEN characters for IPv6, so
**  generally it should always be as large as the latter.  Returns success or
**  failure.
*/
bool
network_sprint_sockaddr(char *dst, size_t size, const struct sockaddr *addr)
{
    const char *result;

#ifdef HAVE_INET6
    if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sin6;

        sin6 = (const struct sockaddr_in6 *) addr;
        result = inet_ntop(AF_INET6, &sin6->sin6_addr, dst, size);
        return (result != NULL);
    }
#endif
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *sin;

        sin = (const struct sockaddr_in *) addr;
        result = inet_ntop(AF_INET, &sin->sin_addr, dst, size);
        return (result != NULL);
    } else {
        errno = EAFNOSUPPORT;
        return false;
    }
}
