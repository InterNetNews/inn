/*  $Id$
**
**  Writes at an offset.
**
**  Replacement for system pwrite(2) call, if missing.  pwrite(2) is defined
**  as not changing the file pointer.  Note that this is not atomic;
**  threaded programs should require the system pwrite(2).
*/
#include "config.h"
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef SEEK_SET
# define SEEK_SET 0
#endif
#ifndef SEEK_CUR
# define SEEK_CUR 1
#endif
#ifndef SEEK_END
# define SEEK_END 2
#endif

ssize_t
pwrite(int fd, void *buf, size_t nbyte, OFFSET_T offset)
{
    OFFSET_T    current;
    ssize_t     nwritten;

    current = lseek(fd, 0, SEEK_CUR);
    if (current < 0 || lseek(fd, offset, SEEK_SET) < 0)
        return -1;

    nwritten = write(fd, buf, nbyte);

    /* Ignore errors in restoring the file position; this isn't ideal, but
       reporting a failed write when the write succeeded is worse. */
    lseek(fd, current, SEEK_SET);
    return nwritten;
}
