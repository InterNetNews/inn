/*  $Id$
**
**  Find and return time information portably.
*/
#include "config.h"
#include "libinn.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif


int
GetTimeInfo(TIMEINFO *Now)
{
    static time_t       NextHour;
    static long         LastTzone;
    struct tm           *tm;
    int                 secondsUntilNextHour;

    struct timeval      tv;

#ifndef HAVE_STRUCT_TM_TM_GMTOFF
    struct tm           local;
    struct tm           gmt;
#endif

    /* Get the basic time. */
    if (gettimeofday(&tv, (struct timezone *) 0) == -1)
        return -1;
    Now->time = tv.tv_sec;
    Now->usec = tv.tv_usec;

    /* Now get the timezone if the last time < HH:00:00 <= now for some HH.  */
    if (NextHour <= Now->time) {
        tm = localtime(&Now->time);
        if (tm == NULL)
            return -1;
        secondsUntilNextHour = 60 * (60 - tm->tm_min) - tm->tm_sec;

#ifdef HAVE_STRUCT_TM_TM_GMTOFF
        LastTzone = (0 - tm->tm_gmtoff) / 60;
#else
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
#endif  /* defined(HAVE_STRUCT_TM_TM_GMTOFF) */

        NextHour = Now->time + secondsUntilNextHour;
    }
    Now->tzone = LastTzone;
    return 0;
}
