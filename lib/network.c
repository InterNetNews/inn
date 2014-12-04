/* $Id$
 *
 * Utility functions for network connections.
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
 * In this file, casts through void * or const void * of struct sockaddr *
 * parameters are to silence gcc warnings with -Wcast-align.  The specific
 * address types often require stronger alignment than a struct sockaddr, and
 * were originally allocated with that alignment.  GCC doesn't have a good way
 * of knowing that this code is correct.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2011, 2012, 2013, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2004, 2005, 2006, 2007, 2008
 *     by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1991, 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001,
 *     2002, 2003 by The Internet Software Consortium and Rich Salz
 *
 * This code is derived from software contributed to the Internet Software
 * Consortium by Rich Salz.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include <errno.h>
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/macros.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "inn/xmalloc.h"
#include "inn/xwrite.h"

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

/* If IPV6_V6ONLY isn't available, make calls to set_v6only go away. */
#ifndef IPV6_V6ONLY
# define network_set_v6only(fd)         /* empty */
#endif

/* If IP_FREEBIND isn't available, make calls to set_freebind go away. */
#ifndef IP_FREEBIND
# define network_set_freebind(fd)       /* empty */
#endif

/*
 * Windows requires a different function when sending to sockets, but can't
 * return short writes on blocking sockets.
 */
#ifdef _WIN32
# define socket_xwrite(fd, b, s)        send((fd), (b), (s), 0)
#else
# define socket_xwrite(fd, b, s)        xwrite((fd), (b), (s))
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
static void UNUSED
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
static void UNUSED
network_set_freebind(socket_type fd)
{
    int flag = 1;

    if (setsockopt(fd, IPPROTO_IP, IP_FREEBIND, &flag, sizeof(flag)) < 0)
        syswarn("cannot set IPv6 socket to free binding");
}
#endif


/*
 * Create an IPv4 socket and bind it, returning the resulting file descriptor
 * (or INVALID_SOCKET on a failure).
 */
socket_type
network_bind_ipv4(int type, const char *address, unsigned short port)
{
    socket_type fd;
    struct sockaddr_in server;
    struct in_addr addr;

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

    /* Flesh out the socket and do the bind. */
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    if (!inet_aton(address, &addr)) {
        warn("invalid IPv4 address %s", address);
        socket_set_errno_einval();
        return INVALID_SOCKET;
    }
    server.sin_addr = addr;
    sin_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        syswarn("cannot bind socket for %s, port %hu", address, port);
        socket_close(fd);
        return INVALID_SOCKET;
    }
    return fd;
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
network_bind_ipv6(int type, const char *address, unsigned short port)
{
    socket_type fd;
    struct sockaddr_in6 server;
    struct in6_addr addr;

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
     *
     * Ensure there is always a block here to avoid compiler warnings, since
     * network_set_freebind() may expand into nothing.
     */
    if (strcmp(address, "::") != 0) {
        network_set_freebind(fd);
    }

    /* Flesh out the socket and do the bind. */
    memset(&server, 0, sizeof(server));
    server.sin6_family = AF_INET6;
    server.sin6_port = htons(port);
    if (inet_pton(AF_INET6, address, &addr) < 1) {
        warn("invalid IPv6 address %s", address);
        socket_set_errno_einval();
        return INVALID_SOCKET;
    }
    server.sin6_addr = addr;
    sin6_set_length(&server);
    if (bind(fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        syswarn("cannot bind socket for %s, port %hu", address, port);
        socket_close(fd);
        return INVALID_SOCKET;
    }
    return fd;
}

#else /* HAVE_INET6 */

socket_type
network_bind_ipv6(int type UNUSED, const char *address, unsigned short port)
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
network_bind_all(int type, unsigned short port, socket_type **fds,
                 unsigned int *count)
{
    struct addrinfo hints, *addrs, *addr;
    unsigned int size;
    int status;
    socket_type fd;
    char service[16], name[INET6_ADDRSTRLEN];

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
            fd = network_bind_ipv4(type, name, port);
        else if (addr->ai_family == AF_INET6)
            fd = network_bind_ipv6(type, name, port);
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
network_bind_all(int type, unsigned short port, socket_type **fds,
                 unsigned int *count)
{
    socket_type fd;

    fd = network_bind_ipv4(type, "0.0.0.0", port);
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


/*
 * Free the array of file descriptors allocated by network_bind_all.  This is
 * a simple wrapper around free, needed on platforms where libraries allocate
 * memory from a different memory domain than programs (such as Windows).
 */
void
network_bind_all_free(socket_type *fds)
{
    free(fds);
}


/*
 * Given an array of file descriptors and the length of that array (the same
 * data that's returned by network_bind_all), wait for an incoming connection
 * on any of those sockets and return the file descriptor that selects ready
 * for read.
 *
 * This is primarily intended for UDP services listening on multiple file
 * descriptors, and also provides part of the code for network_accept_any.
 * TCP services will probably want to use network_accept_any instead.
 *
 * Returns the new socket on success or INVALID_SOCKET on failure.  Note that
 * INVALID_SOCKET may be returned if the timeout is interrupted by a signal,
 * which is not, precisely speaking, an error condition.  In this case, errno
 * will be set to EINTR.
 *
 * This is not intended to be a replacement for a full event loop, just some
 * simple shared code for UDP services.
 */
socket_type
network_wait_any(socket_type fds[], unsigned int count)
{
    fd_set readfds;
    socket_type maxfd, fd;
    unsigned int i;
    int status;

    FD_ZERO(&readfds);
    maxfd = -1;
    for (i = 0; i < count; i++) {
        FD_SET(fds[i], &readfds);
        if (fds[i] > maxfd)
            maxfd = fds[i];
    }
    status = select(maxfd + 1, &readfds, NULL, NULL, NULL);
    if (status < 0)
        return INVALID_SOCKET;
    fd = INVALID_SOCKET;
    for (i = 0; i < count; i++)
        if (FD_ISSET(fds[i], &readfds)) {
            fd = fds[i];
            break;
        }
    return fd;
}


/*
 * Given an array of file descriptors and the length of that array (the same
 * data that's returned by network_bind_all), wait for an incoming connection
 * on any of those sockets, accept the connection with accept(), and return
 * the new file descriptor.
 *
 * This is essentially a replacement for accept() with a single socket for
 * daemons that are listening to multiple separate bound sockets, possibly
 * because they need to listen to specific interfaces or possibly because
 * they're listening for both IPv4 and IPv6 connections.
 *
 * Returns the new socket on success or INVALID_SOCKET on failure.  On
 * success, fills out the arguments with the address and address length of the
 * accepted client.  No error will be reported, so the caller should do that.
 * Note that INVALID_SOCKET may be returned if the timeout is interrupted by a
 * signal, which is not, precisely speaking, an error condition.  In this
 * case, errno will be set to EINTR.
 */
socket_type
network_accept_any(socket_type fds[], unsigned int count,
                   struct sockaddr *addr, socklen_t *addrlen)
{
    socket_type fd;

    fd = network_wait_any(fds, count);
    if (fd == INVALID_SOCKET)
        return INVALID_SOCKET;
    else
        return accept(fd, addr, addrlen);
}


/*
 * Binds the given socket to an appropriate source address for its family
 * using the provided source address.  Returns true on success and false on
 * failure.
 */
static bool
network_source(socket_type fd, int family, const char *source)
{
    if (source == NULL && innconf == NULL)
        return true;
    if (family == AF_INET) {
        struct sockaddr_in saddr;

        if (source == NULL && innconf != NULL)
            source = innconf->sourceaddress;
        if (source == NULL ||
            strcmp(source, "all") == 0 || strcmp(source, "any") == 0)
              return true;

        memset(&saddr, 0, sizeof(saddr));
        saddr.sin_family = AF_INET;
        if (!inet_aton(source, &saddr.sin_addr)) {
            socket_set_errno_einval();
            return false;
        }
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#ifdef HAVE_INET6
    else if (family == AF_INET6) {
        struct sockaddr_in6 saddr;

        memset(&saddr, 0, sizeof(saddr));
        if (source == NULL && innconf != NULL)
            source = innconf->sourceaddress6;
        if (source == NULL ||
            strcmp(source, "all") == 0 || strcmp(source, "any") == 0)
              return true;

        saddr.sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, source, &saddr.sin6_addr) < 1) {
            socket_set_errno_einval();
            return false;
        }
        return bind(fd, (struct sockaddr *) &saddr, sizeof(saddr)) == 0;
    }
#endif
    else {
        socket_set_errno(EAFNOSUPPORT);
        return false;
    }
}


/*
 * Internal helper function that waits for a non-blocking connect to complete
 * on a socket.  Takes the file descriptor and the timeout.  Returns 0 on a
 * successful completion of the connect within the timeout and -1 on failure.
 * On failure, sets the socket errno.
 */
static int
connect_wait(socket_type fd, time_t timeout)
{
    int status, err;
    socklen_t length;
    struct timeval tv;
    fd_set set;

    /*
     * Use select to poll the file descriptor.  Loop if interrupted by a
     * caught signal.  This means we could wait for longer than the timeout
     * when interrupted, but there's no good way of recovering the elapsed
     * time that's worth the hassle.
     */
    do {
        tv.tv_sec = timeout;
        tv.tv_usec = 0;
        FD_ZERO(&set);
        FD_SET(fd, &set);
        status = select(fd + 1, NULL, &set, NULL, &tv);
    } while (status < 0 && socket_errno == EINTR);

    /*
     * If we timed out, set errno appropriately.  If the connection completes,
     * retrieve the actual status from the socket.
     */
    if (status == 0 && !FD_ISSET(fd, &set)) {
        status = -1;
        socket_set_errno(ETIMEDOUT);
    } else if (status > 0 && FD_ISSET(fd, &set)) {
        length = sizeof(err);
        status = getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &length);
        if (status == 0) {
            status = (err == 0) ? 0 : -1;
            socket_set_errno(err);
        }
    }
    return status;
}


/*
 * Given a linked list of addrinfo structs representing the remote service,
 * try to create a local socket and connect to that service.  Takes an
 * optional source address.  Try each address in turn until one of them
 * connects.  Returns the file descriptor of the open socket on success, or
 * INVALID_SOCKET on failure.  Tries to leave the reason for the failure in
 * errno.
 */
socket_type
network_connect(const struct addrinfo *ai, const char *source, time_t timeout)
{
    socket_type fd = INVALID_SOCKET;
    int oerrno, status;

    for (status = -1; status != 0 && ai != NULL; ai = ai->ai_next) {
        if (fd != INVALID_SOCKET)
            socket_close(fd);
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd == INVALID_SOCKET)
            continue;
        if (!network_source(fd, ai->ai_family, source))
            continue;
        if (timeout == 0)
            status = connect(fd, ai->ai_addr, ai->ai_addrlen);
        else {
            fdflag_nonblocking(fd, true);
            status = connect(fd, ai->ai_addr, ai->ai_addrlen);
            if (status < 0 && socket_errno == EINPROGRESS)
                status = connect_wait(fd, timeout);
            oerrno = socket_errno;
            fdflag_nonblocking(fd, false);
            socket_set_errno(oerrno);
        }
    }
    if (status == 0)
        return fd;
    else {
        if (fd != INVALID_SOCKET) {
            oerrno = socket_errno;
            socket_close(fd);
            socket_set_errno(oerrno);
        }
        return INVALID_SOCKET;
    }
}


/*
 * Like network_connect, but takes a host and a port instead of an addrinfo
 * struct list.  Returns the file descriptor of the open socket on success, or
 * INVALID_SOCKET on failure.  If getaddrinfo fails, errno may not be set to
 * anything useful.
 */
socket_type
network_connect_host(const char *host, unsigned short port,
                     const char *source, time_t timeout)
{
    struct addrinfo hints, *ai;
    char portbuf[16];
    socket_type fd;
    int status, oerrno;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    status = snprintf(portbuf, sizeof(portbuf), "%hu", port);
    if (status > 0 && (size_t) status > sizeof(portbuf)) {
        status = -1;
        socket_set_errno_einval();
    }
    if (status < 0)
        return INVALID_SOCKET;
    if (getaddrinfo(host, portbuf, &hints, &ai) != 0)
        return INVALID_SOCKET;
    fd = network_connect(ai, source, timeout);
    oerrno = socket_errno;
    freeaddrinfo(ai);
    socket_set_errno(oerrno);
    return fd;
}


/*
 * Create a new socket of the specified domain and type and do the binding as
 * if we were a regular client socket, but then return before connecting.
 * Returns the file descriptor of the open socket on success, or
 * INVALID_SOCKET on failure.  Intended primarily for the use of clients that
 * will then go on to do a non-blocking connect.
 */
socket_type
network_client_create(int domain, int type, const char *source)
{
    socket_type fd;
    int oerrno;

    fd = socket(domain, type, 0);
    if (fd == INVALID_SOCKET)
        return INVALID_SOCKET;
    if (!network_source(fd, domain, source)) {
        oerrno = socket_errno;
        socket_close(fd);
        socket_set_errno(oerrno);
        return INVALID_SOCKET;
    }
    return fd;
}


/*
 * Equivalent to read, but reads all the available data up to the buffer
 * length, using multiple reads if needed and handling EINTR and EAGAIN.  If
 * we get EOF before we get enough data, set the socket errno to EPIPE.
 */
static ssize_t
socket_xread(socket_type fd, void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    /* Abort the read if we try 100 times with no forward progress. */
    for (total = 0, status = 0; total < size; total += status) {
        if (++count > 100)
            break;
        status = socket_read(fd, (char *) buffer + total, size - total);
        if (status > 0)
            count = 0;
        else if (status == 0)
            break;
        else {
            if ((socket_errno != EINTR) && (socket_errno != EAGAIN))
                break;
            status = 0;
        }
    }
    if (status == 0 && total < size)
        socket_set_errno(EPIPE);
    return (total < size) ? -1 : (ssize_t) total;
}


/*
 * Read the specified number of bytes from the network, enforcing a timeout
 * (in seconds).  We use select to wait for data to become available and then
 * keep reading until either we time out or we've gotten all the data we're
 * looking for.  timeout may be 0 to never time out.  Return true on success
 * and false (setting socket_errno) on failure.
 */
bool
network_read(socket_type fd, void *buffer, size_t total, time_t timeout)
{
    time_t start, now;
    fd_set set;
    struct timeval tv;
    size_t got = 0;
    ssize_t status;

    /* If there's no timeout, do this the easy way. */
    if (timeout == 0)
        return (socket_xread(fd, buffer, total) >= 0);

    /*
     * The hard way.  We try to apply the timeout on the whole read.  If
     * either select or read fails with EINTR, restart the loop, and rely on
     * the overall timeout to limit how long we wait without forward
     * progress.
     */
    start = time(NULL);
    now = start;
    do {
        FD_ZERO(&set);
        FD_SET(fd, &set);
        tv.tv_sec = timeout - (now - start);
        if (tv.tv_sec < 1)
            tv.tv_sec = 1;
        tv.tv_usec = 0;
        status = select(fd + 1, &set, NULL, NULL, &tv);
        if (status < 0) {
            if (socket_errno == EINTR)
                continue;
            return false;
        } else if (status == 0) {
            socket_set_errno(ETIMEDOUT);
            return false;
        }
        status = socket_read(fd, (char *) buffer + got, total - got);
        if (status < 0) {
            if (socket_errno == EINTR)
                continue;
            return false;
        } else if (status == 0) {
            socket_set_errno(EPIPE);
            return false;
        }
        got += status;
        if (got == total)
            return true;
        now = time(NULL);
    } while (now - start < timeout);
    socket_set_errno(ETIMEDOUT);
    return false;
}


/*
 * Write the specified number of bytes from the network, enforcing a timeout
 * (in seconds).  We use select to wait for the socket to become available and
 * then keep reading until either we time out or we've sent all the data.
 * timeout may be 0 to never time out.  Return true on success and false
 * (setting socket_errno) on failure.
 */
bool
network_write(socket_type fd, const void *buffer, size_t total, time_t timeout)
{
    time_t start, now;
    fd_set set;
    struct timeval tv;
    size_t sent = 0;
    ssize_t status;
    int err;

    /* If there's no timeout, do this the easy way. */
    if (timeout == 0)
        return (socket_xwrite(fd, buffer, total) >= 0);

    /* The hard way.  We try to apply the timeout on the whole write.  If
     * either select or read fails with EINTR, restart the loop, and rely on
     * the overall timeout to limit how long we wait without forward progress.
     */
    fdflag_nonblocking(fd, true);
    start = time(NULL);
    now = start;
    do {
        FD_ZERO(&set);
        FD_SET(fd, &set);
        tv.tv_sec = timeout - (now - start);
        if (tv.tv_sec < 1)
            tv.tv_sec = 1;
        tv.tv_usec = 0;
        status = select(fd + 1, NULL, &set, NULL, &tv);
        if (status < 0) {
            if (socket_errno == EINTR)
                continue;
            goto fail;
        } else if (status == 0) {
            socket_set_errno(ETIMEDOUT);
            goto fail;
        }
        status = socket_write(fd, (const char *) buffer + sent, total - sent);
        if (status < 0) {
            if (socket_errno == EINTR)
                continue;
            goto fail;
        }
        sent += status;
        if (sent == total) {
            fdflag_nonblocking(fd, false);
            return true;
        }
        now = time(NULL);
    } while (now - start < timeout);
    socket_set_errno(ETIMEDOUT);

fail:
    err = socket_errno;
    fdflag_nonblocking(fd, false);
    socket_set_errno(err);
    return false;
}


/*
 * Print an ASCII representation of the address of the given sockaddr into the
 * provided buffer.  This buffer must hold at least INET_ADDRSTRLEN characters
 * for IPv4 addresses and INET6_ADDRSTRLEN characters for IPv6, so generally
 * it should always be as large as the latter.  Returns success or failure.
 */
bool
network_sockaddr_sprint(char *dst, size_t size, const struct sockaddr *addr)
{
    const char *result;

#ifdef HAVE_INET6
    if (addr->sa_family == AF_INET6) {
        const struct sockaddr_in6 *sin6;

        sin6 = (const struct sockaddr_in6 *) (const void *) addr;
        if (IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr)) {
            struct in_addr in;

            memcpy(&in, sin6->sin6_addr.s6_addr + 12, sizeof(in));
            result = inet_ntop(AF_INET, &in, dst, size);
        } else
            result = inet_ntop(AF_INET6, &sin6->sin6_addr, dst, size);
        return (result != NULL);
    }
#endif
    if (addr->sa_family == AF_INET) {
        const struct sockaddr_in *sin;

        sin = (const struct sockaddr_in *) (const void *) addr;
        result = inet_ntop(AF_INET, &sin->sin_addr, dst, size);
        return (result != NULL);
    } else {
        socket_set_errno(EAFNOSUPPORT);
        return false;
    }
}


/*
 * Compare the addresses from two sockaddrs and see whether they're equal.
 * IPv4 addresses that have been mapped to IPv6 addresses compare equal to the
 * corresponding IPv4 address.
 */
bool
network_sockaddr_equal(const struct sockaddr *a, const struct sockaddr *b)
{
    const struct sockaddr_in *a4;
    const struct sockaddr_in *b4;
#ifdef HAVE_INET6
    const struct sockaddr_in6 *a6;
    const struct sockaddr_in6 *b6;
    const struct sockaddr *tmp;
#endif

    a4 = (const struct sockaddr_in *) (const void *) a;
    b4 = (const struct sockaddr_in *) (const void *) b;

#ifdef HAVE_INET6
    a6 = (const struct sockaddr_in6 *) (const void *) a;
    b6 = (const struct sockaddr_in6 *) (const void *) b;
    if (a->sa_family == AF_INET && b->sa_family == AF_INET6) {
        tmp = a;
        a = b;
        b = tmp;
        a6 = (const struct sockaddr_in6 *) (const void *) a;
        b4 = (const struct sockaddr_in *) (const void *) b;
    }
    if (a->sa_family == AF_INET6) {
        if (b->sa_family == AF_INET6)
            return IN6_ARE_ADDR_EQUAL(&a6->sin6_addr, &b6->sin6_addr);
        else if (b->sa_family != AF_INET)
            return false;
        else if (!IN6_IS_ADDR_V4MAPPED(&a6->sin6_addr))
            return false;
        else {
            struct in_addr in;

            memcpy(&in, a6->sin6_addr.s6_addr + 12, sizeof(in));
            return (in.s_addr == b4->sin_addr.s_addr);
        }
    }
#endif

    if (a->sa_family != AF_INET || b->sa_family != AF_INET)
        return false;
    return (a4->sin_addr.s_addr == b4->sin_addr.s_addr);
}


/*
 * Returns the port of a sockaddr or 0 on error.
 */
unsigned short
network_sockaddr_port(const struct sockaddr *sa)
{
    const struct sockaddr_in *sin;

#ifdef HAVE_INET6
    const struct sockaddr_in6 *sin6;

    if (sa->sa_family == AF_INET6) {
        sin6 = (const struct sockaddr_in6 *) (const void *) sa;
        return htons(sin6->sin6_port);
    }
#endif
    if (sa->sa_family != AF_INET)
        return 0;
    else {
        sin = (const struct sockaddr_in *) (const void *) sa;
        return htons(sin->sin_port);
    }
}


/*
 * Compare two addresses given as strings, applying an optional mask.  Returns
 * true if the addresses are equal modulo the mask and false otherwise,
 * including on syntax errors in the addresses or mask specification.
 */
bool
network_addr_match(const char *a, const char *b, const char *mask)
{
    struct in_addr a4, b4, tmp;
    unsigned long cidr;
    char *end;
    unsigned int i;
    unsigned long bits, addr_mask;
#ifdef HAVE_INET6
    struct in6_addr a6, b6;
#endif

    /*
     * AIX 7.1 treats the empty string as equivalent to 0.0.0.0 and allows it
     * to match, but it's too easy to get the empty string from some sort of
     * syntax error.  Special-case the empty string to always return false.
     */
    if (a[0] == '\0' || b[0] == '\0')
        return false;

    /*
     * If the addresses are IPv4, the mask may be in one of two forms.  It can
     * either be a traditional mask, like 255.255.0.0, or it can be a CIDR
     * subnet designation, like 16.  (The caller should have already removed
     * the slash separating it from the address.)
     */
    if (inet_aton(a, &a4) && inet_aton(b, &b4)) {
        if (mask == NULL)
            addr_mask = htonl(0xffffffffUL);
        else if (strchr(mask, '.') == NULL) {
            cidr = strtoul(mask, &end, 10);
            if (cidr > 32 || *end != '\0')
                return false;
            for (bits = 0, i = 0; i < cidr; i++)
                bits |= (1UL << (31 - i));
            addr_mask = htonl(bits);
        } else if (inet_aton(mask, &tmp))
            addr_mask = tmp.s_addr;
        else
            return false;
        return (a4.s_addr & addr_mask) == (b4.s_addr & addr_mask);
    }
            
#ifdef HAVE_INET6
    /*
     * Otherwise, if the address is IPv6, the mask is required to be a CIDR
     * subnet designation.
     */
    if (!inet_pton(AF_INET6, a, &a6) || !inet_pton(AF_INET6, b, &b6))
        return false;
    if (mask == NULL)
        cidr = 128;
    else {
        cidr = strtoul(mask, &end, 10);
        if (cidr > 128 || *end != '\0')
            return false;
    }
    for (i = 0; i * 8 < cidr; i++) {
        if ((i + 1) * 8 <= cidr) {
            if (a6.s6_addr[i] != b6.s6_addr[i])
                return false;
        } else {
            for (addr_mask = 0, bits = 0; bits < cidr % 8; bits++)
                addr_mask |= (1UL << (7 - bits));
            if ((a6.s6_addr[i] & addr_mask) != (b6.s6_addr[i] & addr_mask))
                return false;
        }
    }
    return true;
#else
    return false;
#endif
}
