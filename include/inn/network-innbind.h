/* $Id$
 *
 * Prototypes for network connection utility functions using innbind.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2010, 2011, 2012, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2004, 2005, 2006, 2007, 2008, 2010
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

#ifndef INN_NETWORK_INNBIND_H
#define INN_NETWORK_INNBIND_H 1

#include <inn/defines.h>
#include "portable/macros.h"
#include "portable/socket.h"
#include "portable/stdbool.h"

#include <sys/types.h>

BEGIN_DECLS

/*
 * Create a socket of the given type and bind it to the specified address and
 * port (either IPv4 or IPv6), returning the resulting file descriptor or -1
 * on error.  Errors are reported using warn/syswarn.  To bind to all
 * interfaces, use "any" or "all" for address.
 */
socket_type network_innbind_ipv4(int type, const char *addr, unsigned short port)
    __attribute__((__nonnull__));
socket_type network_innbind_ipv6(int type, const char *addr, unsigned short port)
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
                         unsigned int *count)
    __attribute__((__nonnull__));

END_DECLS

#endif /* INN_NETWORK_INNBIND_H */
