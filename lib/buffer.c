/* $Id$
 *
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
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2011, 2012, 2014
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

#include "config.h"
#include "clibrary.h"

#include <errno.h>
#include <sys/stat.h>

#include "inn/buffer.h"
#include "inn/xmalloc.h"


/*
 * Allocate a new struct buffer and initialize it.
 */
struct buffer *
buffer_new(void)
{
    return xcalloc(1, sizeof(struct buffer));
}


/*
 * Free a buffer.
 */
void
buffer_free(struct buffer *buffer)
{
    if (buffer == NULL)
        return;
    free(buffer->data);
    free(buffer);
}


/*
 * Resize a buffer to be at least as large as the provided second argument.
 * Resize buffers to multiples of 1KB to keep the number of reallocations to a
 * minimum.  Refuse to resize a buffer to make it smaller.
 */
void
buffer_resize(struct buffer *buffer, size_t size)
{
    if (size < buffer->size)
        return;
    buffer->size = (size + 1023) & ~1023UL;
    buffer->data = xrealloc(buffer->data, buffer->size);
}


/*
 * Compact a buffer by moving the data between buffer->used and buffer->left
 * to the beginning of the buffer, overwriting the already-consumed data.
 */
void
buffer_compact(struct buffer *buffer)
{
    if (buffer->used == 0)
        return;
    if (buffer->left != 0)
        memmove(buffer->data, buffer->data + buffer->used, buffer->left);
    buffer->used = 0;
}


/*
 * Replace whatever data is currently in the buffer with the provided data.
 * Resize the buffer if needed.
 */
void
buffer_set(struct buffer *buffer, const char *data, size_t length)
{
    if (length > 0) {
        buffer_resize(buffer, length);
        memmove(buffer->data, data, length);
    }
    buffer->left = length;
    buffer->used = 0;
}


/*
 * Append data to a buffer.  The new data shows up as additional unused data
 * at the end of the buffer.  Resize the buffer if needed.
 */
void
buffer_append(struct buffer *buffer, const char *data, size_t length)
{
    size_t total;

    if (length == 0)
        return;
    total = buffer->used + buffer->left;
    buffer_resize(buffer, total + length);
    buffer->left += length;
    memcpy(buffer->data + total, data, length);
}


/*
 * Print data into a buffer from the supplied va_list, appending to the end.
 * The new data shows up as unused data at the end of the buffer.  The
 * trailing nul is not added to the buffer.
 */
void
buffer_append_vsprintf(struct buffer *buffer, const char *format, va_list args)
{
    size_t total, avail;
    ssize_t status;
    va_list args_copy;

    total = buffer->used + buffer->left;
    avail = buffer->size - total;
    va_copy(args_copy, args);
    status = vsnprintf(buffer->data + total, avail, format, args_copy);
    va_end(args_copy);
    if (status < 0)
        return;
    if ((size_t) status + 1 <= avail) {
        buffer->left += status;
    } else {
        buffer_resize(buffer, total + status + 1);
        avail = buffer->size - total;
        status = vsnprintf(buffer->data + total, avail, format, args);
        if (status < 0 || (size_t) status + 1 > avail)
            return;
        buffer->left += status;
    }
}


/*
 * Print data into a buffer, appending to the end.  The new data shows up as
 * unused data at the end of the buffer.  Resize the buffer if needed.  The
 * trailing nul is not added to the buffer.
 */
void
buffer_append_sprintf(struct buffer *buffer, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    buffer_append_vsprintf(buffer, format, args);
    va_end(args);
}


/*
 * Replace the current buffer contents with data printed from the supplied
 * va_list.  The new data shows up as unused data at the end of the buffer.
 * The trailing nul is not added to the buffer.
 */
void
buffer_vsprintf(struct buffer *buffer, const char *format, va_list args)
{
    buffer_set(buffer, NULL, 0);
    buffer_append_vsprintf(buffer, format, args);
}


/*
 * Replace the current buffer contents with data printed from the supplied
 * format string and arguments.  The new data shows up as unused data at the
 * end of the buffer.  The trailing nul is not added to the buffer.
 */
void
buffer_sprintf(struct buffer *buffer, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    buffer_vsprintf(buffer, format, args);
    va_end(args);
}


/*
 * Swap the contents of two buffers.
 */
void
buffer_swap(struct buffer *one, struct buffer *two)
{
    struct buffer tmp;

    tmp = *one;
    *one = *two;
    *two = tmp;
}


/*
 * Find a given string in the unconsumed data in buffer.  We know that all the
 * data prior to start (an offset into the space between buffer->used and
 * buffer->left) has already been searched.  Returns the offset of the string
 * (with the same meaning as start) in offset if found, and returns true if
 * the terminator is found and false otherwise.
 */
bool
buffer_find_string(struct buffer *buffer, const char *string, size_t start,
                   size_t *offset)
{
    char *terminator, *data;
    size_t length;

    length = strlen(string);
    do {
        data = buffer->data + buffer->used + start;
        terminator = memchr(data, string[0], buffer->left - start);
        if (terminator == NULL)
            return false;
        start = (terminator - buffer->data) - buffer->used;
        if (buffer->left - start < length)
            return false;
        start++;
    } while (memcmp(terminator, string, length) != 0);
    *offset = start - 1;
    return true;
}


/*
 * Read from a file descriptor into a buffer, up to the available space in the
 * buffer, and return the number of characters read.
 */
ssize_t
buffer_read(struct buffer *buffer, int fd)
{
    ssize_t count;

    do {
        size_t used = buffer->used + buffer->left;
        count = read(fd, buffer->data + used, buffer->size - used);
    } while (count == -1 && (errno == EAGAIN || errno == EINTR));
    if (count > 0)
        buffer->left += count;
    return count;
}


/*
 * Read from a file descriptor until end of file is reached, doubling the
 * buffer size as necessary to hold all of the data.  Returns true on success,
 * false on failure (in which case errno will be set).
 */
bool
buffer_read_all(struct buffer *buffer, int fd)
{
    ssize_t count;

    if (buffer->size == 0)
        buffer_resize(buffer, 1024);
    do {
        size_t used = buffer->used + buffer->left;
        if (buffer->size <= used)
            buffer_resize(buffer, buffer->size * 2);
        count = buffer_read(buffer, fd);
    } while (count > 0);
    return (count == 0);
}


/*
 * Read the entire contents of a file into a buffer.  This is a slight
 * optimization over buffer_read_all because it can stat the file descriptor
 * first and size the buffer appropriately.  buffer_read_all will still handle
 * the case where the file size changes while it's being read.  Returns true
 * on success, false on failure (in which case errno will be set).
 */
bool
buffer_read_file(struct buffer *buffer, int fd)
{
    struct stat st;
    size_t used = buffer->used + buffer->left;

    if (fstat(fd, &st) < 0)
        return false;
    buffer_resize(buffer, st.st_size + used);
    return buffer_read_all(buffer, fd);
}
