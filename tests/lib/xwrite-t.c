/* $Id$ */
/* Test suite for xwrite and xwritev. */

#include "config.h"
#include "clibrary.h"
#include <sys/uio.h>

#include "libinn.h"

/* The data array we'll use to do testing. */
char data[256];

/* These come from fakewrite. */
extern char write_buffer[];
extern size_t write_offset;
extern int write_interrupt;
extern int write_fail;

static void
test_write(int n, int status, int total)
{
    int success;

    success = (status == total && memcmp(data, write_buffer, 256) == 0);
    printf("%sok %d\n", success ? "" : "not ", n);
    if (!success && status != total)
        printf("  status %d, total %d\n", status, total);
}

int
main(void)
{
    int i;
    struct iovec iov[4];

    puts("11");
    for (i = 0; i < 256; i++) data[i] = i;
    test_write(1, xwrite(0, data, 256), 256);
    write_offset = 0;
    write_interrupt = 1;
    memset(data, 0, 256);
    test_write(2, xwrite(0, data, 256), 256);
    write_offset = 0;
    for (i = 0; i < 32; i++) data[i] = i * 2;
    test_write(3, xwrite(0, data, 32), 32);
    for (i = 32; i < 65; i++) data[i] = i * 2;
    test_write(4, xwrite(0, data + 32, 33), 33);
    write_offset = 0;
    write_interrupt = 0;
    memset(data, 0, 256);
    iov[0].iov_base = data;
    iov[0].iov_len = 256;
    test_write(5, xwritev(0, iov, 1), 256);
    write_offset = 0;
    for (i = 0; i < 256; i++) data[i] = i;
    iov[0].iov_len = 128;
    iov[1].iov_base = &data[128];
    iov[1].iov_len = 16;
    iov[2].iov_base = &data[144];
    iov[2].iov_len = 112;
    test_write(6, xwritev(0, iov, 3), 256);
    write_offset = 0;
    write_interrupt = 1;
    memset(data, 0, 256);
    iov[0].iov_len = 32;
    iov[1].iov_base = &data[32];
    iov[1].iov_len = 224;
    test_write(7, xwritev(0, iov, 2), 256);
    for (i = 0; i < 32; i++) data[i] = i * 2;
    write_offset = 0;
    test_write(8, xwritev(0, iov, 1), 32);
    for (i = 32; i < 65; i++) data[i] = i * 2;
    iov[0].iov_base = &data[32];
    iov[0].iov_len = 16;
    iov[1].iov_base = &data[48];
    iov[1].iov_len = 1;
    iov[2].iov_base = &data[49];
    iov[2].iov_len = 8;
    iov[3].iov_base = &data[57];
    iov[3].iov_len = 8;
    test_write(9, xwritev(0, iov, 4), 33);
    write_offset = 0;
    write_interrupt = 0;
    write_fail = 1;
    test_write(10, xwrite(0, data + 1, 255), -1);
    iov[0].iov_base = data + 1;
    iov[0].iov_len = 255;
    test_write(11, xwritev(0, iov, 1), -1);
    return 0;
}
