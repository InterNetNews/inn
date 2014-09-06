/* $Id$
 *
 * Prototypes for malloc routines with failure handling.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2010, 2012, 2013, 2014
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

#ifndef INN_XMALLOC_H
#define INN_XMALLOC_H 1

#include "config.h"
#include "portable/macros.h"

#include <stdarg.h>
#include <stddef.h>

/*
 * The functions are actually macros so that we can pick up the file and line
 * number information for debugging error messages without the user having to
 * pass those in every time.
 */
#define xcalloc(n, size)        x_calloc((n), (size), __FILE__, __LINE__)
#define xmalloc(size)           x_malloc((size), __FILE__, __LINE__)
#define xrealloc(p, size)       x_realloc((p), (size), __FILE__, __LINE__)
#define xstrdup(p)              x_strdup((p), __FILE__, __LINE__)
#define xstrndup(p, size)       x_strndup((p), (size), __FILE__, __LINE__)
#define xvasprintf(p, f, a)     x_vasprintf((p), (f), (a), __FILE__, __LINE__)
#define xreallocarray(p, n, size) \
    x_reallocarray((p), (n), (size), __FILE__, __LINE__)

/*
 * asprintf is a special case since it takes variable arguments.  If we have
 * support for variadic macros, we can still pass in the file and line and
 * just need to put them somewhere else in the argument list than last.
 * Otherwise, just call x_asprintf directly.  This means that the number of
 * arguments x_asprintf takes must vary depending on whether variadic macros
 * are supported.
 */
#ifdef INN_HAVE_C99_VAMACROS
# define xasprintf(p, f, ...) \
    x_asprintf((p), __FILE__, __LINE__, (f), __VA_ARGS__)
#elif INN_HAVE_GNU_VAMACROS
# define xasprintf(p, f, args...) \
    x_asprintf((p), __FILE__, __LINE__, (f), args)
#else
# define xasprintf x_asprintf
#endif

BEGIN_DECLS

/*
 * Last two arguments are always file and line number.  These are internal
 * implementations that should not be called directly.
 */
void *x_calloc(size_t, size_t, const char *, int)
    __attribute__((__alloc_size__(1, 2), __malloc__, __nonnull__));
void *x_malloc(size_t, const char *, int)
    __attribute__((__alloc_size__(1), __malloc__, __nonnull__));
void *x_realloc(void *, size_t, const char *, int)
    __attribute__((__alloc_size__(2), __malloc__, __nonnull__(3)));
void *x_reallocarray(void *, size_t, size_t, const char *, int)
    __attribute__((__alloc_size__(2, 3), __malloc__, __nonnull__(4)));
char *x_strdup(const char *, const char *, int)
    __attribute__((__malloc__, __nonnull__));
char *x_strndup(const char *, size_t, const char *, int)
    __attribute__((__malloc__, __nonnull__));
void x_vasprintf(char **, const char *, va_list, const char *, int)
    __attribute__((__nonnull__));

/* asprintf special case. */
#if INN_HAVE_C99_VAMACROS || INN_HAVE_GNU_VAMACROS
void x_asprintf(char **, const char *, int, const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 4, 5)));
#else
void x_asprintf(char **, const char *, ...)
    __attribute__((__nonnull__, __format__(printf, 2, 3)));
#endif

/*
 * Failure handler takes the function, the size, the file, and the line.  The
 * size will be zero if the failure was due to some failure in snprintf
 * instead of a memory allocation failure.
 */
typedef void (*xmalloc_handler_type)(const char *, size_t, const char *, int);

/* The default error handler. */
void xmalloc_fail(const char *, size_t, const char *, int)
    __attribute__((__nonnull__));

/*
 * Assign to this variable to choose a handler other than the default, which
 * just calls sysdie.
 */
extern xmalloc_handler_type xmalloc_error_handler;

END_DECLS

#endif /* INN_XMALLOC_H */
