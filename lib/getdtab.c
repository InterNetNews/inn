/*  $Revision$
**
**  Determine the current limit on open file descriptors.
**
**  Portably determine the limit on open file descriptors, preferring
**  sysconf(), then getrlimit(), then getdtablesize(), then ulimit(),
**  then <sys/param.h>, and falling back on the guaranteed minimum of 20.
*/

#include "config.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifdef HAVE_SYSCONF
# define FDCOUNT_SYSCONF
#else
# ifdef HAVE_RLIMIT
#  include <sys/time.h>
#  include <sys/resource.h>
#  ifdef RLIMIT_NOFILE
#   define FDCOUNT_RLIMIT
#  endif
# endif
#endif

#if !defined(FDCOUNT_SYSCONF) && !defined(FDCOUNT_RLIMIT)
# ifdef HAVE_GETDTABLESIZE
#  define FDCOUNT_GETDTAB
# else
#  ifdef HAVE_ULIMIT
#   define FDCOUNT_ULIMIT
#  else
#   include <sys/param.h>
#   define FDCOUNT_CONSTANT
#  endif
# endif
#endif


#ifdef FDCOUNT_SYSCONF
int
getfdcount(void)
{
    return sysconf(_SC_OPEN_MAX);
}
#endif /* FDCOUNT_SYSCONF */


#ifdef FDCOUNT_RLIMIT
int
getfdcount(void)
{
    struct rlimit rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0)
        return -1;
    return rl.rlim_cur;
}
#endif /* FDCOUNT_RLIMIT */


#ifdef FDCOUNT_GETDTAB
int
getfdcount(void)
{
    return getdtablesize();
}
#endif /* FDCOUNT_GETDTAB */


#ifdef FDCOUNT_ULIMIT
int
getfdcount(void)
{
# ifdef UL_GDESLIM
    return ulimit(UL_GDESLIM, 0);
# else
    return ulimit(4, 0);
# endif
}
#endif /* FDCOUNT_ULIMIT */


#ifdef FDCOUNT_CONSTANT
int getfdcount(void)
{
# ifdef NOFILE
    return NOFILE;
# else
    return 20;
# endif
}
#endif /* FDCOUNT_CONSTANT */
