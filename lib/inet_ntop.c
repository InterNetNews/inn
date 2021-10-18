/*
 * Replacement for a missing inet_ntop.
 *
 * Provides an implementation of inet_ntop that only supports IPv4 addresses
 * for hosts that are missing it.  If you want IPv6 support, you need to have
 * a real inet_ntop function; this function is only provided so that code can
 * call inet_ntop unconditionally without needing to worry about whether the
 * host supports IPv6.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2005, 2020 Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011, 2013-2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#include "config.h"
#include "portable/socket.h"
#include "portable/system.h"

#include <errno.h>

/* This may already be defined by the system headers. */
#ifndef INET_ADDRSTRLEN
#    define INET_ADDRSTRLEN 16
#endif

/* Systems old enough to not support inet_ntop may not have this either. */
#ifndef EAFNOSUPPORT
#    define EAFNOSUPPORT EDOM
#endif

/*
 * If we're running the test suite, rename inet_ntop to avoid conflicts with
 * the system version.
 */
#if TESTING
#    undef inet_ntop
#    define inet_ntop test_inet_ntop
const char *test_inet_ntop(int, const void *, char *, socklen_t);
#endif

const char *
inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
    const unsigned char *p;
    int status;

    if (af != AF_INET) {
        socket_set_errno(EAFNOSUPPORT);
        return NULL;
    }
    if (size < INET_ADDRSTRLEN) {
        errno = ENOSPC;
        return NULL;
    }
    p = src;
    /* clang-format off */
    status = snprintf(dst, size, "%u.%u.%u.%u",
                      (unsigned int) (p[0] & 0xff),
                      (unsigned int) (p[1] & 0xff),
                      (unsigned int) (p[2] & 0xff),
                      (unsigned int) (p[3] & 0xff));
    /* clang-format on */
    if (status < 0 || (size_t) status >= (size_t) size)
        return NULL;
    return dst;
}
