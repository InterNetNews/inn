/*  $Id$
**
**  Utility functions for network connections.
**
**  This is a collection of utility functions for network connections and
**  socket creation, encapsulating some of the complexities of IPv4 and IPv6
**  support and abstracting operations common to most network code.
*/

#ifndef INN_NETWORK_H
#define INN_NETWORK_H 1

#include <inn/defines.h>
#include <sys/types.h>          /* socklen_t */

/* Forward declarations to avoid unnecessary includes. */
struct addrinfo;
struct sockaddr;

/* We don't want to accidentally use IPv6 if we were built without it, even if
   we end up using the system getaddrinfo with IPv6 support.  We can do that
   by hinting getaddrinfo away from returning IPv6 addresses.  We set this
   constant to AF_UNSPEC if we have IPv6 support or AF_INET if we don't and
   then use it in getaddrinfo hints. */
#if INN_HAVE_INET6
# define NETWORK_AF_HINT AF_UNSPEC
#else
# define NETWORK_AF_HINT AF_INET
#endif

BEGIN_DECLS

/* Create a socket and bind it to the specified address and port (either IPv4
   or IPv6), returning the resulting file descriptor or -1 on error.  Errors
   are reported using warn/syswarn.  To bind to all interfaces, use "any" or
   "all" for address. */
int network_bind_ipv4(const char *address, unsigned short port);
int network_bind_ipv6(const char *address, unsigned short port);

/* Create and bind sockets for every local address (normally two, one for IPv4
   and one for IPv6, if IPv6 support is enabled).  If IPv6 is not enabled,
   just one socket will be created and bound to the IPv4 wildcard address.
   fds will be set to an array containing the resulting file descriptors, with
   count holding the count returned. */
void network_bind_all(unsigned short port, int **fds, int *count);

/* Create a socket and connect it to the remote service given by the linked
   list of addrinfo structs.  Returns the new file descriptor on success and
   -1 on failure, with the error left in errno.  Takes an optional source
   address; if not provided and inn.conf has been read, the source address is
   set based on innconf->sourceaddress and innconf->sourceaddress6. */
int network_connect(struct addrinfo *, const char *source);

/* Like network_connect but takes a host and port instead.  If host lookup
   fails, errno may not be set to anything useful. */
int network_connect_host(const char *host, unsigned short port,
                         const char *source);

/* Creates a socket of the specified domain and type and binds it to the
   appropriate source address, either the one supplied or the appropriate
   innconf setting if the provided source address is NULL.  To bind to all
   interfaces, use "all" for address.  Returns the newly created file
   descriptor or -1 on error.

   This is a lower-level function intended primarily for the use of clients
   that will then go on to do a non-blocking connect. */
int network_client_create(int domain, int type, const char *source);

/* Kill IP options (like source routing) if possible.  Returns false only when
   IP options have been found but clearing them failed. */
bool network_kill_options(int fd, struct sockaddr *remote);

/* Put an ASCII representation of the address in a sockaddr into the provided
   buffer, which should hold at least INET6_ADDRSTRLEN characters. */
bool network_sockaddr_sprint(char *, size_t, const struct sockaddr *);

/* Returns if the addresses from the two sockaddrs are equal.  The ports are
   ignored, and only AF_INET or AF_INET6 sockaddrs are supported (all others
   will return false). */
bool network_sockaddr_equal(const struct sockaddr *, const struct sockaddr *);

/* Returns the port number from a sockaddr. */
unsigned short network_sockaddr_port(const struct sockaddr *);

END_DECLS

#endif /* INN_NETWORK_H */
