/*  $Id$
**
**  Lock a file or a range in a file, portably.
**
**  Provides a lock_file() function to lock a file, and if fcntl() is
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

BOOL
lock_file(int fd, LOCKTYPE type, BOOL block)
{
    struct flock fl;

    switch(type) {
        case LOCK_READ:         fl.l_type = F_RDLCK;    break;
        case LOCK_WRITE:        fl.l_type = F_WRLCK;    break;
        default:
        case LOCK_UNLOCK:       fl.l_type = F_UNLCK;    break;
    }
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return (fcntl(fd, block ? F_SETLKW : F_SETLK, &fl) == 0);
}

#else /* !HAVE_FCNTL */
# ifdef HAVE_FLOCK
#  include <sys/file.h>

BOOL
lock_file(int fd, LOCKTYPE type, BOOL block)
{
    int mode;
    switch(type) {
        case LOCK_READ:    mode = LOCK_SH | (block ? 0 : LOCK_NB);    break;
        case LOCK_WRITE:   mode = LOCK_EX | (block ? 0 : LOCK_NB);    break;
        default:
        case LOCK_UNLOCK:  mode = LOCK_UN;    break;
    }
    return (flock(fd, mode) == 0);
}

# else /* !HAVE_FLOCK */
#  ifdef HAVE_LOCKF
#   ifdef HAVE_UNISTD_H
#    include <unistd.h>
#   endif
#   ifdef HAVE_FCNTL_H
#    include <fcntl.h>
#   endif

BOOL
lock_file(int fd, LOCKTYPE type, BOOL block)
{
    BOOL ret;
    int mode;
    struct stat	Sb;
    off_t pos = lseek(fd, 0, SEEK_CUR);
    switch(type) {
        case LOCK_READ:   return fstat(fd, &Sb);
        case LOCK_WRITE:  mode = block ? F_LOCK : F_TLOCK;    break;
        default:
        case LOCK_UNLOCK: mode = F_ULOCK;    break;
    }    
    if(pos != -1)
	lseek(fd, 0, SEEK_SET);
    ret = (lockf(fd, mode, 0) == 0);
    if(pos != -1)
	lseek(fd, pos, SEEK_SET);
    return ret;
}

#  else /* !HAVE_LOCKF */
#   include <sys/stat.h>

BOOL
lock_file(int fd, LOCKTYPE type, BOOL block)
{
    struct stat	Sb;

    return (fstat(fd, &Sb) == 0);
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
