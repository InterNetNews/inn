/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <sys/param.h>


#if	defined(FDCOUNT_GETDTAB)
int getfdcount(void)
{
    static int		size;

    if (size <= 0) {
	if ((size = getdtablesize()) < 0)
	    return -1;
    }
    return size;
}
#endif	/* defined(FDCOUNT_GETDTAB) */


#if	defined(FDCOUNT_GETRLIMIT)
#include <sys/time.h>
#include <sys/resource.h>

int getfdcount(void)
{
    static int		size;
    struct rlimit	rl;

#if	defined(HAVE_RLIMIT)
    if (size <= 0) {
	if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
	    return -1;
	size = rl.rlim_cur;
    }
#endif	/* defined(HAVE_RLIMIT) */
    return size;
}
#endif	/* defined(FDCOUNT_GETRLIMIT) */


#if	defined(FDCOUNT_SYSCONF)
#include <unistd.h>
#include <limits.h>

int getfdcount(void)
{
    static int		size;

    if (size <= 0) {
	if ((size = sysconf(_SC_OPEN_MAX)) < 0)
	    return -1;
    }
    return size;
}
#endif	/* defined(FDCOUNT_SYSCONF) */


#if	defined(FDCOUNT_ULIMIT)
int getfdcount(void)
{
    static int		size;

    if (size <= 0) {
	if ((size = ulimit(4, 0L)) < 0)
	    return -1;
    }
    return size;
}
#endif	/* defined(FDCOUNT_ULIMIT) */


#if	defined(FDCOUNT_CONSTANT)
int getfdcount(void)
{
#if	defined(NOFILE)
    return NOFILE;
#else
    return 20;
#endif	/* defined(NOFILE) */
}
#endif	/* defined(FDCOUNT_CONSTANT) */
