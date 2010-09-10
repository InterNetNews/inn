/*  $Id$
**
**  Replacement implementation of getnameinfo.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  This is an implementation of the getnameinfo function for systems that
**  lack it, so that code can use getnameinfo always.  It provides IPv4
**  support only; for IPv6 support, a native getnameinfo implemenation is
**  required.
**
**  This file should generally be included by way of portable/socket.h rather
**  than directly.
*/

#ifndef PORTABLE_GETNAMEINFO_H
#define PORTABLE_GETNAMEINFO_H 1

#include "config.h"

/* Skip this entire file if a system getaddrinfo was detected. */
#if !HAVE_GETNAMEINFO

#include <sys/types.h>
#include <sys/socket.h>

/* Constants for flags from RFC 3493, combined with binary or. */
#define NI_NOFQDN       0x0001
#define NI_NUMERICHOST  0x0002
#define NI_NAMEREQD     0x0004
#define NI_NUMERICSERV  0x0008
#define NI_DGRAM        0x0010

/* Maximum length of hostnames and service names.  Our implementation doesn't
   use these values, so they're taken from Linux.  They're provided just for
   code that uses them to size buffers. */
#ifndef NI_MAXHOST
# define NI_MAXHOST     1025
#endif
#ifndef NI_MAXSERV
# define NI_MAXSERV     32
#endif

BEGIN_DECLS

int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *node, socklen_t nodelen,
                char *service, socklen_t servicelen, int flags);

END_DECLS

#endif /* !HAVE_GETNAMEINFO */
#endif /* !PORTABLE_GETNAMEINFO_H */
