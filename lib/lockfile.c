/*  $Id$
**
**  Lock a file or a range in a file, portably.
**
**  Provides a LockFile() function to lock a file, and if fcntl() is
**  available, a LockRange() function to lock a range of a file.  Prefer
**  fcntl() if available.  If not, fall back on flock() and then to
**  lockf() as a last resort.  If all fails, don't lock at all, and return
**  true if fstat() succeeds on the file descriptor.
*/

#include "config.h"
#include "libinn.h"

#ifdef HAVE_FCNTL
# include <stdio.h>
# include <sys/types.h>
# ifdef HAVE_UNISTD_H
#  include <unistd.h>
# endif
# include <fcntl.h>
# ifndef SEEK_SET
#  define SEEK_SET 0
# endif

int
LockFile(int fd, BOOL block)
{
    struct flock fl;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return fcntl(fd, block ? F_SETLKW : F_SETLK, &fl);
}

#else /* !HAVE_FCNTL */
# ifdef HAVE_FLOCK
#  include <sys/file.h>

int
LockFile(int fd, BOOL block)
{
    return flock(fd, LOCK_EX | (block ? 0 : LOCK_NB));
}

# else /* !HAVE_FLOCK */
#  ifdef HAVE_LOCKF
#   ifdef HAVE_UNISTD_H
#    include <unistd.h>
#   endif
#   ifdef HAVE_FCNTL_H
#    include <fcntl.h>
#   endif

int
LockFile(int fd, BOOL block)
{
    return lockf(fd, block ? F_LOCK : F_TLOCK, 0);
}

#  else /* !HAVE_LOCKF */
#   include <sys/stat.h>

int
LockFile(int fd, BOOL block)
{
    struct stat	Sb;

    return fstat(fd, &Sb);
}

#  endif /* !HAVE_LOCKF */
# endif /* !HAVE_FLOCK */
#endif /* !HAVE_FCNTL */


#ifdef HAVE_FCNTL
BOOL
LockRange(int fd, LOCKTYPE type, BOOL block, OFFSET_T offset, OFFSET_T size)
{
    struct flock        fl;
    int                 ret;

    switch (type) {
        case LOCK_READ:         fl.l_type = F_RDLCK;    break;
        case LOCK_WRITE:        fl.l_type = F_WRLCK;    break;
        default:
        case LOCK_UNLOCK:       fl.l_type = F_UNLCK;    break;
    }

    fl.l_whence = SEEK_SET;
    fl.l_start = offset;
    fl.l_len = size;

    ret = fcntl(fd, block ? F_SETLKW : F_SETLK, &fl);
    return (ret != -1);
}
#endif /* HAVE_FCNTL */
