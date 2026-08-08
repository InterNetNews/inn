/*
**  Test suite for the robust descriptor reader.
**
**  Written by Kevin Bowling in 2026.
*/

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/libinn.h"
#include "tap/basic.h"

int
main(void)
{
    static const char input[] = "test data";
    struct stat st;
    char path[] = "readin-XXXXXX";
    char *contents;
    char buffer[sizeof(input)];
    int fd, fds[2], oerrno;

    if (pipe(fds) < 0)
        sysbail("cannot create pipe");
    plan(14);

    is_int(sizeof(input), write(fds[1], input, sizeof(input)),
           "write test data");
    close(fds[1]);
    is_int(0, xread(fds[0], buffer, sizeof(buffer)), "read complete input");
    ok(memcmp(buffer, input, sizeof(input)) == 0, "input matches");
    close(fds[0]);

    errno = 0;
    is_int(-1, xread(-1, buffer, -1), "reject a negative length");
    oerrno = errno;
    is_int(EINVAL, oerrno, "negative length sets errno");

    if (pipe(fds) < 0)
        sysbail("cannot create pipe");
    is_int(1, write(fds[1], input, 1), "write partial input");
    close(fds[1]);
    errno = 0;
    is_int(-1, xread(fds[0], buffer, 2), "reject premature EOF");
    oerrno = errno;
    is_int(EIO, oerrno, "premature EOF sets errno");
    close(fds[0]);

    fd = mkstemp(path);
    if (fd < 0)
        sysbail("cannot create temporary input file");
    is_int(sizeof(input) - 1, write(fd, input, sizeof(input) - 1),
           "write temporary input file");
    close(fd);
    contents = ReadInFile(path, &st);
    if (contents == NULL)
        sysbail("cannot read temporary input file");
    is_int(sizeof(input) - 1, st.st_size, "report input file size");
    is_string(input, contents, "read and terminate input file");
    free(contents);

    fd = open(path, O_WRONLY);
    if (fd < 0)
        sysbail("cannot reopen temporary input file");
    errno = 0;
    contents = ReadInDescriptor(fd, NULL);
    oerrno = errno;
    ok(contents == NULL, "descriptor read failure is reported");
    is_int(EBADF, oerrno, "descriptor read failure preserves errno");
    ok(fcntl(fd, F_GETFD) >= 0, "descriptor remains owned by caller");
    close(fd);
    unlink(path);

    return 0;
}
