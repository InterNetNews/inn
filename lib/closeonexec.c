/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <errno.h>


#if	defined(CLX_IOCTL)
#include <sgtty.h>


/*
**  Mark a file close-on-exec so that it doesn't get shared with our
**  children.  Ignore any error codes.
*/
void
CloseOnExec(fd, flag)
    int		fd, flag;
{
    int		oerrno;

    oerrno = errno;
    (void)ioctl(fd, (unsigned long) (flag ? FIOCLEX : FIONCLEX), (char *)NULL);
    errno = oerrno;
}
#endif	/* defined(CLX_IOCTL) */



#if	defined(CLX_FCNTL)
#include <fcntl.h>


/*
**  Mark a file close-on-exec so that it doesn't get shared with our
**  children.  Ignore any error codes.
*/
void
CloseOnExec(fd, flag)
    int		fd;
    int		flag;
{
    int		oerrno;

    oerrno = errno;
    (void)fcntl(fd, F_SETFD, flag ? 1 : 0);
    errno = oerrno;
}
#endif	/* defined(CLX_FCNTL) */
