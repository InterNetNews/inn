/* $Id$ */
/* Test suite for the Quick I/O library */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "libinn.h"
#include "inn/qio.h"

static void
ok(int n, int success)
{
    printf("%sok %d\n", success ? "" : "not ", n);
}

static void
output(int fd, const void *data, size_t size)
{
    if (xwrite(fd, data, size) < 0)
        sysdie("Can't write to .testout");
}

int
main(void)
{
    unsigned char data[256], line[256], out[256];
    unsigned char c;
    char *result;
    int i, count, fd;
    ssize_t status;
    size_t size = 8192;
    QIOSTATE *qio;
    bool success;

#if HAVE_ST_BLKSIZE
    struct stat st;
#endif

    for (c = 1, i = 0; i < 255; i++, c++)
        data[i] = c;
    data[9] = ' ';
    data[255] = '\255';
    memcpy(line, data, 255);
    line[255] = '\n';
    memcpy(out, data, 255);
    out[255] = '\0';
    fd = open(".testout", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) sysdie("Can't create .testout");

#if HAVE_ST_BLKSIZE
    /* Mostly duplicate the code from qio.c so that we can test with lines
       exactly as large as the buffer. */
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
        size = st.st_blksize;
        if (size > 4 * 8192 || size < 4096)
            size = 8192;
    }
#endif /* HAVE_ST_BLKSIZE */

    /* Start with small, equally sized lines exactly equal to the buffer.
       Then a line equal in size to the buffer, then a short line and
       another line equal in size to the buffer, then a half line and lines
       repeated to fill another buffer, then a line that's one character too
       long. */
    count = size / 256;
    for (i = 0; i < count; i++)
        output(fd, line, 256);
    for (i = 0; i < count - 1; i++)
        output(fd, data, 256);
    output(fd, line, 256);
    output(fd, "\n", 1);
    for (i = 0; i < count - 1; i++)
        output(fd, data, 256);
    output(fd, line, 256);
    output(fd, data, 127);
    output(fd, "\n", 1);
    for (i = 0; i < count; i++)
        output(fd, line, 256);
    for (i = 0; i < count; i++)
        output(fd, data, 256);
    output(fd, "\n", 1);
    close(fd);

    puts("17");

    /* Now make sure we can read all that back correctly. */
    qio = QIOopen(".testout");
    ok(1, (qio != NULL) && !QIOerror(qio) && (QIOfileno(qio) > 0));
    if (unlink(".testout") < 0) sysdie("Can't unlink .testout");
    for (success = true, i = 0; i < count; i++) {
        result = QIOread(qio);
        success = (success && !QIOerror(qio) && (QIOlength(qio) == 255)
                   && !strcmp(result, out));
    }
    ok(2, success);
    ok(3, QIOtell(qio) == size);
    result = QIOread(qio);
    if (strlen(result) < size - 1) {
        ok(4, false);
    } else {
        for (success = true, i = 0; i < count - 1; i++)
            success = success && !memcmp(result + i * 256, data, 256);
        success = success && !memcmp(result + i * 256, data, 255);
        ok(4, success);
    }
    ok(5, QIOtell(qio) == 2 * size);
    result = QIOread(qio);
    ok(6, !QIOerror(qio) && QIOlength(qio) == 0 && *result == 0);
    result = QIOread(qio);
    if (strlen(result) < size - 1) {
        ok(7, false);
    } else {
        for (success = true, i = 0; i < count - 1; i++)
            success = success && !memcmp(result + i * 256, data, 256);
        success = success && !memcmp(result + i * 256, data, 255);
        ok(7, success);
    }
    ok(8, QIOtell(qio) == 3 * size + 1);
    result = QIOread(qio);
    ok(9, (!QIOerror(qio) && QIOlength(qio) == 127 && strlen(result) == 127
           && !memcmp(result, data, 127)));
    for (success = true, i = 0; i < count; i++) {
        result = QIOread(qio);
        success = (success && !QIOerror(qio) && (QIOlength(qio) == 255)
                   && !strcmp(result, out));
    }
    ok(10, success);
    ok(11, QIOtell(qio) == 4 * size + 129);
    result = QIOread(qio);
    ok(12, !result && QIOerror(qio) && QIOtoolong(qio));
    ok(13, QIOrewind(qio) == 0);
    ok(14, QIOtell(qio) == 0);
    result = QIOread(qio);
    ok(15, (!QIOerror(qio) && QIOlength(qio) == 255 && strlen(result) == 255
            && !strcmp(result, out)));
    ok(16, QIOtell(qio) == 256);
    fd = QIOfileno(qio);
    QIOclose(qio);
    ok(17, close(fd) < 0 && errno == EBADF);

    return 0;
}
