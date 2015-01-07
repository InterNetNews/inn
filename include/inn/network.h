/* $Id$
 *
 * Prototypes for network connection utility functions.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2010, 2011, 2012, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2004, 2005, 2006, 2007, 2008, 2010
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

#ifndef INN_NETWORK_H
#define INN_NETWORK_H 1

#include <inn/defines.h>
#include "inn/portable-macros.h"
#include "inn/portable-socket.h"
#include "inn/portable-stdbool.h"

#include <sys/types.h>

BEGIN_DECLS

/*
 * Create a socket of the given type and bind it to the specified address and
 * port (either IPv4 or IPv6), returning the resulting file descriptor or
 * INVALID_SOCKET on error.  Errors are reported using warn/syswarn.  To bind
 * to all interfaces, use "any" or "all" for address.
 */
socket_type network_bind_ipv4(int type, const char *addr, unsigned short port)
    __attribute__((__nonnull__));
socket_type network_bind_ipv6(int type, const char *addr, unsigned short port)
    __attribute__((__nonnull__));

/*
 * Create and bind sockets of the given type for every local address (normally
 * two, one for IPv4 and one for IPv6, if IPv6 support is enabled).  If IPv6
 * is not enabled, just one socket will be created and bound to the IPv4
 * wildcard address.  Returns true on success and false (setting errno) on
 * failure.
 *
 * fds will be set to an array containing the resulting file descriptors, with
 * count holding the count returned.  Use network_bind_all_free to free the
 * array of file descriptors when no longer needed.
 */
bool network_bind_all(int type, unsigned short port, socket_type **fds,
                      unsigned int *count)
    __attribute__((__nonnull__));
void network_bind_all_free(socket_type *fds);

/*
 * Wait on an array of file descriptor for one of them to select ready for
 * read, and return the first file descriptor that does so.  This is primarily
 * intended for UDP services listening on multiple file descriptors.  TCP
 * services will probably want to use network_accept_any instead.
 *
 * This is not intended to be a replacement for a full event loop, just some
 * simple shared code for UDP services.
 */
socket_type network_wait_any(socket_type fds[], unsigned int count)
    __attribute__((__nonnull__));

/*
 * Accept an incoming connection from any file descriptor in an array.  This
 * is a blocking accept that will wait until there is an incoming connection,
 * unless interrupted by receipt of a signal.  Returns the new socket or
 * INVALID_SOCKET, and fills out the arguments with the address of the remote
 * client.  If the wait for a connection was interrupted by a signal, returns
 * INVALID_SOCKET with errno set to EINTR.
 */
socket_type network_accept_any(socket_type fds[], unsigned int count,
                               struct sockaddr *addr, socklen_t *addrlen)
    __attribute__((__nonnull__(1)));

/*
 * Create a socket and connect it to the remote service given by the linked
 * list of addrinfo structs.  Returns the new file descriptor on success and
 * INVALID_SOCKET on failure, with the error left in errno.  Takes an optional
 * source address and a timeout in seconds, which may be 0 for no timeout.
 * (Source may also be "all" or "any", which mean the same thing as NULL: do
 * not use any particular source address.)
 */
socket_type network_connect(const struct addrinfo *, const char *source,
                            time_t)
    __attribute__((__nonnull__(1)));

/*
 * Like network_connect but takes a host and port instead.  If host lookup
 * fails, errno may not be set to anything useful.
 */
socket_type network_connect_host(const char *host, unsigned short port,
                                 const char *source, time_t)
    __attribute__((__nonnull__(1)));

/*
 * Creates a socket of the specified domain and type and binds it to the
 * appropriate source address, either the one supplied or all addresses if the
 * source address is NULL, "all", or "any".  Returns the newly created file
 * descriptor or INVALID_SOCKET on error.
 *
 * This is a lower-level function intended primarily for the use of clients
 * that will then go on to do a non-blocking connect.
 */
socket_type network_client_create(int domain, int type, const char *source);

/*
 * Read or write the specified number of bytes to the network, enforcing a
 * timeout.  Both return true on success and false on failure; on failure, the
 * socket errno is set.
 *
 * network_write will set the file descriptor non-blocking and then set it
 * back to blocking at the conclusion of the write, so don't use this function
 * with file descriptors that should stay non-blocking.
 */
bool network_read(socket_type, void *, size_t, time_t)
    __attribute__((__nonnull__));
bool network_write(socket_type, const void *, size_t, time_t)
    __attribute__((__nonnull__));

/*
 * Put an ASCII representation of the address in a sockaddr into the provided
 * buffer, which should hold at least INET6_ADDRSTRLEN characters.
 */
bool network_sockaddr_sprint(char *, size_t, const struct sockaddr *)
    __attribute__((__nonnull__));

/*
 * Returns if the addresses from the two sockaddrs are equal.  The ports are
 * ignored, and only AF_INET or AF_INET6 sockaddrs are supported (all others
 * will return false).
 */
bool network_sockaddr_equal(const struct sockaddr *, const struct sockaddr *)
    __attribute__((__nonnull__));

/* Returns the port number from a sockaddr. */
unsigned short network_sockaddr_port(const struct sockaddr *)
    __attribute__((__nonnull__));

/*
 * Compare two addresses relative to an optional mask.  Returns true if
 * they're equal, false otherwise or on a parse error.
 */
bool network_addr_match(const char *, const char *, const char *mask)
    __attribute__((__nonnull__(1, 2)));

END_DECLS

#endif /* INN_NETWORK_H */
