/*
 * Prototypes for setting or clearing file descriptor flags.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2010-2011
 *     The Board of Trustees of the Leland Stanford Junior University
 * Copyright 2004-2006 Internet Systems Consortium, Inc. ("ISC")
 * Copyright 1991, 1994-2003 The Internet Software Consortium and Rich Salz
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
 *
 * SPDX-License-Identifier: ISC
 */

#ifndef INN_FDFLAG_H
#define INN_FDFLAG_H 1

#include "inn/portable-macros.h"
#include "inn/portable-socket.h"
#include "inn/portable-stdbool.h"

BEGIN_DECLS

/*
 * Set a file descriptor close-on-exec or nonblocking.  fdflag_close_exec is
 * not supported on Windows and will always return false.  fdflag_nonblocking
 * is defined to take a socket_type so that it can be supported on Windows.
 * On UNIX systems, you can safely pass in a non-socket file descriptor, but
 * be aware that this will fail to compile on Windows.
 */
bool fdflag_close_exec(int fd, bool flag);
bool fdflag_nonblocking(socket_type fd, bool flag);

END_DECLS

#endif /* INN_FDFLAG_H */
