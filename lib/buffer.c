/*  $Id$
**
**  Counted, reusable memory buffer.
**
**  A buffer is an allocated bit of memory with a known size and a separate
**  data length.  It's intended to store strings and can be reused repeatedly
**  to minimize the number of memory allocations.  Buffers increase in
**  increments of 1K.
**
**  A buffer contains a notion of the data that's been used and the data
**  that's been left, used when the buffer is an I/O buffer where lots of data
**  is buffered and then slowly processed out of the buffer.  The total length
**  of the data is used + left.  If a buffer is just used to store some data,
**  used can be set to 0 and left stores the length of the data.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>

#include "inn/buffer.h"
#include "libinn.h"

/*
**  Allocate a new struct buffer and initialize it.
*/
struct buffer *
buffer_new(void)
{
    struct buffer *buffer;

    buffer = xmalloc(sizeof(struct buffer));
    buffer->size = 0;
    buffer->used = 0;
    buffer->left = 0;
    buffer->data = NULL;
    return buffer;
}


/*
**  Free a buffer.
*/
void
buffer_free(struct buffer *buffer)
{
    if (buffer->data != NULL)
        free(buffer->data);
    free(buffer);
}


/*
**  Resize a buffer to be at least as large as the provided second argument.
**  Resize buffers to multiples of 1KB to keep the number of reallocations to
**  a minimum.  Refuse to resize a buffer to make it smaller.
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
**  Compact a buffer by moving the data between buffer->used and buffer->left
**  to the beginning of the buffer, overwriting the already-consumed data.
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
**  Replace whatever data is currently in the buffer with the provided data.
**  Resize the buffer if needed.
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
**  Append data to a buffer.  The new data shows up as additional unused data
**  at the end of the buffer.  Resize the buffer if needed.
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
**  Print data into a buffer from the supplied va_list, either appending to
**  the end of it or replacing the existing contents.  The new data shows up
**  as unused data at the end of the buffer.  Returns true if successful and
**  false if there isn't enough room.  If there wasn't enough room, the buffer
**  is also resized so that if the command is retried, it will succeed.  The
**  trailing nul is not added to the buffer.
*/
bool
buffer_vsprintf(struct buffer *buffer, bool append, const char *format,
                va_list args)
{
    size_t total, avail;
    ssize_t status;

    if (!append)
        buffer_set(buffer, NULL, 0);
    total = buffer->used + buffer->left;
    avail = buffer->size - total;
    status = vsnprintf(buffer->data + total, avail, format, args);
    if (status < 0)
        return false;
    if ((size_t) status + 1 <= avail) {
        buffer->left += status;
        return true;
    } else {
        buffer_resize(buffer, total + status + 1);
        return false;
    }
}


/*
**  Print data into a buffer, either appending to the end of it or replacing
**  the existing contents.  The new data shows up as unused data at the end of
**  the buffer.  Resize the buffer if needed.  The trailing nul is not added
**  to the buffer.
*/
void
buffer_sprintf(struct buffer *buffer, bool append, const char *format, ...)
{
    va_list args;
    bool done;

    va_start(args, format);
    done = buffer_vsprintf(buffer, append, format, args);
    va_end(args);
    if (!done) {
        va_start(args, format);
        buffer_vsprintf(buffer, append, format, args);
        va_end(args);
    }
}


/*
**  Swap the contents of two buffers.
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
**  Find a given string in the unconsumed data in buffer.  We know that all
**  the data prior to start (an offset into the space between buffer->used and
**  buffer->left) has already been searched.  Returns the offset of the string
**  (with the same meaning as start) in offset if found, and returns true if
**  the terminator is found and false otherwise.
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
**  Read from a file descriptor into a buffer, up to the available space in
**  the buffer, and return the number of characters read.
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
