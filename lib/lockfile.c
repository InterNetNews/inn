/*  $Id$
**
**  Lock a file or a range in a file.
**
**  Provides inn_lock_file and inn_lock_range functions to lock or unlock a
**  file or ranges within a file with a more convenient syntax than fcntl.
**  Assume that fcntl is available.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>

#include "libinn.h"

bool
inn_lock_file(int fd, enum inn_locktype type, bool block)
{
    return inn_lock_range(fd, type, block, 0, 0);
}

bool
inn_lock_range(int fd, enum inn_locktype type, bool block, off_t offset,
               off_t size)
{
    struct flock fl;
    int status;

    switch (type) {
        case INN_LOCK_READ:     fl.l_type = F_RDLCK;    break;
        case INN_LOCK_WRITE:    fl.l_type = F_WRLCK;    break;
        default:
        case INN_LOCK_UNLOCK:   fl.l_type = F_UNLCK;    break;
    }

    do {
	fl.l_whence = SEEK_SET;
	fl.l_start = offset;
	fl.l_len = size;

	status = fcntl(fd, block ? F_SETLKW : F_SETLK, &fl);
    } while (status == -1 && errno == EINTR);
    return (status != -1);
}
