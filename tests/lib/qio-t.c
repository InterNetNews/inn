/* $Id$ */
/* Test suite for the Quick I/O library */

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/messages.h"
#include "inn/qio.h"
#include "libinn.h"
#include "libtest.h"

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
    size_t size = 8192;
    QIOSTATE *qio;
    bool success;

#if HAVE_STRUCT_STAT_ST_BLKSIZE
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

#if HAVE_STRUCT_STAT_ST_BLKSIZE
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
       long.  Finally, put out another short line. */
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
    output(fd, line, 256);
    close(fd);

    test_init(36);

    /* Now make sure we can read all that back correctly. */
    qio = QIOopen(".testout");
    ok(1, qio != NULL);
    ok(2, !QIOerror(qio));
    ok(3, QIOfileno(qio) > 0);
    if (unlink(".testout") < 0)
        sysdie("Can't unlink .testout");
    for (success = true, i = 0; i < count; i++) {
        result = QIOread(qio);
        success = (success && !QIOerror(qio) && (QIOlength(qio) == 255)
                   && !strcmp(result, (char *) out));
    }
    ok(4, success);
    ok(5, QIOtell(qio) == (off_t) size);
    result = QIOread(qio);
    if (strlen(result) < size - 1) {
        ok(6, false);
    } else {
        for (success = true, i = 0; i < count - 1; i++)
            success = success && !memcmp(result + i * 256, data, 256);
        success = success && !memcmp(result + i * 256, data, 255);
        ok(6, success);
    }
    ok(7, QIOtell(qio) == (off_t) (2 * size));
    result = QIOread(qio);
    ok(8, !QIOerror(qio));
    ok(9, QIOlength(qio) == 0);
    ok(10, *result == 0);
    result = QIOread(qio);
    if (strlen(result) < size - 1) {
        ok(11, false);
    } else {
        for (success = true, i = 0; i < count - 1; i++)
            success = success && !memcmp(result + i * 256, data, 256);
        success = success && !memcmp(result + i * 256, data, 255);
        ok(11, success);
    }
    ok(12, QIOtell(qio) == (off_t) (3 * size + 1));
    result = QIOread(qio);
    ok(13, !QIOerror(qio));
    ok(14, QIOlength(qio) == 127);
    ok(15, strlen(result) == 127);
    ok(16, !memcmp(result, data, 127));
    for (success = true, i = 0; i < count; i++) {
        result = QIOread(qio);
        success = (success && !QIOerror(qio) && (QIOlength(qio) == 255)
                   && !strcmp(result, (char *) out));
    }
    ok(17, success);
    ok(18, QIOtell(qio) == (off_t) (4 * size + 129));
    result = QIOread(qio);
    ok(19, !result);
    ok(20, QIOerror(qio));
    ok(21, QIOtoolong(qio));
    result = QIOread(qio);
    ok(22, !QIOerror(qio));
    ok(23, QIOlength(qio) == 255);
    ok(24, strlen(result) == 255);
    ok(25, !memcmp(result, line, 255));
    result = QIOread(qio);
    ok(26, !result);
    ok(27, !QIOerror(qio));
    ok(28, QIOrewind(qio) == 0);
    ok(29, QIOtell(qio) == 0);
    result = QIOread(qio);
    ok(30, !QIOerror(qio));
    ok(31, QIOlength(qio) == 255);
    ok(32, strlen(result) == 255);
    ok(33, !strcmp(result, (char *) out));
    ok(34, QIOtell(qio) == 256);
    fd = QIOfileno(qio);
    QIOclose(qio);
    ok(35, close(fd) < 0);
    ok(36, errno == EBADF);

    return 0;
}
