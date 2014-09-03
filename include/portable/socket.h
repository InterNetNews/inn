/* $Id$
 *
 * Portability wrapper around <sys/socket.h> and friends.
 *
 * This header file is the equivalent of:
 *
 *     #include <arpa/inet.h>
 *     #include <netinet/in.h>
 *     #include <netdb.h>
 *     #include <sys/socket.h>
 *
 * but also cleans up various messes, mostly related to IPv6 support, and
 * provides a set of portability interfaces that work on both UNIX and
 * Windows.  It ensures that inet_aton, inet_ntoa, and inet_ntop are available
 * and properly prototyped.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2014 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2009, 2011, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2004, 2005, 2006, 2007
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

#ifndef PORTABLE_SOCKET_H
#define PORTABLE_SOCKET_H 1

#include "config.h"

#include <errno.h>
#include <sys/types.h>

/* BSDI needs <netinet/in.h> before <arpa/inet.h>. */
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netinet/in.h>
# include <arpa/inet.h>
# include <netdb.h>
# include <sys/socket.h>
#endif

/*
 * Pick up definitions of getaddrinfo and getnameinfo if not otherwise
 * available.
 */
#include <portable/getaddrinfo.h>
#include <portable/getnameinfo.h>

/* Define socklen_t if it's not available in sys/socket.h. */
#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

/*
 * Defined by RFC 3493, used to store a generic address.  All of the extra
 * goop here is to ensure that the structs are appropriately aligned on
 * platforms that may require 64-bit alignment for the embedded addresses.
 */
#if !HAVE_STRUCT_SOCKADDR_STORAGE
# define SS_MAXSIZE_ 128
# ifdef HAVE_LONG_LONG_INT
#  define SS_ALIGNSIZE_ sizeof(long long)
#  define SS_ALIGNTYPE_ long long
# else
#  define SS_ALIGNSIZE_ sizeof(long)
#  define SS_ALIGNTYPE_ long
# endif
# if HAVE_STRUCT_SOCKADDR_SA_LEN
#  define SS_PAD1SIZE_ (SS_ALIGNSIZE_ - 2 * sizeof(unsigned char))
#  define SS_PAD2SIZE_ \
    (SS_MAXSIZE_ - (2 * sizeof(unsigned char) + SS_PAD1SIZE_ + SS_ALIGNSIZE_))
struct sockaddr_storage {
    unsigned char ss_len;
    unsigned char ss_family;
    char __ss_pad1[SS_PAD1SIZE_];
    SS_ALIGNTYPE_ __ss_align;
    char __ss_pad2[SS_PAD2SIZE_];
};
# else
#  define SS_PAD1SIZE_ (SS_ALIGNSIZE_ - sizeof(unsigned char))
#  define SS_PAD2SIZE_ \
    (SS_MAXSIZE_ - (sizeof(unsigned char) + SS_PAD1SIZE_ + SS_ALIGNSIZE_))
struct sockaddr_storage {
    unsigned short ss_family;
    char __ss_pad1[SS_PAD1SIZE_];
    SS_ALIGNTYPE_ __ss_align;
    char __ss_pad2[SS_PAD2SIZE_];
};
# endif
#endif

/*
 * RFC 2553 used underscores, so some old implementations may have that
 * instead of the non-uglified names from RFC 3493.
 */
#if HAVE_STRUCT_SOCKADDR_STORAGE && !HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY
# define ss_family __ss_family
# define ss_len    __ss_len
#endif

/* Fix IN6_ARE_ADDR_EQUAL if required. */
#ifdef HAVE_BROKEN_IN6_ARE_ADDR_EQUAL
# undef IN6_ARE_ADDR_EQUAL
# define IN6_ARE_ADDR_EQUAL(a, b) \
    (memcmp((a), (b), sizeof(struct in6_addr)) == 0)
#endif

/* Define an SA_LEN macro that gives us the length of a sockaddr. */
#if !HAVE_SA_LEN
# if HAVE_STRUCT_SOCKADDR_SA_LEN
#  define SA_LEN(s)     ((s)->sa_len)
# else
/* Hack courtesy of the USAGI project. */
#  if HAVE_INET6
#   define SA_LEN(s) \
    ((((const struct sockaddr *)(s))->sa_family == AF_INET6)            \
        ? sizeof(struct sockaddr_in6)                                   \
        : ((((const struct sockaddr *)(s))->sa_family == AF_INET)       \
            ? sizeof(struct sockaddr_in)                                \
            : sizeof(struct sockaddr)))
#  else
#   define SA_LEN(s) \
    ((((const struct sockaddr *)(s))->sa_family == AF_INET)             \
        ? sizeof(struct sockaddr_in)                                    \
        : sizeof(struct sockaddr))
#  endif
# endif /* HAVE_SOCKADDR_LEN */
#endif /* !HAVE_SA_LEN_MACRO */

/*
 * AI_ADDRCONFIG results in an error from getaddrinfo on BSD/OS and possibly
 * other platforms.  If configure determined it didn't work, pretend it
 * doesn't exist.
 */
#if !defined(HAVE_GETADDRINFO_ADDRCONFIG) && defined(AI_ADDRCONFIG)
# undef AI_ADDRCONFIG
#endif

/*
 * POSIX requires AI_ADDRCONFIG and AI_NUMERICSERV, but some implementations
 * don't have them yet.  We also may have hidden AI_ADDRCONFIG if it doesn't
 * work.  It's only used in a bitwise OR of flags, so defining them to 0 makes
 * them harmlessly go away.
 */
#ifndef AI_ADDRCONFIG
# define AI_ADDRCONFIG 0
#endif
#ifndef AI_NUMERICSERV
# define AI_NUMERICSERV 0
#endif

/*
 * Constants required by the IPv6 API.  The buffer size required to hold any
 * nul-terminated text representation of the given address type.
 */
#ifndef INET_ADDRSTRLEN
# define INET_ADDRSTRLEN 16
#endif
#ifndef INET6_ADDRSTRLEN
# define INET6_ADDRSTRLEN 46
#endif

/*
 * This is one of the defined error codes from inet_ntop, but it may not be
 * available on systems too old to have that function.
 */
#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EDOM
#endif

/*
 * EAI_ADDRFAMILY was made obsolete by RFC 3493, but it may still be used by
 * obsolete IPv6 stacks and may be distinct from EAI_FAMILY.  Define it so
 * that code that needs to handle this case can compare against it
 * unconditionally.
 */
#ifndef EAI_ADDRFAMILY
# define EAI_ADDRFAMILY EAI_FAMILY
#endif

BEGIN_DECLS

/*
 * Provide prototypes for inet_aton and inet_ntoa if not prototyped in the
 * system header files since they're occasionally available without proper
 * prototypes.  If we're providing a replacement, be sure to set visibility
 * accordingly.
 */
#if !HAVE_DECL_INET_ATON
# if !HAVE_INET_ATON
extern int inet_aton(const char *, struct in_addr *)
    __attribute__((__visibility__("hidden")));
# else
extern int inet_aton(const char *, struct in_addr *);
# endif
#endif
#if !HAVE_DECL_INET_NTOA
# if !HAVE_INET_NTOA
extern const char *inet_ntoa(const struct in_addr)
    __attribute__((__visibility__("hidden")));
# else
extern const char *inet_ntoa(const struct in_addr);
# endif
#endif

#if !HAVE_INET_NTOP
# ifdef _WIN32
extern const char *inet_ntop(int, const void *, char *, int);
# else
extern const char *inet_ntop(int, const void *, char *, socklen_t);
# endif
#endif

/*
 * Used for portability to Windows, which requires different functions be
 * called to close sockets, send data to or read from sockets, and get socket
 * errors than the regular functions and variables.  Windows also uses SOCKET
 * to store socket descriptors instead of an int.
 *
 * socket_init must be called before socket functions are used and
 * socket_shutdown at the end of the program.  socket_init may return failure,
 * but this interface doesn't have a way to retrieve the exact error.
 *
 * socket_close, socket_read, and socket_write must be used instead of the
 * standard functions.  On Windows, closesocket must be called instead of
 * close for sockets and recv and send must always be used instead of read and
 * write.
 *
 * When reporting errors from socket functions, use socket_errno and
 * socket_strerror instead of errno and strerror.  When setting errno to
 * something for socket errors (to preserve errors through close, for
 * example), use socket_set_errno instead of just assigning to errno.
 *
 * Socket file descriptors must be passed and stored in variables of type
 * socket_type rather than an int.  Use INVALID_SOCKET for invalid socket file
 * descriptors rather than -1, and compare to INVALID_SOCKET when testing
 * whether operations succeed.
 */
#ifdef _WIN32
int socket_init(void);
# define socket_shutdown()      WSACleanup()
# define socket_close(fd)       closesocket(fd)
# define socket_read(fd, b, s)  recv((fd), (b), (s), 0)
# define socket_write(fd, b, s) send((fd), (b), (s), 0)
# define socket_errno           WSAGetLastError()
# define socket_set_errno(e)    WSASetLastError(e)
const char *socket_strerror(int);
typedef SOCKET socket_type;
#else
# define socket_init()          1
# define socket_shutdown()      /* empty */
# define socket_close(fd)       close(fd)
# define socket_read(fd, b, s)  read((fd), (b), (s))
# define socket_write(fd, b, s) write((fd), (b), (s))
# define socket_errno           errno
# define socket_set_errno(e)    errno = (e)
# define socket_strerror(e)     strerror(e)
# define INVALID_SOCKET         -1
typedef int socket_type;
#endif

END_DECLS

#endif /* !PORTABLE_SOCKET_H */
