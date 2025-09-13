/*
 * Counted, reusable memory buffer.
 *
 * A buffer is an allocated block of memory with a known size and a separate
 * data length.  It's intended to store strings and can be reused repeatedly
 * to minimize the number of memory allocations.  Buffers increase in
 * increments of 1K, or double for some operations.
 *
 * A buffer contains a record of what data has been used and what data is as
 * yet unprocessed, used when the buffer is an I/O buffer where lots of data
 * is buffered and then slowly processed out of the buffer.  The total length
 * of the data is used + left.  If a buffer is just used to store some data,
 * used can be set to 0 and left stores the length of the data.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2014-2015, 2022, 2025 Russ Allbery <eagle@eyrie.org>
 * Copyright 2011-2012
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

#ifndef INN_BUFFER_H
#define INN_BUFFER_H 1

#include "inn/portable-macros.h"
#include "inn/portable-stdbool.h"

#include <stdarg.h>
#include <sys/types.h>

struct buffer {
    size_t size; /* Total allocated length. */
    size_t used; /* Data already used. */
    size_t left; /* Remaining unused data. */
    char *data;  /* Pointer to allocated memory. */
};

BEGIN_DECLS

/* Free an allocated buffer. */
void buffer_free(struct buffer *);

/* Allocate a new buffer and initialize its contents. */
struct buffer *buffer_new(void)
    __attribute__((__warn_unused_result__, __malloc__(buffer_free)));

/*
 * Resize a buffer to be at least as large as the provided size.  Invalidates
 * pointers into the buffer.
 */
void buffer_resize(struct buffer *, size_t) __attribute__((__nonnull__));

/*
 * Compact a buffer, removing all used data and moving unused data to the
 * beginning of the buffer.  Invalidates pointers into the buffer.
 */
void buffer_compact(struct buffer *) __attribute__((__nonnull__));

/*
 * Set the buffer contents, ignoring anything currently there.  If length is
 * 0, empties the buffer, in which case data may be NULL.
 */
void buffer_set(struct buffer *, const char *data, size_t length)
    __attribute__((__nonnull__(1)));

/*
 * Set the buffer contents via a sprintf-style format string.  No trailing
 * nul is added.
 */
void buffer_sprintf(struct buffer *, const char *, ...)
    __attribute__((__format__(printf, 2, 3), __nonnull__));
void buffer_vsprintf(struct buffer *, const char *, va_list)
    __attribute__((__format__(printf, 2, 0), __nonnull__));

/* Append data to the buffer. */
void buffer_append(struct buffer *, const char *data, size_t length)
    __attribute__((__nonnull__(1)));

/* Append via an sprintf-style format string.  No trailing nul is added. */
void buffer_append_sprintf(struct buffer *, const char *, ...)
    __attribute__((__format__(printf, 2, 3), __nonnull__));
void buffer_append_vsprintf(struct buffer *, const char *, va_list)
    __attribute__((__format__(printf, 2, 0), __nonnull__));

/* Swap the contents of two buffers. */
void buffer_swap(struct buffer *, struct buffer *)
    __attribute__((__nonnull__));

/*
 * Find the given string in the unconsumed data in a buffer.  start is an
 * offset into the unused data specifying where to start the search (to save
 * time with multiple searches).  Pass 0 to start the search at the beginning
 * of the unused data.  Returns true if the terminator is found, putting the
 * offset (into the unused data space) of the beginning of the terminator into
 * the fourth argument.  Returns false if the terminator isn't found.
 */
bool buffer_find_string(struct buffer *, const char *, size_t start,
                        size_t *offset) __attribute__((__nonnull__));

/*
 * Read from a file descriptor into a buffer, up to the available space in the
 * buffer.  Return the number of characters read.  Retries the read if
 * interrupted by a signal or if it returns EAGAIN, but stops on any other
 * error or after any successful read.  Returns -1 on an error reading from
 * the file descriptor and sets errno.
 */
ssize_t buffer_read(struct buffer *, int fd)
    __attribute__((__fd_arg_read__(2), __nonnull__));

/*
 * Read from a file descriptor into a buffer until end of file is reached.
 * Returns true on success and false (setting errno) on error.
 */
bool buffer_read_all(struct buffer *, int fd)
    __attribute__((__fd_arg_read__(2), __nonnull__));

/*
 * Read the contents of a file into a buffer.  This should be used instead of
 * buffer_read_all when fstat can be called on the file descriptor.  Returns
 * true on success and false (setting errno) on error.
 */
bool buffer_read_file(struct buffer *, int fd)
    __attribute__((__fd_arg_read__(2), __nonnull__));

END_DECLS

#endif /* INN_BUFFER_H */
