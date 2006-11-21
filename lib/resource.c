/*  $Id$
**
*/

#include "config.h"
#include "clibrary.h"
#include "inn/libinn.h"

#ifdef HAVE_GETRUSAGE

#include <sys/time.h>
#include <sys/resource.h>

#define TIMEVALasDOUBLE(t)	\
    ((double)(t).tv_sec + ((double)(t).tv_usec) / 1000000.0)

int getrusage(int who, struct rusage *rusage);

int GetResourceUsage(double *usertime, double *systime)
{
    struct rusage	R;

    if (getrusage(RUSAGE_SELF, &R) < 0)
	return -1;
    *usertime = TIMEVALasDOUBLE(R.ru_utime);
    *systime = TIMEVALasDOUBLE(R.ru_stime);
    return 0;
}

#else /* HAVE_GETRUSAGE */

#include <sys/param.h>
#include <sys/times.h>

#if	!defined(HZ)
#define HZ	60
#endif	/* !defined(HZ) */

#define CPUTIMEasDOUBLE(t1, t2)		((double)(t1 + t2) / (double)HZ)

int GetResourceUsage(double *usertime, double *systime)
{
    struct tms	T;

    if (times(&T) == -1)
	return -1;
    *usertime = CPUTIMEasDOUBLE(T.tms_utime, T.tms_cutime);
    *systime = CPUTIMEasDOUBLE(T.tms_stime, T.tms_cstime);
    return 0;
}

#endif /* !HAVE_GETRUSAGE */
