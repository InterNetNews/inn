/* Test suite for the Quick I/O library */

#define LIBTEST_NEW_FORMAT 1

#include "portable/system.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/libinn.h"
#include "inn/qio.h"
#include "tap/basic.h"


static void
output(int fd, const void *data, size_t size)
{
    if (xwrite(fd, data, size) < 0)
        sysbail("Can't write to .testout");
}


int
main(void)
{
    unsigned char data[256], line[256], out[256];
    unsigned char c;
    char *result;
    int i, count, fd;
    QIOSTATE *qio;
    bool success;

    for (c = 1, i = 0; i < 255; i++, c++)
        data[i] = c;
    data[9] = ' ';
    data[255] = '\255';
    memcpy(line, data, 255);
    line[255] = '\n';
    memcpy(out, data, 255);
    out[255] = '\0';
    fd = open(".testout", O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
        sysbail("Can't create .testout");

    /* Start with small, equally sized lines exactly equal to the buffer.
       Then a line equal in size to the buffer, then a short line and
       another line equal in size to the buffer, then a half line and lines
       repeated to fill another buffer, then a line that's one character too
       long.  Finally, put out another short line. */
    count = QIO_BUFFERSIZE / 256;
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

    plan(36);

    /* Now make sure we can read all that back correctly. */
    qio = QIOopen(".testout");
    ok(qio != NULL, "QIOpen works");
    ok(!QIOerror(qio), "...and there is no error state");
    ok(QIOfileno(qio) > 0, "...and QIOfileno returns something valid");
    for (success = true, i = 0; i < count; i++) {
        result = QIOread(qio);
        success = (success && !QIOerror(qio) && (QIOlength(qio) == 255)
                   && !strcmp(result, (char *) out));
    }
    ok(success, "Able to read the first QIO_BUFFERSIZE of data");
    is_int(QIOtell(qio), (off_t) QIO_BUFFERSIZE, "QIOtell is correct");
    result = QIOread(qio);
    if (strlen(result) < QIO_BUFFERSIZE - 1) {
        ok(false, "Read largest possible line");
    } else {
        for (success = true, i = 0; i < count - 1; i++)
            success = success && !memcmp(result + i * 256, data, 256);
        success = success && !memcmp(result + i * 256, data, 255);
        ok(success, "Read largest possible line");
    }
    is_int(QIOtell(qio), (off_t) (2 * QIO_BUFFERSIZE), "QIOtell is correct");
    result = QIOread(qio);
    ok(!QIOerror(qio), "No error on reading an empty line");
    is_int(QIOlength(qio), 0, "Length of the line is 0");
    is_string(result, "", "...and result is an empty string");
    result = QIOread(qio);
    if (strlen(result) < QIO_BUFFERSIZE - 1) {
        ok(false, "Read largest line again");
    } else {
        for (success = true, i = 0; i < count - 1; i++)
            success = success && !memcmp(result + i * 256, data, 256);
        success = success && !memcmp(result + i * 256, data, 255);
        ok(success, "Read largest line again");
    }
    is_int(QIOtell(qio), (off_t) (3 * QIO_BUFFERSIZE + 1),
           "QIOtell is correct");
    result = QIOread(qio);
    ok(!QIOerror(qio), "No error on a shorter read");
    is_int(QIOlength(qio), 127, "Length is 127");
    is_int(strlen(result), 127, "String is 127");
    ok(!memcmp(result, data, 127), "Data is correct");
    for (success = true, i = 0; i < count; i++) {
        result = QIOread(qio);
        success = (success && !QIOerror(qio) && (QIOlength(qio) == 255)
                   && !strcmp(result, (char *) out));
    }
    ok(success, "Able to read another batch of lines");
    is_int(QIOtell(qio), (off_t) (4 * QIO_BUFFERSIZE + 129),
           "QIOtell is correct");
    result = QIOread(qio);
    ok(!result, "Failed to read too long of line");
    ok(QIOerror(qio), "Error reported");
    ok(QIOtoolong(qio), "...and too long flag is set");
    result = QIOread(qio);
    ok(!QIOerror(qio), "Reading again succeeds");
    is_int(QIOlength(qio), 255, "...and returns the next block");
    is_int(strlen(result), 255, "...and length is correct");
    ok(!memcmp(result, line, 255), "...and data is correct");
    result = QIOread(qio);
    ok(!result, "End of file reached");
    ok(!QIOerror(qio), "...with no error");
    is_int(QIOrewind(qio), 0, "QIOrewind works");
    is_int(QIOtell(qio), 0, "...and QIOtell is correct");
    result = QIOread(qio);
    ok(!QIOerror(qio), "Reading the first line works");
    is_int(QIOlength(qio), 255, "...and QIOlength is correct");
    is_int(strlen(result), 255, "...and the length is correct");
    ok(!strcmp(result, (char *) out), "...and the data is correct");
    is_int(QIOtell(qio), 256, "...and QIOtell is correct");
    fd = QIOfileno(qio);
    QIOclose(qio);
    ok(close(fd) < 0, "QIOclose closed the file descriptor");
    is_int(errno, EBADF, "...as confirmed by errno");

    if (unlink(".testout") < 0)
        sysbail("Can't unlink .testout");

    return 0;
}
