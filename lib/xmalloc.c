/*
 * malloc routines with failure handling.
 *
 * Usage:
 *
 *      extern xmalloc_handler_t memory_error;
 *      extern const char *string;
 *      char *buffer;
 *      va_list args;
 *
 *      xmalloc_error_handler = memory_error;
 *      buffer = xmalloc(1024);
 *      xrealloc(buffer, 2048);
 *      free(buffer);
 *      buffer = xcalloc(1, 1024);
 *      free(buffer);
 *      buffer = xstrdup(string);
 *      free(buffer);
 *      buffer = xstrndup(string, 25);
 *      free(buffer);
 *      xasprintf(&buffer, "%s", "some string");
 *      free(buffer);
 *      xvasprintf(&buffer, "%s", args);
 *
 * xmalloc, xcalloc, xrealloc, and xstrdup behave exactly like their C library
 * counterparts without the leading x except that they will never return NULL.
 * Instead, on error, they call xmalloc_error_handler, passing it the name of
 * the function whose memory allocation failed, the amount of the allocation,
 * and the file and line number where the allocation function was invoked
 * (from __FILE__ and __LINE__).  This function may do whatever it wishes,
 * such as some action to free up memory or a call to sleep to hope that
 * system resources return.  If the handler returns, the interrupted memory
 * allocation function will try its allocation again (calling the handler
 * again if it still fails).
 *
 * xreallocarray behaves the same as the OpenBSD reallocarray function but for
 * the same error checking, which in turn is the same as realloc but with
 * calloc-style arguments and size overflow checking.
 *
 * xstrndup behaves like xstrdup but only copies the given number of
 * characters.  It allocates an additional byte over its second argument and
 * always nul-terminates the string.
 *
 * xasprintf and xvasprintf behave just like their GNU glibc library
 * implementations except that they do the same checking as described above.
 * xasprintf will only be able to provide accurate file and line information
 * on systems that support variadic macros.
 *
 * The default error handler, if none is set by the caller, prints an error
 * message to stderr and exits with exit status 1.  An error handler must take
 * a const char * (function name), size_t (bytes allocated), const char *
 * (file), and int (line).
 *
 * xmalloc will return a pointer to a valid memory region on an xmalloc of 0
 * bytes, ensuring this by allocating space for one character instead of 0
 * bytes.
 *
 * The functions defined here are actually x_malloc, x_realloc, etc.  The
 * header file defines macros named xmalloc, etc. that pass the file name and
 * line number to these functions.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2015, 2023 Russ Allbery <eagle@eyrie.org>
 * Copyright 2012-2014
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

#include "config.h"
#include "portable/system.h"

#include "inn/messages.h"
#include "inn/xmalloc.h"


/*
 * The default error handler.
 */
void
xmalloc_fail(const char *function, size_t size, const char *file, int line)
{
    if (size == 0)
        sysdie("failed to format output with %s at %s line %d", function, file,
               line);
    else
        sysdie("failed to %s %lu bytes at %s line %d", function,
               (unsigned long) size, file, line);
}

/* Assign to this variable to choose a handler other than the default. */
xmalloc_handler_type xmalloc_error_handler = xmalloc_fail;


void *
x_malloc(size_t size, const char *file, int line)
{
    void *p;
    size_t real_size;

    real_size = (size > 0) ? size : 1;
    p = malloc(real_size);
    while (p == NULL) {
        xmalloc_error_handler("malloc", size, file, line);
        p = malloc(real_size);
    }
    return p;
}


void *
x_calloc(size_t n, size_t size, const char *file, int line)
{
    void *p;

    n = (n > 0) ? n : 1;
    size = (size > 0) ? size : 1;
    p = calloc(n, size);
    while (p == NULL) {
        xmalloc_error_handler("calloc", n * size, file, line);
        p = calloc(n, size);
    }
    return p;
}


void *
x_realloc(void *p, size_t size, const char *file, int line)
{
    void *newp;

    newp = realloc(p, size);
    if (size == 0)
        return newp;

        /*
         * GCC 13.2.0 (and some earlier versions) misdiagnose this error
         * handling as a use-after-free of p, but the C standard guarantees
         * that if realloc fails (which is true in every case when it returns
         * NULL when size > 0), p is unchanged and still valid.
         */
#if __GNUC__ >= 12 && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wuse-after-free"
#endif
    while (newp == NULL) {
        xmalloc_error_handler("realloc", size, file, line);
        newp = realloc(p, size);
    }
    return newp;
#if __GNUC__ >= 12 && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif
}


void *
x_reallocarray(void *p, size_t n, size_t size, const char *file, int line)
{
    void *newp;

    newp = reallocarray(p, n, size);
    if (size == 0 || n == 0)
        return newp;

        /*
         * GCC 13.2.0 (and some earlier versions) misdiagnose this error
         * handling as a use-after-free of p, but the documentation of
         * reallocarray guarantees that if reallocarray fails (which is true in
         * every case when it returns NULL when size > 0 and n > 0), p is
         * unchanged and still valid.
         */
#if __GNUC__ >= 12 && !defined(__clang__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wuse-after-free"
#endif
    while (newp == NULL) {
        xmalloc_error_handler("reallocarray", n * size, file, line);
        newp = reallocarray(p, n, size);
    }
#if __GNUC__ >= 12 && !defined(__clang__)
#    pragma GCC diagnostic pop
#endif
    return newp;
}


char *
x_strdup(const char *s, const char *file, int line)
{
    char *p;
    size_t len;

    len = strlen(s) + 1;
    p = malloc(len);
    while (p == NULL) {
        xmalloc_error_handler("strdup", len, file, line);
        p = malloc(len);
    }
    memcpy(p, s, len);
    return p;
}


/*
 * Avoid using the system strndup function since it may not exist (on Mac OS
 * X, for example), and there's no need to introduce another portability
 * requirement.
 */
char *
x_strndup(const char *s, size_t size, const char *file, int line)
{
    const char *p;
    size_t length;
    char *copy;

    /* Don't assume that the source string is nul-terminated. */
    for (p = s; (size_t) (p - s) < size && *p != '\0'; p++)
        ;
    length = p - s;
    copy = malloc(length + 1);
    while (copy == NULL) {
        xmalloc_error_handler("strndup", length + 1, file, line);
        copy = malloc(length + 1);
    }
    memcpy(copy, s, length);
    copy[length] = '\0';
    return copy;
}


void
x_vasprintf(char **strp, const char *fmt, va_list args, const char *file,
            int line)
{
    va_list args_copy;
    int status;

    va_copy(args_copy, args);
    status = vasprintf(strp, fmt, args_copy);
    va_end(args_copy);
    while (status < 0) {
        va_copy(args_copy, args);
        status = vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        status = (status < 0) ? 0 : status + 1;
        xmalloc_error_handler("vasprintf", status, file, line);
        va_copy(args_copy, args);
        status = vasprintf(strp, fmt, args_copy);
        va_end(args_copy);
    }
}


#if HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS
void
x_asprintf(char **strp, const char *file, int line, const char *fmt, ...)
{
    va_list args, args_copy;
    int status;

    va_start(args, fmt);
    va_copy(args_copy, args);
    status = vasprintf(strp, fmt, args_copy);
    va_end(args_copy);
    while (status < 0) {
        va_copy(args_copy, args);
        status = vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        status = (status < 0) ? 0 : status + 1;
        xmalloc_error_handler("asprintf", status, file, line);
        va_copy(args_copy, args);
        status = vasprintf(strp, fmt, args_copy);
        va_end(args_copy);
    }
    va_end(args);
}
#else  /* !(HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS) */
void
x_asprintf(char **strp, const char *fmt, ...)
{
    va_list args, args_copy;
    int status;

    va_start(args, fmt);
    va_copy(args_copy, args);
    status = vasprintf(strp, fmt, args_copy);
    va_end(args_copy);
    while (status < 0) {
        va_copy(args_copy, args);
        status = vsnprintf(NULL, 0, fmt, args_copy);
        va_end(args_copy);
        status = (status < 0) ? 0 : status + 1;
        xmalloc_error_handler("asprintf", status, __FILE__, __LINE__);
        va_copy(args_copy, args);
        status = vasprintf(strp, fmt, args_copy);
        va_end(args_copy);
    }
    va_end(args);
}
#endif /* !(HAVE_C99_VAMACROS || HAVE_GNU_VAMACROS) */
