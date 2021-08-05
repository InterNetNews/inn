/*
 * Portability wrapper around <sys/uio.h>.
 *
 * Provides a definition of the iovec struct for platforms that don't have it
 * (primarily Windows).  Currently, the corresponding readv and writev
 * functions are not provided or prototyped here.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <https://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Written by Russ Allbery <eagle@eyrie.org>
 * Copyright 2008, 2011
 *     The Board of Trustees of the Leland Stanford Junior University
 *
 * Copying and distribution of this file, with or without modification, are
 * permitted in any medium without royalty provided the copyright notice and
 * this notice are preserved.  This file is offered as-is, without any
 * warranty.
 *
 * SPDX-License-Identifier: FSFAP
 */

#ifndef PORTABLE_UIO_H
#define PORTABLE_UIO_H 1

#include <sys/types.h>

/* remctl.h provides its own definition of this struct on Windows. */
#if defined(HAVE_SYS_UIO_H)
#    include <sys/uio.h>
#elif !defined(REMCTL_H)
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif

#endif /* !PORTABLE_UIO_H */
