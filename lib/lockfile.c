/*  $Id$
**
**  Lock a file or a range in a file.
**
**  Provides lock_file and lock_range functions to lock or unlock a file or
**  ranges within a file with a more convenient syntax than fcntl.  Assume
**  that fcntl is available.
*/

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include "libinn.h"

bool
lock_file(int fd, enum locktype type, bool block)
{
    return lock_range(fd, type, block, 0, 0);
}

bool
lock_range(int fd, enum locktype type, bool block, off_t offset, off_t size)
{
    struct flock fl;
    int status;

    switch (type) {
        case LOCK_READ:         fl.l_type = F_RDLCK;    break;
        case LOCK_WRITE:        fl.l_type = F_WRLCK;    break;
        default:
        case LOCK_UNLOCK:       fl.l_type = F_UNLCK;    break;
    }

    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = size;

    status = fcntl(fd, block ? F_SETLKW : F_SETLK, &fl);
    return (status != -1);
}
