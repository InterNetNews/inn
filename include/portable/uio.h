/* $Id$
 *
 * Portability wrapper around <sys/uio.h>.
 *
 * Provides a definition of the iovec struct for platforms that don't have it
 * (primarily Windows).  Currently, the corresponding readv and writev
 * functions are not provided or prototyped here.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 *
 * The authors hereby relinquish any claim to any copyright that they may have
 * in this work, whether granted under contract or by operation of law or
 * international treaty, and hereby commit to the public, at large, that they
 * shall not, at any time in the future, seek to enforce any copyright in this
 * work against any person or entity, or prevent any person or entity from
 * copying, publishing, distributing or creating derivative works of this
 * work.
 */

#ifndef PORTABLE_UIO_H
#define PORTABLE_UIO_H 1

#include <sys/types.h>

/* remctl.h provides its own definition of this struct on Windows. */
#if defined(HAVE_SYS_UIO_H)
# include <sys/uio.h>
#elif !defined(REMCTL_H)
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif

#endif /* !PORTABLE_UIO_H */
