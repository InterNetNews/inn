/* $Id$
 *
 * write and writev replacements to handle partial writes.
 *
 * Usage:
 *
 *     ssize_t xwrite(int fildes, const void *buf, size_t nbyte);
 *     ssize_t xpwrite(int fildes, const void *buf, size_t nbyte,
 *                     off_t offset);
 *     ssize_t xwritev(int fildes, const struct iovec *iov, int iovcnt);
 *
 * xwrite, xpwrite, and xwritev behave exactly like their C library
 * counterparts except that, if write or writev succeeds but returns a number
 * of bytes written less than the total bytes, the write is repeated picking
 * up where it left off until the full amount of the data is written.  The
 * write is also repeated if it failed with EINTR.  The write will be aborted
 * after 10 successive writes with no forward progress.
 *
 * Both functions return the number of bytes written on success or -1 on an
 * error, and will leave errno set to whatever the underlying system call set
 * it to.  Note that it is possible for a write to fail after some data was
 * written, on the subsequent additional write; in that case, these functions
 * will return -1 and the number of bytes actually written will be lost.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2008, 2013, 2014
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
#include "portable/uio.h"

#include <assert.h>
#include <errno.h>

#include "inn/xwrite.h"

/*
 * If we're running the test suite, call testing versions of the write
 * functions.  #undef pwrite first because large file support may define a
 * macro pwrite (pointing to pwrite64) on some platforms (e.g. Solaris).
 */
#if TESTING
# undef pwrite
# define pwrite fake_pwrite
# define write  fake_write
# define writev fake_writev
ssize_t fake_pwrite(int, const void *, size_t, off_t);
ssize_t fake_write(int, const void *, size_t);
ssize_t fake_writev(int, const struct iovec *, int);
#endif


ssize_t
xwrite(int fd, const void *buffer, size_t size)
{
    size_t total;
    ssize_t status;
    int count = 0;

    if (size == 0)
	return 0;

    /* Abort the write if we try ten times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 10)
            break;
        status = write(fd, (const char *) buffer + total, size - total);
        if (status > 0)
            count = 0;
        if (status < 0) {
            if (errno != EINTR)
                break;
            status = 0;
        }
    }
    return (total < size) ? -1 : (ssize_t) total;
}


#ifndef _WIN32
ssize_t
xpwrite(int fd, const void *buffer, size_t size, off_t offset)
{
    size_t total;
    ssize_t status;
    int count = 0;

    if (size == 0)
	return 0;

    /* Abort the write if we try ten times with no forward progress. */
    for (total = 0; total < size; total += status) {
        if (++count > 10)
            break;
        status = pwrite(fd, (const char *) buffer + total, size - total,
                        offset + total);
        if (status > 0)
            count = 0;
        if (status < 0) {
            if (errno != EINTR)
                break;
            status = 0;
        }
    }
    return (total < size) ? -1 : (ssize_t) total;
}
#endif


ssize_t
xwritev(int fd, const struct iovec iov[], int iovcnt)
{
    ssize_t total, status = 0;
    size_t left, offset;
    int iovleft, i, count;
    struct iovec *tmpiov;

    /*
     * Bounds-check the iovcnt argument.  This is just for our safety.  The
     * system will probably impose a lower limit on iovcnt, causing the later
     * writev to fail with an error we'll return.
     */
    if (iovcnt == 0)
	return 0;
    if (iovcnt < 0 || (size_t) iovcnt > SIZE_MAX / sizeof(struct iovec)) {
        errno = EINVAL;
        return -1;
    }

    /* Get a count of the total number of bytes in the iov array. */
    for (total = 0, i = 0; i < iovcnt; i++)
        total += iov[i].iov_len;
    if (total == 0)
	return 0;

    /*
     * First, try just writing it all out.  Most of the time this will succeed
     * and save us lots of work.  Abort the write if we try ten times with no
     * forward progress.
     */
    count = 0;
    do {
        if (++count > 10)
            break;
        status = writev(fd, iov, iovcnt);
        if (status > 0)
            count = 0;
    } while (status < 0 && errno == EINTR);
    if (status < 0)
        return -1;
    if (status == total)
        return total;

    /*
     * If we fell through to here, the first write partially succeeded.
     * Figure out how far through the iov array we got, and then duplicate the
     * rest of it so that we can modify it to reflect how much we manage to
     * write on successive tries.
     */
    offset = status;
    left = total - offset;
    for (i = 0; offset >= (size_t) iov[i].iov_len; i++)
        offset -= iov[i].iov_len;
    iovleft = iovcnt - i;
    assert(iovleft > 0);
    tmpiov = calloc(iovleft, sizeof(struct iovec));
    if (tmpiov == NULL)
        return -1;
    memcpy(tmpiov, iov + i, iovleft * sizeof(struct iovec));

    /*
     * status now contains the offset into the first iovec struct in tmpiov.
     * Go into the write loop, trying to write out everything remaining at
     * each point.  At the top of the loop, status will contain a count of
     * bytes written out at the beginning of the set of iovec structs.
     */
    i = 0;
    do {
        if (++count > 10)
            break;

        /* Skip any leading data that has been written out. */
        for (; offset >= (size_t) tmpiov[i].iov_len && iovleft > 0; i++) {
            offset -= tmpiov[i].iov_len;
            iovleft--;
        }
        tmpiov[i].iov_base = (char *) tmpiov[i].iov_base + offset;
        tmpiov[i].iov_len -= offset;

        /* Write out what's left and return success if it's all written. */
        status = writev(fd, tmpiov + i, iovleft);
        if (status <= 0)
            offset = 0;
        else {
            offset = status;
            left -= offset;
            count = 0;
        }
    } while (left > 0 && (status >= 0 || errno == EINTR));

    /* We're either done or got an error; if we're done, left is now 0. */
    free(tmpiov);
    return (left == 0) ? total : -1;
}
