/* $Id$
 *
 * Test suite for xwrite, xwritev, and xpwrite.
 *
 * The canonical version of this file is maintained in the rra-c-util package,
 * which can be found at <http://www.eyrie.org/~eagle/software/rra-c-util/>.
 *
 * Copyright 2000, 2001, 2002, 2004 Russ Allbery <eagle@eyrie.org>
 * Copyright 2009, 2014
 *     The Board of Trustees of the Leland Stanford Junior University
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
 */

#define LIBTEST_NEW_FORMAT 1

#include "config.h"
#include "clibrary.h"
#include "portable/uio.h"

#include <errno.h>

#include "tap/basic.h"
#include "inn/xwrite.h"

/* The data array we'll use to do testing. */
char data[256];

/* These come from fakewrite. */
extern char write_buffer[];
extern size_t write_offset;
extern int write_interrupt;
extern int write_fail;


static void
test_write(int status, int total, const char *name)
{
    is_int(total, status, "%s return status", name);
    ok(memcmp(data, write_buffer, 256) == 0, "%s output", name);
}


int
main(void)
{
    int i;
    struct iovec iov[4];

    plan(44);

    /* Test xwrite. */
    for (i = 0; i < 256; i++)
        data[i] = i;
    test_write(xwrite(0, data, 256), 256, "xwrite");
    write_offset = 0;
    write_interrupt = 1;
    memset(data, 0, 256);
    test_write(xwrite(0, data, 256), 256, "xwrite interrupted");
    write_offset = 0;
    for (i = 0; i < 32; i++)
        data[i] = i * 2;
    test_write(xwrite(0, data, 32), 32, "xwrite first block");
    for (i = 32; i < 65; i++)
        data[i] = i * 2;
    test_write(xwrite(0, data + 32, 33), 33, "xwrite second block");
    write_offset = 0;
    write_interrupt = 0;

    /* Test xwritev. */
    memset(data, 0, 256);
    iov[0].iov_base = data;
    iov[0].iov_len = 256;
    test_write(xwritev(0, iov, 1), 256, "xwritev");
    write_offset = 0;
    for (i = 0; i < 256; i++)
        data[i] = i;
    iov[0].iov_len = 128;
    iov[1].iov_base = &data[128];
    iov[1].iov_len = 16;
    iov[2].iov_base = &data[144];
    iov[2].iov_len = 112;
    test_write(xwritev(0, iov, 3), 256, "xwritev with multiple iovs");
    write_offset = 0;
    write_interrupt = 1;
    memset(data, 0, 256);
    iov[0].iov_len = 32;
    iov[1].iov_base = &data[32];
    iov[1].iov_len = 224;
    test_write(xwritev(0, iov, 2), 256, "xwritev interrupted");
    for (i = 0; i < 32; i++)
        data[i] = i * 2;
    write_offset = 0;
    test_write(xwritev(0, iov, 1), 32, "xwritev first block");
    for (i = 32; i < 65; i++)
        data[i] = i * 2;
    iov[0].iov_base = &data[32];
    iov[0].iov_len = 16;
    iov[1].iov_base = &data[48];
    iov[1].iov_len = 1;
    iov[2].iov_base = &data[49];
    iov[2].iov_len = 8;
    iov[3].iov_base = &data[57];
    iov[3].iov_len = 8;
    test_write(xwritev(0, iov, 4), 33, "xwritev second block");
    write_offset = 0;
    write_interrupt = 0;

    /* Test bounds errors in xwritev. */
    is_int(-1, xwritev(0, iov, -1), "xwrite with negative count");
    is_int(EINVAL, errno, "...with correct errno");
    if (INT_MAX <= SIZE_MAX / sizeof(struct iovec))
        skip_block(2, "xwritev count overflow not possible");
    else {
        is_int(-1, xwritev(0, iov, INT_MAX), "xwrite with INT_MAX count");
        is_int(EINVAL, errno, "...with correct errno");
    }

    /* Test xpwrite. */
    for (i = 0; i < 256; i++)
        data[i] = i;
    test_write(xpwrite(0, data, 256, 0), 256, "xpwrite");
    write_interrupt = 1;
    memset(data + 1, 0, 255);
    test_write(xpwrite(0, data + 1, 255, 1), 255, "xpwrite interrupted");
    for (i = 0; i < 32; i++)
        data[i + 32] = i * 2;
    test_write(xpwrite(0, data + 32, 32, 32), 32, "xpwrite first block");
    for (i = 32; i < 65; i++)
        data[i + 32] = i * 2;
    test_write(xpwrite(0, data + 64, 33, 64), 33, "xpwrite second block");
    write_interrupt = 0;

    /* Test failures. */
    write_fail = 1;
    test_write(xwrite(0, data + 1, 255), -1, "xwrite fail");
    iov[0].iov_base = data + 1;
    iov[0].iov_len = 255;
    test_write(xwritev(0, iov, 1), -1, "xwritev fail");
    test_write(xpwrite(0, data + 1, 255, 0), -1, "xpwrite fail");

    /* Test zero-length writes. */
    test_write(xwrite(0, "   ", 0), 0, "xwrite zero length");
    test_write(xpwrite(0, "   ", 0, 2), 0, "xpwrite zero length");
    iov[0].iov_base = data + 1;
    iov[0].iov_len = 2;
    test_write(xwritev(0, iov, 0), 0, "xwritev zero length");
    iov[0].iov_len = 0;
    test_write(xwritev(0, iov, 1), 0, "xwritev zero length with buffers");

    return 0;
}
