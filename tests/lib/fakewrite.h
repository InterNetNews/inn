/*
 * Testing interface to fake write functions.
 *
 * This header defines the interfaces to fake write functions used to test
 * error handling wrappers around system write functions.
 *
 * Copyright 2000-2002, 2004, 2017 Russ Allbery <eagle@eyrie.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * SPDX-License-Identifier: MIT
 */

#define LIBTEST_NEW_FORMAT 1

#ifndef TESTS_INN_FAKEWRITE_H
#define TESTS_INN_FAKEWRITE_H 1

#include "config.h"
#include "portable/macros.h"
#include "portable/stdbool.h"

#include <sys/types.h>

BEGIN_DECLS

/* Replacement functions called instead of C library calls. */
extern ssize_t fake_write(int, const void *, size_t);
extern ssize_t fake_pwrite(int, const void *, size_t, off_t);
extern ssize_t fake_writev(int, const struct iovec *, int);

/* The data written and how many bytes have been written. */
extern char write_buffer[256];
extern size_t write_offset;

/* If true, half the calls to write or writev will fail with EINTR. */
extern int write_interrupt;

/* If true, all write or writev calls will return 0. */
extern bool write_fail;

END_DECLS

#endif /* !TESTS_INN_FAKEWRITE_H */
