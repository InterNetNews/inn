/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"

#ifdef HAVE_FLOCK
# include <sys/file.h>
#elif HAVE_LOCKF
# if defined(HAVE_UNISTD_H)
#  include <unistd.h>
# endif /* defined(HAVE_UNISTD_H) */
# include <fcntl.h>
#elif HAVE_FCNTL
# include <fcntl.h>
# if	!defined(SEEK_SET)
#  define SEEK_SET	0
# endif	/* !defined(SEEK_SET) */
#else
# include <sys/stat.h>
#endif	

/*
**  Try to lock a file descriptor.
*/
int LockFile(int fd, BOOL Block)
{
#ifdef HAVE_FLOCK
    return flock(fd, Block ? LOCK_EX : LOCK_EX | LOCK_NB);
#elif HAVE_LOCKF
    return lockf(fd, Block ? F_LOCK : F_TLOCK, 0L);
#elif HAVE_FCNTL
    struct flock	fl;

    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 0;
    return fcntl(fd, Block ? F_SETLKW : F_SETLK, &fl);
#else 
    struct stat	Sb;

    return fstat(fd, &Sb);
#endif	

}
