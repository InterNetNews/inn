/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"


#if	defined(NBIO_IOCTL)
#include <sys/ioctl.h>

/*
**  Enable or disable non-blocking I/O mode.
*/
int SetNonBlocking(int fd, BOOL flag)
{
    int		state;

    state = flag ? 1 : 0;
    return ioctl(fd, FIONBIO, (char *)&state);
}

#endif	/* defined(NBIO_IOCTL) */


#if	defined(NBIO_FCNTL)
#include <sys/file.h>
#include <fcntl.h>

#if	!defined(FNDELAY)
#define FNDELAY		O_NDELAY
#endif	/* !defined(FNDELAY) */


/*
**  Enable or disable non-blocking I/O mode.
*/
int SetNonBlocking(int fd, BOOL flag)
{
    int		mode;

    if ((mode = fcntl(fd, F_GETFL, 0)) < 0)
	return -1;
    if (flag)
	mode |= FNDELAY;
    else
	mode &= ~FNDELAY;
    return fcntl(fd, F_SETFL, mode);
}
#endif	/* defined(NBIO_FCNTL) */
