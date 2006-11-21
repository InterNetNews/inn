/*  $Id$
**
**  Set or clear file descriptor flags.
**
**  Simple functions (wrappers around fcntl) to set or clear file descriptor
**  flags like close on exec or nonblocking I/O.
*/

#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"
#include <errno.h>
#include <fcntl.h>

/*
**  Set a file to close on exec.
**
**  One is supposed to retrieve the flags, add FD_CLOEXEC, and then set
**  them, although I've never seen a system with any flags other than
**  close-on-exec.  Do it right anyway; it's not that expensive.  Avoid
**  changing errno.  Errors are ignored, since it generally doesn't cause
**  significant harm to fail.
*/
void
close_on_exec(int fd, bool flag)
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


/*
**  Set a file descriptor to nonblocking (or clear the nonblocking flag if
**  flag is false).
**
**  Always use O_NONBLOCK; O_NDELAY is *not* the same thing historically.
**  The semantics of O_NDELAY are that if the read would block, it returns 0
**  instead.  This is indistinguishable from an end of file condition.
**  POSIX added O_NONBLOCK, which requires read to return -1 and set errno
**  to EAGAIN, which is what we want.
**
**  FNDELAY (4.3BSD) originally did the correct thing, although it has a
**  different incompatibility (affecting all users of a socket rather than
**  just a file descriptor and returning EWOULDBLOCK instead of EAGAIN) that
**  we don't care about in INN.  Using it is *probably* safe, but BSD should
**  also have the ioctl, and at least on Solaris FNDELAY does the same thing
**  as O_NDELAY, not O_NONBLOCK.  So if we don't have O_NONBLOCK, fall back
**  to the ioctl instead.
**
**  Reference:  Stevens, Advanced Unix Programming, pg. 364.
**
**  Note that O_NONBLOCK is known not to work on earlier versions of ULTRIX,
**  SunOS, and AIX, possibly not setting the socket nonblocking at all,
**  despite the fact that they do define it.  It works in later SunOS and,
**  current AIX, however, and a 1999-10-25 survey of current operating
**  systems failed to turn up any that didn't handle it correctly (as
**  required by POSIX), while HP-UX 11.00 did use the broken return-zero
**  semantics of O_NDELAY (most other operating systems surveyed treated
**  O_NDELAY as synonymous with O_NONBLOCK).  Accordingly, we currently
**  unconditionally use O_NONBLOCK.  If this causes too many problems, an
**  autoconf test may be required.
*/

#ifdef O_NONBLOCK

int
nonblocking(int fd, bool flag)
{
    int mode;

    mode = fcntl(fd, F_GETFL, 0);
    if (mode < 0)
        return -1;
    mode = (flag ? (mode | O_NONBLOCK) : (mode & ~O_NONBLOCK));
    return fcntl(fd, F_SETFL, mode);
}

#else /* !O_NONBLOCK */

#include <sys/ioctl.h>
#if HAVE_SYS_FILIO_H
# include <sys/filio.h>
#endif

int
nonblocking(int fd, bool flag)
{
    int state;

    state = flag ? 1 : 0;
    return ioctl(fd, FIONBIO, &state);
}

#endif /* !O_NONBLOCK */
