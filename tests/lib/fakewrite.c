/* $Id$ */
/* Fake write and writev functions for testing xwrite and xwritev. */

#include "config.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>

#include "libinn.h"

ssize_t fake_write(int, const void *, size_t);
ssize_t fake_pwrite(int, const void *, size_t, off_t);
ssize_t fake_writev(int, const struct iovec *, int);

/* All the data is actually written into this buffer.  We use write_offset
   to track how far we've written. */
char write_buffer[256];
size_t write_offset = 0;

/* If write_interrupt is non-zero, then half of the calls to write or writev
   will fail, returning -1 with errno set to EINTR. */
int write_interrupt = 0;

/* If write_fail is non-zero, all writes or writevs will return 0,
   indicating no progress in writing out the buffer. */
int write_fail = 0;

/* Accept a write request and write only the first 32 bytes of it into
   write_buffer (or as much as will fit), returning the amount written. */
ssize_t
fake_write(int fd UNUSED, const void *data, size_t n)
{
    size_t total;

    if (write_fail)
        return 0;
    if (write_interrupt && (write_interrupt++ % 2) == 0) {
        errno = EINTR;
        return -1;
    }
    total = (n < 32) ? n : 32;
    if (256 - write_offset < total)
        total = 256 - write_offset;
    memcpy(write_buffer + write_offset, data, total);
    write_offset += total;
    return total;
}

/* Accept a pwrite request and write only the first 32 bytes of it into
   write_buffer at the specified offset (or as much as will fit), returning
   the amount written. */
ssize_t
fake_pwrite(int fd UNUSED, const void *data, size_t n, off_t offset)
{
    size_t total;

    if (write_fail)
        return 0;
    if (write_interrupt && (write_interrupt++ % 2) == 0) {
        errno = EINTR;
        return -1;
    }
    total = (n < 32) ? n : 32;
    if (offset > 256) {
        errno = ENOSPC;
        return -1;
    }
    if ((size_t) (256 - offset) < total)
        total = 256 - offset;
    memcpy(write_buffer + offset, data, total);
    return total;
}

/* Accept an xwrite request and write only the first 32 bytes of it into
   write_buffer (or as much as will fit), returning the amount written. */
ssize_t
fake_writev(int fd UNUSED, const struct iovec *iov, int iovcnt)
{
    int total, i;
    size_t left, n;

    if (write_fail)
        return 0;
    if (write_interrupt && (write_interrupt++ % 2) == 0) {
        errno = EINTR;
        return -1;
    }
    left = 256 - write_offset;
    if (left > 32)
        left = 32;
    total = 0;
    for (i = 0; i < iovcnt && left != 0; i++) {
        n = ((size_t) iov[i].iov_len < left) ? iov[i].iov_len : left;
        memcpy(write_buffer + write_offset, iov[i].iov_base, n);
        write_offset += n;
        total += n;
        left -= n;
    }
    return total;
}
