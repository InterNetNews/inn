/* $Id$ */
/* mkstemp test suite */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <sys/stat.h>

#include "libtest.h"

int test_mkstemp(char *template);

int
main(void)
{
    int fd;
    char template[] = "tsXXXXXXX";
    char tooshort[] = "XXXXX";
    char bad1[] = "/foo/barXXXXX";
    char bad2[] = "/foo/barXXXXXX.out";
    char buffer[256];
    struct stat st1, st2;
    ssize_t length;

    test_init(20);

    /* First, test a few error messages. */
    errno = 0;
    ok_int(1, -1, test_mkstemp(tooshort));
    ok(2, errno == EINVAL);
    ok_string(3, "XXXXX", tooshort);
    errno = 0;
    ok_int(4, -1, test_mkstemp(bad1));
    ok(5, errno == EINVAL);
    ok_string(6, "/foo/barXXXXX", bad1);
    errno = 0;
    ok_int(7, -1, test_mkstemp(bad2));
    ok(8, errno == EINVAL);
    ok_string(9, "/foo/barXXXXXX.out", bad2);
    errno = 0;

    /* Now try creating a real file. */
    fd = test_mkstemp(template);
    ok(10, fd >= 0);
    ok(11, strcmp(template, "tsXXXXXXX") != 0);
    ok(12, strncmp(template, "tsX", 3) == 0);
    ok(13, access(template, F_OK) == 0);

    /* Make sure that it's the same file as template refers to now. */
    ok(14, stat(template, &st1) == 0);
    ok(15, fstat(fd, &st2) == 0);
    ok(16, st1.st_ino == st2.st_ino);
    unlink(template);

    /* Make sure the open mode is correct. */
    length = strlen(template);
    ok(17, write(fd, template, length) == length);
    ok(18, lseek(fd, 0, SEEK_SET) == 0);
    ok(19, read(fd, buffer, length) == length);
    buffer[length] = '\0';
    ok_string(20, template, buffer);
    close(fd);

    return 0;
}
