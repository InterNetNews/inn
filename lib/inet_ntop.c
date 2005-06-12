/*  $Id$
**
**  Replacement for a missing inet_ntop.
**
**  Written by Russ Allbery <rra@stanford.edu>
**  This work is hereby placed in the public domain by its author.
**
**  Provides an implementation of inet_ntop that only supports IPv4 addresses
**  for hosts that are missing it.  If you want IPv6 support, you need to have
**  a real inet_ntop function; this function is only provided so that code can
**  call inet_ntop unconditionally without needing to worry about whether the
**  host supports IPv6.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <netinet/in.h>

/* This may already be defined by the system headers. */
#ifndef INET_ADDRSTRLEN
# define INET_ADDRSTRLEN 16
#endif

/* Systems old enough to not support inet_ntop may not have this either. */
#ifndef EAFNOSUPPORT
# define EAFNOSUPPORT EDOM
#endif

/* If we're running the test suite, rename inet_ntop to avoid conflicts with
   the system version. */
#if TESTING
# define inet_ntop test_inet_ntop
const char *test_inet_ntop(int, const void *, char *, socklen_t);
#endif

const char *
inet_ntop(int af, const void *src, char *dst, socklen_t cnt)
{
    const unsigned char *p;

    if (af != AF_INET) {
        errno = EAFNOSUPPORT;
        return NULL;
    }
    if (cnt < INET_ADDRSTRLEN) {
        errno = ENOSPC;
        return NULL;
    }
    p = src;
    snprintf(dst, cnt, "%u.%u.%u.%u",
             (unsigned int) (p[0] & 0xff), (unsigned int) (p[1] & 0xff),
             (unsigned int) (p[2] & 0xff), (unsigned int) (p[3] & 0xff));
    return dst;
}
