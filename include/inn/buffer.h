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

#ifndef INN_BUFFER_H
#define INN_BUFFER_H 1

struct buffer {
    size_t size;                /* Total allocated length. */
    size_t used;                /* Data already used. */
    size_t left;                /* Remaining unused data. */
    char *data;                 /* Pointer to allocated memory. */
};

BEGIN_DECLS

/* Allocate a new buffer and initialize its contents. */
struct buffer *buffer_new(void);

/* Resize a buffer to be at least as large as the provided size. */
void buffer_resize(struct buffer *, size_t);

/* Set the buffer contents, ignoring anything currently there. */
void buffer_set(struct buffer *, const char *data, size_t length);

/* Append data to the buffer. */
void buffer_append(struct buffer *, const char *data, size_t length);

/* Swap the contents of two buffers. */
void buffer_swap(struct buffer *, struct buffer *);

END_DECLS

#endif /* INN_BUFFER_H */
