/*  $Id$
**
**  write and writev replacements to handle partial writes.
**
**  Usage:
**
**      ssize_t xwrite(int fildes, void *buf, size_t nbyte);
**      ssize_t xwritev(int fildes, const struct iovec *iov, int iovcnt);
**
**  xwrite and xwritev behave exactly like their C library counterparts
**  except that, if write or writev succeeds but returns a number of bytes
**  written less than the total bytes, the write is repeated picking up
**  where it left off until the full amount of the data is written.  The
**  write is also repeated if it failed with EINTR.  The write will be
**  aborted after 10 successive writes with no forward progress.
**
**  Both functions return the number of bytes written on success or -1 on an
**  error, and will leave errno set to whatever the underlying system call
**  set it to.  Note that it is possible for a write to fail after some data
**  was written, on the subsequent additional write; in that case, these
**  functions will return -1 and the number of bytes actually written will
**  be lost.
*/

#include "config.h"
#include "libinn.h"

#include <errno.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/uio.h>
#if HAVE_UNISTD_H
# include <unistd.h>
#endif

ssize_t
xwrite(int fd, const void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    /* Abort the write if we try ten times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 10) break;
        status = write(fd, (const char *) buffer + total, size - total);
        if (status > 0) count = 0;
        if (status < 0) {
            if (errno != EINTR) break;
            status = 0;
        }
    }
    return (total < size) ? -1 : total;
}

ssize_t
xwritev(int fd, const struct iovec iov[], int iovcnt)
{
    ssize_t total, status;
    size_t left;
    int iovleft, i, count;
    struct iovec *tmpiov;

    /* Get a count of the total number of bytes in the iov array. */
    for (total = 0, i = 0; i < iovcnt; i++)
        total += iov[i].iov_len;

    /* First, try just writing it all out.  Most of the time this will
       succeed and save us lots of work.  Abort the write if we try ten
       times with no forward progress. */
    count = 0;
    do {
        if (++count > 10) break;
        status = writev(fd, iov, iovcnt);
        if (status > 0) count = 0;
    } while (status < 0 && errno == EINTR);
    if (status < 0) return -1;
    if (status == total) return total;

    /* If we fell through to here, the first write partially succeeded.
       Figure out how far through the iov array we got, and then duplicate
       the rest of it so that we can modify it to reflect how much we manage
       to write on successive tries. */
    left = total - status;
    for (i = 0; status >= iov[i].iov_len; i++)
        status -= iov[i].iov_len;
    iovleft = iovcnt - i;
    tmpiov = xmalloc(iovleft * sizeof(struct iovec));
    memcpy(tmpiov, iov + i, iovleft * sizeof(struct iovec));

    /* status now contains the offset into the first iovec struct in tmpiov.
       Go into the write loop, trying to write out everything remaining at
       each point.  At the top of the loop, status will contain a count of
       bytes written out at the beginning of the set of iovec structs. */
    i = 0;
    do {
        if (++count > 10) break;

        /* Skip any leading data that has been written out. */
        if (status < 0) status = 0;
        for (; status >= tmpiov[i].iov_len && iovleft > 0; i++, iovleft--)
            status -= tmpiov[i].iov_len;
        tmpiov[i].iov_base = (char *) tmpiov[i].iov_base + status;
        tmpiov[i].iov_len -= status;

        /* Write out what's left and return success if it's all written. */
        status = writev(fd, tmpiov + i, iovleft);
        if (status > 0) {
            left -= status;
            count = 0;
        }
    } while (left > 0 && (status >= 0 || errno == EINTR));

    /* We're either done or got an error; if we're done, left is now 0. */
    free(tmpiov);
    return (left == 0) ? total : -1;
}
