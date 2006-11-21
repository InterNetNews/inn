/*  $Id$
**
**  Portably determine or set the limit on open file descriptors.
**
**  Pretty much all platforms these days have getrlimit and setrlimit, so
**  prefer those, but for determining the current limit preserve the old
**  portability (if we don't have getrlimit, try sysconf, then
**  getdtablesize, then ulimit, and then try to find a hard-coded constant
**  in <sys/param.h> and failing that fall back to the POSIX-guaranteed
**  minimum of 20.
**
**  For setting the limit, only setrlimit is supported; if it isn't
**  available, return -1 always.  We also refuse to set the limit to
**  something higher than select can handle, checking against FD_SETSIZE.
**
**  Note that on some versions of Linux (2.2.x reported), sysconf may return
**  the wrong value for the maximum file descriptors.  getrlimit is correct,
**  so always prefer it.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#if HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

/* FreeBSD 3.4 RELEASE needs <sys/time.h> before <sys/resource.h>. */
#if HAVE_GETRLIMIT || HAVE_SETRLIMIT
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/resource.h>
#endif

#include "inn/libinn.h"

#if HAVE_SETRLIMIT && defined(RLIMIT_NOFILE)

int
setfdlimit(unsigned int limit)
{
    struct rlimit rl;

#ifdef FD_SETSIZE
    if (limit > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }
#endif

    rl.rlim_cur = 0;
    rl.rlim_max = 0;

#if HAVE_GETRLIMIT
    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
        rl.rlim_cur = 0;
        rl.rlim_max = 0;
    }
#endif

    rl.rlim_cur = limit;
    if (limit > rl.rlim_max)
        rl.rlim_max = limit;
    return setrlimit(RLIMIT_NOFILE, &rl);
}

#else /* !(HAVE_SETRLIMIT && RLIMIT_NOFILE) */

int
setfdlimit(unsigned int limit UNUSED)
{
    /* Unimplemented system call is close enough. */
    errno = ENOSYS;
    return -1;
}

#endif /* !(HAVE_SETRLIMIT && RLIMIT_NOFILE) */

#if HAVE_GETRLIMIT && defined(RLIMIT_NOFILE)

int
getfdlimit(void)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        return -1;
    return rl.rlim_cur;
}

#elif HAVE_SYSCONF

int
getfdlimit(void)
{
    return sysconf(_SC_OPEN_MAX);
}

#elif HAVE_GETDTABLESIZE

int
getfdlimit(void)
{
    return getdtablesize();
}

#elif HAVE_ULIMIT

int
getfdlimit(void)
{
# ifdef UL_GDESLIM
    return ulimit(UL_GDESLIM, 0);
# else
    return ulimit(4, 0);
# endif
}

#else /* no function mechanism available */
# if HAVE_LIMITS_H
#  include <limits.h>
# endif
# include <sys/param.h>

int
getfdcount(void)
{
# ifdef NOFILE
    return NOFILE;
# else
    return 20;
# endif
}

#endif
