/*  $Id$
**
**  Portability wrapper around <sys/socket.h> and friends.
**
**  This header file is the equivalent of:
**
**      #include <arpa/inet.h>
**      #include <netinet/in.h>
**      #include <sys/socket.h>
**
**  but also cleans up various messes, mostly related to IPv6 support.  It
**  ensures that inet_aton and inet_ntoa are available and properly
**  prototyped.
*/

#ifndef PORTABLE_SOCKET_H
#define PORTABLE_SOCKET_H 1

#include "config.h"
#include <sys/types.h>

/* BSDI needs <netinet/in.h> before <arpa/inet.h>. */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

/* Provide prototypes for inet_aton and inet_ntoa if not prototyped in the
   system header files since they're occasionally available without proper
   prototypes. */
#if NEED_DECLARATION_INET_ATON
extern int              inet_aton(const char *, struct in_addr *);
#endif
#if NEED_DECLARATION_INET_NTOA
extern const char *     inet_ntoa(const struct in_addr);
#endif

/* Defined by RFC 2553, used to store a generic address.  Note that this
   doesn't do the alignment mangling that RFC 2553 does; it's not clear if
   that should be added.... */
#if !HAVE_SOCKADDR_STORAGE
# if HAVE_SOCKADDR_LEN
struct sockaddr_storage {
    unsigned char ss_len;
    unsigned char ss_family;
    unsigned char __padding[128 - 2];
};
# else
struct sockaddr_storage {
    unsigned short ss_family;
    unsigned char __padding[128 - 2];
};
# endif
#endif

/* Use convenient, non-uglified names for the fields since we use them quite a
   bit in code. */
#if HAVE_2553_STYLE_SS_FAMILY
# define ss_family __ss_family
# define ss_len    __ss_len
#endif

/* Define an SA_LEN macro that gives us the length of a sockaddr. */
#if !HAVE_SA_LEN_MACRO
# if HAVE_SOCKADDR_LEN
#  define SA_LEN(s)     ((s)->sa_len)
# else
/* Hack courtesy of the USAGI project. */
#  if HAVE_INET6
#   define SA_LEN(s) \
    ((((struct sockaddr *)(s))->sa_family == AF_INET6)          \
        ? sizeof(struct sockaddr_in6)                           \
        : ((((struct sockaddr *)(s))->sa_family == AF_INET)     \
            ? sizeof(struct sockaddr_in)                        \
            : sizeof(struct sockaddr)))
#  else
#   define SA_LEN(s) \
    ((((struct sockaddr *)(s))->sa_family == AF_INET)           \
        ? sizeof(struct sockaddr_in)                            \
        : sizeof(struct sockaddr))
#  endif
# endif /* HAVE_SOCKADDR_LEN */
#endif /* !HAVE_SA_LEN_MACRO */

#endif /* PORTABLE_SOCKET_H */
