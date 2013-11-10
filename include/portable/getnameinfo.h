/* $Id$
 *
 * Replacement implementation of getnameinfo.
 *
 * This is an implementation of the getnameinfo function for systems that lack
 * it, so that code can use getnameinfo always.  It provides IPv4 support
 * only; for IPv6 support, a native getnameinfo implemenation is required.
 *
 * This file should generally be included by way of portable/socket.h rather
 * than directly.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#ifndef PORTABLE_GETNAMEINFO_H
#define PORTABLE_GETNAMEINFO_H 1

#include "config.h"

/* Skip this entire file if a system getaddrinfo was detected. */
#if !HAVE_GETNAMEINFO

/* OpenBSD likes to have sys/types.h included before sys/socket.h. */
#include <sys/types.h>
#include <sys/socket.h>

/* Constants for flags from RFC 3493, combined with binary or. */
#define NI_NOFQDN       0x0001
#define NI_NUMERICHOST  0x0002
#define NI_NAMEREQD     0x0004
#define NI_NUMERICSERV  0x0008
#define NI_DGRAM        0x0010

/*
 * Maximum length of hostnames and service names.  Our implementation doesn't
 * use these values, so they're taken from Linux.  They're provided just for
 * code that uses them to size buffers.
 */
#ifndef NI_MAXHOST
# define NI_MAXHOST     1025
#endif
#ifndef NI_MAXSERV
# define NI_MAXSERV     32
#endif

BEGIN_DECLS

/* Default to a hidden visibility for all portability functions. */
#pragma GCC visibility push(hidden)

/* Function prototypes. */
int getnameinfo(const struct sockaddr *sa, socklen_t salen,
                char *node, socklen_t nodelen,
                char *service, socklen_t servicelen, int flags);

/* Undo default visibility change. */
#pragma GCC visibility pop

END_DECLS

#endif /* !HAVE_GETNAMEINFO */
#endif /* !PORTABLE_GETNAMEINFO_H */
