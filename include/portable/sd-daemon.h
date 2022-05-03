/*
 * Portability wrapper around systemd-daemon headers.
 *
 * Currently, only sd_is_socket, sd_listen_fds, sd_notify, and sd_notifyf are
 * guaranteed to be provided by this interface.  This takes the approach of
 * stubbing out these functions if the libsystemd-daemon library is not
 * available, rather than providing a local implementation, on the grounds
 * that anyone who wants systemd status reporting should be able to build with
 * the systemd libraries.  In particular, sd_is_socket always returns false
 * and does not perform its intended function.
 *
 * The stubs for sd_notify and sd_notifyf provided as functions, rather than
 * using #define, to allow for the variadic function and to avoid compiler
 * warnings when they are used without assigning the return value as is
 * recommended in the sd_notify(3) man page.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2021-2022 Russ Allbery <eagle@eyrie.org>
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

#include "config.h"
#include "portable/macros.h"

BEGIN_DECLS

#ifdef HAVE_SD_NOTIFY
#    include <systemd/sd-daemon.h>
#else
#    define SD_LISTEN_FDS_START       3
#    define sd_is_socket(fd, f, t, l) 0
#    define sd_listen_fds(u)          0
int sd_notify(int, const char *);
int sd_notifyf(int, const char *, ...);
#endif

END_DECLS

#endif /* !PORTABLE_SD_DAEMON_H */
