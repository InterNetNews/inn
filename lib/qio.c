/*  $Id$
**
**  Quick I/O package.
**
**  A set of routines optimized for reading through files line by line.
**  This package uses internal buffering like stdio, but is even more
**  aggressive about its buffering.  The basic read call reads a single line
**  and returns the whole line, provided that it can fit in the buffer.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "inn/qio.h"
#include "libinn.h"

/* A reasonable default buffer size to use. */
#define QIO_BUFFERSIZE  8192

/*
**  Given a file descriptor, return a reasonable buffer size to use for that
**  file.  Uses st_blksize if available and reasonable, QIO_BUFFERSIZE
**  otherwise.
*/
static size_t
buffer_size(int fd UNUSED)
{
    size_t size = QIO_BUFFERSIZE;

#if HAVE_STRUCT_STAT_ST_BLKSIZE
    struct stat st;

    /* The Solaris 2.6 man page says that st_blksize is not defined for
       block or character special devices (and could contain garbage), so
       only use this value for regular files. */
    if (fstat(fd, &st) == 0 && S_ISREG(st.st_mode)) {
        size = st.st_blksize;
        if (size > (4 * QIO_BUFFERSIZE) || size < (QIO_BUFFERSIZE / 2))
            size = QIO_BUFFERSIZE;
    }
#endif /* HAVE_STRUCT_STAT_ST_BLKSIZE */

    return size;
}


/*
**  Open a quick file from a descriptor.
*/
QIOSTATE *
QIOfdopen(const int fd)
{
    QIOSTATE *qp;

    qp = xmalloc(sizeof(*qp));
    qp->_fd = fd;
    qp->_length = 0;
    qp->_size = buffer_size(fd);
    qp->_buffer = xmalloc(qp->_size);
    qp->_start = qp->_buffer;
    qp->_end = qp->_buffer;
    qp->_count = 0;
    qp->_flag = QIO_ok;

    return qp;
}


/*
**  Open a quick file from a file name.
*/
QIOSTATE *
QIOopen(const char *name)
{
    int fd;

    fd = open(name, O_RDONLY);
    if (fd < 0)
        return NULL;
    return QIOfdopen(fd);
}


/*
**  Close an open quick file.
*/
void
QIOclose(QIOSTATE *qp)
{
    close(qp->_fd);
    free(qp->_buffer);
    free(qp);
}


/*
**  Rewind a quick file.  Reads the first buffer full of data automatically,
**  anticipating the first read from the file.  Returns -1 on error, 0 on
**  success.
*/
int
QIOrewind(QIOSTATE *qp)
{
    ssize_t nread;

    if (lseek(qp->_fd, 0, SEEK_SET) < 0)
        return -1;
    nread = read(qp->_fd, qp->_buffer, qp->_size);
    if (nread < 0)
        return nread;
    qp->_count = nread;
    qp->_start = qp->_buffer;
    qp->_end = qp->_buffer + nread;
    return 0;
}


/*
**  Get the next newline-terminated line from a quick file, replacing the
**  newline with a nul.  Returns a pointer to that line on success and NULL
**  on failure or end of file, with _flag set appropriately.
*/
char *
QIOread(QIOSTATE *qp)
{
    char *p, *line;
    ssize_t nread;
    size_t nleft;

    /* Loop until we get a result or fill the buffer. */
    qp->_flag = QIO_ok;
    while (1) {
        nleft = qp->_end - qp->_start;

        /* If nleft <= 0, the buffer currently contains no data that hasn't
           previously been returned by QIOread, so we can overwrite the
           buffer with new data.  Otherwise, first check the existing data
           to see if we have a full line. */
        if (nleft <= 0) {
            qp->_start = qp->_buffer;
            qp->_end = qp->_buffer;
        } else {
            p = memchr(qp->_start, '\n', nleft);
            if (p != NULL) {
                *p = '\0';
                qp->_length = p - qp->_start;
                line = qp->_start;
                qp->_start = p + 1;
                return (qp->_flag == QIO_long) ? NULL : line;
            }

            /* Not there.  See if our buffer is full.  If so, tag as having
               seen too long of a line.  This will cause us to keep reading
               as normal until we finally see the end of a line and then
               return NULL. */
            if (nleft >= qp->_size) {
                qp->_flag = QIO_long;
                qp->_start = qp->_end;
                nleft = 0;
            }

            /* We need to read more data.  If there's read data in buffer,
               then move the unread data down to the beginning of the buffer
               first. */
            if (qp->_start > qp->_buffer) {
                if (nleft > 0)
                    memmove(qp->_buffer, qp->_start, nleft);
                qp->_start = qp->_buffer;
                qp->_end = qp->_buffer + nleft;
            }
        }

        /* Read in some more data, and then let the loop try to find the
           newline again or discover that the line is too long. */
        do {
            nread = read(qp->_fd, qp->_end, qp->_size - nleft);
        } while (nread == -1 && errno == EINTR);
        if (nread <= 0) {
            if (nread < 0)
                qp->_flag = QIO_error;
            return NULL;
        }
        qp->_count += nread;
        qp->_end += nread;
    }
}
