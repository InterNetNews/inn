/*  $Id$
**
**  Portability wrapper around <sys/socket.h> and friends.
**
**  This header file is the equivalent of:
**
**      #include <arpa/inet.h>
**      #include <netinet/in.h>
**      #include <netdb.h>
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
#include <netdb.h>
#include <sys/socket.h>

/* Pick up a definition of getaddrinfo if not otherwise available. */
#include "portable/getaddrinfo.h"

BEGIN_DECLS

/* Provide prototypes for inet_aton and inet_ntoa if not prototyped in the
   system header files since they're occasionally available without proper
   prototypes. */
#if NEED_DECLARATION_INET_ATON
extern int              inet_aton(const char *, struct in_addr *);
#endif
#if NEED_DECLARATION_INET_NTOA
extern const char *     inet_ntoa(const struct in_addr);
#endif

/* Some systems don't define INADDR_LOOPBACK. */
#ifndef INADDR_LOOPBACK
# define INADDR_LOOPBACK 0x7f000001UL
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

/* Make sure we have access to h_errno and hstrerror to print out name
   resolution error messages. */
#if NEED_HERRNO_DECLARATION
extern int h_errno;
#endif
#if !HAVE_HSTRERROR
extern const char *hstrerror(int);
#endif

/* The netdb constants, which aren't always defined (particularly if h_errno
   isn't declared.  We also make sure that a few of the less-used ones are
   defined so that we can deal with them in case statements. */
#ifndef NETDB_SUCCESS
# define NETDB_SUCCESS  0
#endif
#ifndef HOST_NOT_FOUND
# define HOST_NOT_FOUND 1
# define TRY_AGAIN      2
# define NO_RECOVERY    3
# define NO_DATA        4
#endif

/* POSIX requires AI_ADDRCONFIG and AI_NUMERICSERV, but some implementations
   don't have them yet.  It's only used in a bitwise OR of flags, so defining
   them to 0 makes them harmlessly go away. */
#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif
#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0
#endif

END_DECLS

#endif /* PORTABLE_SOCKET_H */
