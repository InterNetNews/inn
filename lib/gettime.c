/*  $Revision$
**
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include "libinn.h"


int GetTimeInfo(TIMEINFO *Now)
{
    static time_t	NextHour;
    static long		LastTzone;
    struct tm		*tm;
    int			secondsUntilNextHour;
#if	defined(HAVE_GETTIMEOFDAY)
    struct timeval	tv;
#endif	/* defined(HAVE_GETTIMEOFDAY) */
#if	!defined(HAVE_TM_GMTOFF)
    struct tm		local;
    struct tm		gmt;
#endif	/* !defined(HAVE_TM_GMTOFF) */

    /* Get the basic time. */
#if	defined(HAVE_GETTIMEOFDAY)
    if (gettimeofday(&tv, (struct timezone *)NULL) == -1)
	return -1;
    Now->time = tv.tv_sec;
    Now->usec = tv.tv_usec;
#else
    /* Can't check for -1 since that might be a time, I guess. */
    (void)time(&Now->time);
    Now->usec = 0;
#endif	/* defined(HAVE_GETTIMEOFDAY) */

    /* Now get the timezone if the last time < HH:00:00 <= now for some HH.  */
    if (NextHour <= Now->time) {
	if ((tm = localtime(&Now->time)) == NULL)
	    return -1;
	secondsUntilNextHour = 60 * (60 - tm->tm_min) - tm->tm_sec;
#if	!defined(HAVE_TM_GMTOFF)
	/* To get the timezone, compare localtime with GMT. */
	local = *tm;
	if ((tm = gmtime(&Now->time)) == NULL)
	    return -1;
	gmt = *tm;

	/* Assume we are never more than 24 hours away. */
	LastTzone = gmt.tm_yday - local.tm_yday;
	if (LastTzone > 1)
	    LastTzone = -24;
	else if (LastTzone < -1)
	    LastTzone = 24;
	else
	    LastTzone *= 24;

	/* Scale in the hours and minutes; ignore seconds. */
	LastTzone += gmt.tm_hour - local.tm_hour;
	LastTzone *= 60;
	LastTzone += gmt.tm_min - local.tm_min;
#else
	LastTzone =  (0 - tm->tm_gmtoff) / 60;
#endif	/* defined(HAVE_TM_GMTOFF) */
	NextHour = Now->time + secondsUntilNextHour;
    }
    Now->tzone = LastTzone;
    return 0;
}
