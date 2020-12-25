/* $Id$
 *
 * Portability wrapper around systemd-daemon headers.
 *
 * Currently, only sd_listen_fds and sd_notify are guaranteed to be provided
 * by this interface.  This takes the approach of stubbing out these functions
 * if the libsystemd-daemon library is not available, rather than providing a
 * local implementation, on the grounds that anyone who wants systemd status
 * reporting should be able to build with the systemd libraries.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_SD_DAEMON_H
#define PORTABLE_SD_DAEMON_H 1

#ifdef HAVE_SD_NOTIFY
#    include <systemd/sd-daemon.h>
#else
#    define SD_LISTEN_FDS_START 3
#    define sd_listen_fds(u)    0
#    define sd_notify(u, s)     0
#endif

#endif /* !PORTABLE_SD_DAEMON_H */
