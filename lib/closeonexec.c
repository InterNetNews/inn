/*  $Id$
**
**  Portably set or clear close-on-exec status of a file descriptor,
**  ignoring errors.
*/
#include "config.h"
#include "libinn.h"

#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <sys/types.h>
# include <fcntl.h>
# ifndef FD_CLOEXEC
#  define FD_CLOEXEC 1
# endif
#else
# ifdef HAVE_SGTTY_H
#  include <sgtty.h>
# endif
# ifdef HAVE_SYS_FILIO_H
#  include <sys/filio.h>
# endif
#endif

#ifdef HAVE_FCNTL_H

/*
**  Technically, one is supposed to retrieve the flags, add FD_CLOEXEC, and
**  then set them, although I've never seen a system with any flags other
**  than close-on-exec.  Do it right anyway; it's not that expensive.
*/
void
CloseOnExec(int fd, int flag)
{
    int oerrno;
    int oflag;

    oerrno = errno;
    oflag = fcntl(fd, F_GETFD, 0);
    if (oflag < 0) {
        errno = oerrno;
        return;
    }
    fcntl(fd, F_SETFD, flag ? (oflag | FD_CLOEXEC) : (oflag & ~FD_CLOEXEC));
    errno = oerrno;
}

#else /* !HAVE_FCNTL_H */

/*
**  If we don't have fcntl.h, assume we'll be able to find a definition of
**  FIOCLEX somewhere.
*/
void
CloseOnExec(int fd, int flag)
{
    int oerrno;

    oerrno = errno;
    ioctl(fd, (flag ? FIOCLEX : FIONCLEX), (char *) 0);
    errno = oerrno;
}

#endif /* !HAVE_FCNTL_H */
