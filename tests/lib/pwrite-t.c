/* $Id$ */
/* pwrite test suite. */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/messages.h"
#include "libtest.h"

ssize_t test_pwrite(int fd, const void *buf, size_t nbyte, off_t offset);

int
main(void)
{
    unsigned char buf[256], result[256];
    unsigned char c;
    int i, fd;
    ssize_t status;

    for (c = 0, i = 0; i < 256; i++, c++)
        buf[i] = c;
    fd = open(".testout", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        sysdie("Can't create .testout");
    if (unlink(".testout") < 0)
        sysdie("Can't unlink .testout");
    memset(result, 0, sizeof(result));

    puts("6");

    ok(1, test_pwrite(fd, buf + 129, 127, 129) == 127);
    ok(2, write(fd, buf, 64) == 64);
    ok(3, test_pwrite(fd, buf + 64, 65, 64) == 65);
    status = read(fd, result, 64);
    ok(4, (status == 64) && !memcmp(result, buf + 64, 64));
        
    if (lseek(fd, 0, SEEK_SET) == (off_t) -1)
        sysdie("Can't rewind .testout");
    status = read(fd, result, 256);
    ok(5, (status == 256) && !memcmp(result, buf, 256));

    close(20);
    errno = 0;
    status = test_pwrite(20, result, 1, 0);
    ok(6, (status == -1) && (errno == EBADF));

    return 0;
}
