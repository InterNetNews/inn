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
**  Replace whatever data is currently in the buffer with the provided data.
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
**  at the end of the buffer.  Resize the buffer to multiples of 1KB.
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
