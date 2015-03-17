/* $Id$
 *
 * Prototypes for message and error reporting (possibly fatal).
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2008, 2010, 2013, 2014
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

#ifndef INN_MESSAGES_H
#define INN_MESSAGES_H 1

#include <inn/defines.h>
#include "inn/portable-macros.h"

#include <stdarg.h>
#include <stddef.h>

BEGIN_DECLS

/*
 * The reporting functions.  The ones prefaced by "sys" add a colon, a space,
 * and the results of strerror(errno) to the output and are intended for
 * reporting failures of system calls.
 */
void debug(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void notice(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void sysnotice(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void warn(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void syswarn(const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 1, 2)));
void die(const char *, ...)
    __attribute__((__nonnull__, __noreturn__, __format__(printf, 1, 2)));
void sysdie(const char *, ...)
    __attribute__((__nonnull__, __noreturn__, __format__(printf, 1, 2)));

/*
 * Set the handlers for various message functions.  All of these functions
 * take a count of the number of handlers and then function pointers for each
 * of those handlers.  These functions are not thread-safe; they set global
 * variables.
 */
void message_handlers_debug(unsigned int count, ...);
void message_handlers_notice(unsigned int count, ...);
void message_handlers_warn(unsigned int count, ...);
void message_handlers_die(unsigned int count, ...);

/*
 * Reset all message handlers back to the defaults and free any memory that
 * was allocated by the other message_handlers_* functions.
 */
void message_handlers_reset(void);

/*
 * Some useful handlers, intended to be passed to message_handlers_*.  All
 * handlers take the length of the formatted message, the format, a variadic
 * argument list, and the errno setting if any.
 */
void message_log_stdout(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_stderr(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_syslog_debug(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_syslog_info(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_syslog_notice(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_syslog_warning(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_syslog_err(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));
void message_log_syslog_crit(size_t, const char *, va_list, int)
    __attribute__((__nonnull__));

/* The type of a message handler. */
typedef void (*message_handler_func)(size_t, const char *, va_list, int);

/* If non-NULL, called before exit and its return value passed to exit. */
extern int (*message_fatal_cleanup)(void);

/*
 * If non-NULL, prepended (followed by ": ") to all messages printed by either
 * message_log_stdout or message_log_stderr.
 */
extern const char *message_program_name;

END_DECLS

#endif /* INN_MESSAGES_H */
