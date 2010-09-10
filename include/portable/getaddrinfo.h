/*  $Id$
**
**  Replacement implementation of getaddrinfo.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  This is an implementation of the getaddrinfo family of functions for
**  systems that lack it, so that code can use getaddrinfo always.  It
**  provides IPv4 support only; for IPv6 support, a native getaddrinfo
**  implemenation is required.
**
**  This file should generally be included by way of portable/socket.h rather
**  than directly.
*/

#ifndef PORTABLE_GETADDRINFO_H
#define PORTABLE_GETADDRINFO_H 1

#include "config.h"

/* Skip this entire file if a system getaddrinfo was detected. */
#if !HAVE_GETADDRINFO

#include <sys/types.h>
#include <sys/socket.h>

/* The struct returned by getaddrinfo, from RFC 3493. */
struct addrinfo {
    int ai_flags;               /* AI_PASSIVE, AI_CANONNAME, .. */
    int ai_family;              /* AF_xxx */
    int ai_socktype;            /* SOCK_xxx */
    int ai_protocol;            /* 0 or IPPROTO_xxx for IPv4 and IPv6 */
    socklen_t ai_addrlen;       /* Length of ai_addr */
    char *ai_canonname;         /* Canonical name for nodename */
    struct sockaddr *ai_addr;   /* Binary address */
    struct addrinfo *ai_next;   /* Next structure in linked list */
};

/* Constants for ai_flags from RFC 3493, combined with binary or. */
#define AI_PASSIVE      0x0001
#define AI_CANONNAME    0x0002
#define AI_NUMERICHOST  0x0004
#define AI_NUMERICSERV  0x0008
#define AI_V4MAPPED     0x0010
#define AI_ALL          0x0020
#define AI_ADDRCONFIG   0x0040

/* Error return codes from RFC 3493. */
#define EAI_AGAIN       1       /* Temporary name resolution failure */
#define EAI_BADFLAGS    2       /* Invalid value in ai_flags parameter */
#define EAI_FAIL        3       /* Permanent name resolution failure */
#define EAI_FAMILY      4       /* Address family not recognized */
#define EAI_MEMORY      5       /* Memory allocation failure */
#define EAI_NONAME      6       /* nodename or servname unknown */
#define EAI_SERVICE     7       /* Service not recognized for socket type */
#define EAI_SOCKTYPE    8       /* Socket type not recognized */
#define EAI_SYSTEM      9       /* System error occurred, see errno */
#define EAI_OVERFLOW    10      /* An argument buffer overflowed */

BEGIN_DECLS

/* Function prototypes. */
int getaddrinfo(const char *nodename, const char *servname,
                const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *ai);
const char *gai_strerror(int ecode);

END_DECLS

#endif /* !HAVE_GETADDRINFO */
#endif /* !PORTABLE_GETADDRINFO_H */
