/*
 * Portability wrapper around <sys/un.h>.
 *
 * This wrapper exists primarily to define SUN_LEN if not defined by the
 * implementation.  In most cases, one will want to include portable/socket.h
 * as well for the normal socket functions.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2014 Russ Allbery <eagle@eyrie.org>
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_SOCKET_UNIX_H
#define PORTABLE_SOCKET_UNIX_H 1

#include "config.h"
#include <sys/un.h>

/*
 * POSIX.1g requires <sys/un.h> to define a SUN_LEN macro for determining
 * the real length of a struct sockaddr_un, but it's not available
 * everywhere yet.  If autoconf couldn't find it, define our own.  This
 * definition is from 4.4BSD by way of Stevens, Unix Network Programming
 * (2nd edition), vol. 1, pg. 917.
 */
#if !HAVE_SUN_LEN
#    define SUN_LEN(sun) \
        (sizeof(*(sun)) - sizeof((sun)->sun_path) + strlen((sun)->sun_path))
#endif

#endif /* !PORTABLE_SOCKET_UNIX_H */
