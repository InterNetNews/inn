/* $Id$ */
/* pread test suite. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "libinn.h"
#include "libtest.h"

ssize_t test_pread(int fd, void *buf, size_t nbyte, off_t offset);

int
main(void)
{
    unsigned char buf[256], result[256];
    unsigned char c;
    int i, fd;
    ssize_t status;
    off_t position;

    for (c = 0, i = 0; i < 256; i++, c++)
        buf[i] = c;
    fd = open(".testout", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) sysdie("Can't create .testout");
    if (unlink(".testout") < 0) sysdie("Can't unlink .testout");
    if (xwrite(fd, buf, 256) < 0) sysdie("Can't write to .testout");
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysdie("Can't rewind .testout");
    memset(result, 0, sizeof(result));

    puts("6");

    status = test_pread(fd, result, 128, 128);
    ok(1, (status == 128) && !memcmp(result, buf + 128, 128));
    status = read(fd, result, 64);
    ok(2, (status == 64) && !memcmp(result, buf, 64));
    status = test_pread(fd, result, 1, 256);
    ok(3, status == 0);
    status = test_pread(fd, result, 256, 0);
    ok(4, (status == 256) && !memcmp(result, buf, 256));
    position = lseek(fd, 0, SEEK_CUR);
    ok(5, position == 64);

    close(20);
    errno = 0;
    status = test_pread(20, result, 1, 0);
    ok(6, (status == -1) && (errno == EBADF));

    return 0;
}
