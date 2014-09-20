/* $Id$
 *
 * Prototypes for write and writev replacements to handle partial writes.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2008, 2010, 2013
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright (c) 2004, 2005, 2006
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

#ifndef INN_XWRITE_H
#define INN_XWRITE_H 1

#include "config.h"
#include "portable/macros.h"

#include <sys/types.h>

/* Forward declaration to avoid an include. */
struct iovec;

BEGIN_DECLS

/*
 * Like the non-x versions of the same function, but keep writing until either
 * the write is not making progress or there's a real error.  Handle partial
 * writes and EINTR/EAGAIN errors.
 */
ssize_t xpwrite(int fd, const void *buffer, size_t size, off_t offset)
    __attribute__((__nonnull__));
ssize_t xwrite(int fd, const void *buffer, size_t size)
    __attribute__((__nonnull__));
ssize_t xwritev(int fd, const struct iovec *iov, int iovcnt)
    __attribute__((__nonnull__));

END_DECLS

#endif /* INN_XWRITE_H */
