/*  $Id$
**
**  Date parsing and conversion routines.
**
**  Provides various date parsing and conversion routines, including
**  generating Date headers for posted articles.  Note that the parsedate
**  parser is separate from this file.
*/

#include "config.h"
#include "clibrary.h"
#include "libinn.h"

#include <time.h>

/* Do not translate these names.  RFC 822 by way of RFC 1036 requires that
   weekday and month names *not* be translated.  This is why we use static
   tables instead of strftime, to avoid locale interference.  */
static const char WEEKDAY[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char MONTH[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
    "Nov", "Dec"
};

/* The maximum length of the date specification, not including the time zone
   comment.  Used to make sure the buffer is large enough. */
#define DATE_LENGTH     32


/*
**  Given a time as a time_t, return the offset in seconds of the local time
**  zone from UTC at that time.  If the second argument is true, the time
**  represents the current time and in that circumstance we can assume that
**  timezone/altzone are correct.  (We can't for arbitrary times in the
**  past.)
*/
static long
local_tz_offset(time_t time, BOOL current)
{
    struct tm *tm;
#if !HAVE_TM_GMTOFF
    struct tm local, gmt;
    long offset;
#endif

    tm = localtime(&time);

#if !HAVE_TM_GMTOFF && HAVE_VAR_TIMEZONE
    if (current)
        return (tm->tm_isdst > 0) ? -altzone : -timezone;
#endif

#if HAVE_TM_GMTOFF
    return tm->tm_gmtoff;
#else
    /* We don't have any easy returnable value, so we call both localtime
       and gmtime and calculate the difference.  Assume that local time is
       never more than 24 hours away from UTC and ignore seconds. */
    local = *tm;
    tm = gmtime(&time);
    gmt = *tm;
    offset = local.tm_yday - gmt.tm_yday;
    if (offset < -1) {
        /* Local time is in the next year. */
        offset = 24;
    } else if (offset > 1) {
        /* Local time is in the previous year. */
        offset = -24;
    } else {
        offset *= 24;
    }
    offset += local.tm_hour - gmt.tm_hour;
    offset *= 60;
    offset += local.tm_min - gmt.tm_min;
    return offset * 60;
#endif /* !HAVE_TM_GMTOFF */
}


/*
**  Given a time_t, a flag saying whether to use local time, a buffer, and
**  the length of the buffer, write the contents of a valid RFC 822 / RFC
**  1036 Date header into the buffer (provided it's long enough).  Returns
**  true on success, false if the buffer is too long.  Use sprintf rather
**  than strftime to be absolutely certain that locales don't result in the
**  wrong output.  If the time is zero, obtain and use the current time.
*/
BOOL
makedate(time_t clock, BOOL local, char *buff, size_t buflen)
{
    time_t realclock;
    struct tm *tmp_tm;
    struct tm tm;
    long tz_offset;
    int tz_hour_offset, tz_min_offset, tz_sign;
    size_t date_length;
    const char *tz_name;

    /* Make sure the buffer is large enough. */
    if (buflen < DATE_LENGTH + 1) return FALSE;

    /* Get the current time if the provided time is 0. */
    realclock = (clock == 0) ? time(NULL) : clock;

    /* RFC 822 says the timezone offset is given as [+-]HHMM, so we have to
       separate the offset into a sign, hours, and minutes.  Dividing the
       offset by 36 looks like it works, but will fail for any offset that
       isn't an even number of hours, and there are half-hour timezones. */
    if (local) {
        tmp_tm = localtime(&realclock);
        tm = *tmp_tm;
        tz_offset = local_tz_offset(realclock, clock == 0);
        tz_sign = (tz_offset < 0) ? -1 : 1;
        tz_offset *= tz_sign;
        tz_hour_offset = tz_offset / 3600;
        tz_min_offset = (tz_offset % 3600) / 60;
    } else {
        tmp_tm = gmtime(&realclock);
        tm = *tmp_tm;
        tz_sign = 1;
        tz_hour_offset = 0;
        tz_min_offset = 0;
    }

    /* tz_min_offset cannot be larger than 60 (by basic mathematics).  In
       some insane circumstances, tz_hour_offset could be larger; if it is,
       fail.  Otherwise, we could overflow our buffer. */
    if (tz_hour_offset > 24) return FALSE;

    /* Generate the actual date string, sans the trailing time zone comment
       but with the day of the week and the seconds (both of which are
       optional in the standard).  Assume the struct tm values are sane and
       won't overflow the buffer (they would have to be in violation of
       ISO/ANSI C to do so). */
    sprintf(buff, "%3.3s, %d %3.3s %d %02d:%02d:%02d %c%02d%02d",
            &WEEKDAY[tm.tm_wday][0], tm.tm_mday, &MONTH[tm.tm_mon][0],
            1900 + tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec,
            (tz_sign > 0) ? '+' : '-', tz_hour_offset, tz_min_offset);
    date_length = strlen(buff);

    /* Now, get a pointer to the time zone abbreviation, and if there is
       enough room in the buffer, add it to the end of the date string as a
       comment. */
    if (!local) {
        tz_name = "UTC";
    } else {
#if HAVE_TM_ZONE
        tz_name = tm.tm_zone;
#elif HAVE_VAR_TZNAME
        tz_name = tzname[(tm.tm_isdst > 0) ? 1 : 0];
#else
        tz_name = NULL;
#endif
    }
    if (tz_name != NULL && date_length + 4 + strlen(tz_name) <= buflen) {
        sprintf(buff + date_length, " (%s)", tz_name);
    }
    return TRUE;
}
