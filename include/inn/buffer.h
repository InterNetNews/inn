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

#include <sys/types.h>
#include <inn/defines.h>

struct buffer {
    size_t size;                /* Total allocated length. */
    size_t used;                /* Data already used. */
    size_t left;                /* Remaining unused data. */
    char *data;                 /* Pointer to allocated memory. */
};

BEGIN_DECLS

/* Allocate a new buffer and initialize its contents. */
struct buffer *buffer_new(void);

/* Free an allocated buffer. */
void buffer_free(struct buffer *);

/* Resize a buffer to be at least as large as the provided size.  Invalidates
   pointers into the buffer. */
void buffer_resize(struct buffer *, size_t);

/* Compact a buffer, removing all used data and moving unused data to the
   beginning of the buffer.  Invalidates pointers into the buffer. */
void buffer_compact(struct buffer *);

/* Set the buffer contents, ignoring anything currently there. */
void buffer_set(struct buffer *, const char *data, size_t length);

/* Append data to the buffer. */
void buffer_append(struct buffer *, const char *data, size_t length);

/* Swap the contents of two buffers. */
void buffer_swap(struct buffer *, struct buffer *);

/* Find the given string in the unconsumed data in a buffer.  start is an
   offset into the unused data specifying where to start the search (to save
   time with multiple searches).  Pass 0 to start the search at the beginning
   of the unused data.  Returns true if the terminator is found, putting the
   offset (into the unused data space) of the beginning of the terminator into
   the fourth argument.  Returns false if the terminator isn't found. */
bool buffer_find_string(struct buffer *, const char *, size_t start,
                        size_t *offset);

/* Read from a file descriptor into a buffer, up to the available space in the
   buffer.  Return the number of characters read. */
ssize_t buffer_read(struct buffer *, int fd);

END_DECLS

#endif /* INN_BUFFER_H */
