/*
 * Prototypes for network connection utility functions using innbind.
 *
 * This file is heavily based on include/inn/network.h.
 */

#ifndef INN_NETWORK_INNBIND_H
#define INN_NETWORK_INNBIND_H 1

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
socket_type network_innbind_ipv4(int type, const char *addr,
                                 unsigned short port)
    __attribute__((__nonnull__));
socket_type network_innbind_ipv6(int type, const char *addr,
                                 unsigned short port)
    __attribute__((__nonnull__));

/*
 * Create and bind sockets of the given type for every local address (normally
 * two, one for IPv4 and one for IPv6, if IPv6 support is enabled).  If IPv6
 * is not enabled, just one socket will be created and bound to the IPv4
 * wildcard address.  Returns true on success and false (setting errno) on
 * failure.
 *
 * fds will be set to an array containing the resulting file descriptors, with
 * count holding the count returned.
 */
bool network_innbind_all(int type, unsigned short port, socket_type **fds,
                         unsigned int *count) __attribute__((__nonnull__));

END_DECLS

#endif /* INN_NETWORK_INNBIND_H */
