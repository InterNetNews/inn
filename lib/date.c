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
#include <ctype.h>
#include <time.h>

#include "libinn.h"

/*
**  Time constants.
**
**  Do not translate these names.  RFC 822 by way of RFC 1036 requires that
**  weekday and month names *not* be translated.  This is why we use static
**  tables rather than strftime for building dates, to avoid locale
**  interference.
*/

static const char WEEKDAY[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

static const char MONTH[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
    "Nov", "Dec"
};

/* Number of days in a month. */
static const int MONTHDAYS[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* Non-numeric time zones.  Supporting these is required to support the
   obsolete date format of RFC 2822.  The military time zones are handled
   separately. */
static const struct {
    const char name[4];
    long offset;
} ZONE_OFFSET[] = {
    { "UT", 0 },                { "GMT", 0 },
    { "EDT", -4 * 60 * 60 },    { "EST", -5 * 60 * 60 },
    { "CDT", -5 * 60 * 60 },    { "CST", -6 * 60 * 60 },
    { "MDT", -6 * 60 * 60 },    { "MST", -7 * 60 * 60 },
    { "PDT", -7 * 60 * 60 },    { "PST", -8 * 60 * 60 },
};


/*
**  Time parsing macros.
*/

/* Whether a given year is a leap year. */
#define ISLEAP(year) \
    (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0))


/*
**  RFC 2822 date parsing rules.
*/

/* The data structure to store a rule.  The interpretation of the other fields
   is based on the value of type.  For NUMBER, read between min and max
   characters and convert to a number.  For LOOKUP, look for max characters
   and find that string in the provided table (with size elements).  For
   DELIM, just make sure that we see the character stored in delimiter. */
struct rule {
    enum {
        TYPE_NUMBER,
        TYPE_LOOKUP,
        TYPE_DELIM
    } type;
    char delimiter;
    const char (*table)[4];
    size_t size;
    int min;
    int max;
};


/*
**  Given a time as a time_t, return the offset in seconds of the local time
**  zone from UTC at that time (adding the offset to UTC time yields local
**  time).  If the second argument is true, the time represents the current
**  time and in that circumstance we can assume that timezone/altzone are
**  correct.  (We can't for arbitrary times in the past.)
*/
static long
local_tz_offset(time_t date, bool current UNUSED)
{
    struct tm *tm;
#if !HAVE_STRUCT_TM_TM_GMTOFF
    struct tm local, gmt;
    long offset;
#endif

    tm = localtime(&date);

#if !HAVE_STRUCT_TM_TM_GMTOFF && HAVE_DECL_ALTZONE
    if (current)
        return (tm->tm_isdst > 0) ? -altzone : -timezone;
#endif

#if HAVE_STRUCT_TM_TM_GMTOFF
    return tm->tm_gmtoff;
#else
    /* We don't have any easy returnable value, so we call both localtime
       and gmtime and calculate the difference.  Assume that local time is
       never more than 24 hours away from UTC and ignore seconds. */
    local = *tm;
    tm = gmtime(&date);
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
**  the length of the buffer, write the contents of a valid RFC 2822 / RFC
**  1036 Date header into the buffer (provided it's long enough).  Returns
**  true on success, false if the buffer is too long.  Use snprintf rather
**  than strftime to be absolutely certain that locales don't result in the
**  wrong output.  If the time is -1, obtain and use the current time.
*/
bool
makedate(time_t date, bool local, char *buff, size_t buflen)
{
    time_t realdate;
    struct tm *tmp_tm;
    struct tm tm;
    long tz_offset;
    int tz_hour_offset, tz_min_offset, tz_sign;
    size_t date_length;
    const char *tz_name;

    /* Make sure the buffer is large enough.  A complete RFC 2822 date with
       spaces wherever FWS is required and the optional weekday takes:

                    1         2         3
           1234567890123456789012345678901
           Sat, 31 Aug 2002 23:45:18 +0000

       31 characters, plus another character for the trailing nul.  The buffer
       will need to have at least another six characters of space to get the
       optional trailing time zone comment. */
    if (buflen < 32)
        return false;

    /* Get the current time if the provided time is -1. */
    realdate = (date == (time_t) -1) ? time(NULL) : date;

    /* RFC 2822 says the timezone offset is given as [+-]HHMM, so we have to
       separate the offset into a sign, hours, and minutes.  Dividing the
       offset by 36 looks like it works, but will fail for any offset that
       isn't an even number of hours, and there are half-hour timezones. */
    if (local) {
        tmp_tm = localtime(&realdate);
        tm = *tmp_tm;
        tz_offset = local_tz_offset(realdate, date == (time_t) -1);
        tz_sign = (tz_offset < 0) ? -1 : 1;
        tz_offset *= tz_sign;
        tz_hour_offset = tz_offset / 3600;
        tz_min_offset = (tz_offset % 3600) / 60;
    } else {
        tmp_tm = gmtime(&realdate);
        tm = *tmp_tm;
        tz_sign = 1;
        tz_hour_offset = 0;
        tz_min_offset = 0;
    }

    /* tz_min_offset cannot be larger than 60 (by basic mathematics).  If
       through some insane circumtances, tz_hour_offset would be larger,
       reject the time as invalid rather than generate an invalid date. */
    if (tz_hour_offset > 24)
        return false;

    /* Generate the actual date string, sans the trailing time zone comment
       but with the day of the week and the seconds (both of which are
       optional in the standard). */
    snprintf(buff, buflen, "%3.3s, %d %3.3s %d %02d:%02d:%02d %c%02d%02d",
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
#if HAVE_STRUCT_TM_TM_ZONE
        tz_name = tm.tm_zone;
#elif HAVE_TZNAME
        tz_name = tzname[(tm.tm_isdst > 0) ? 1 : 0];
#else
        tz_name = NULL;
#endif
    }
    if (tz_name != NULL && date_length + 4 + strlen(tz_name) <= buflen) {
        snprintf(buff + date_length, buflen - date_length, " (%s)", tz_name);
    }
    return true;
}


/*
**  Given a struct tm representing a calendar time in UTC, convert it to
**  seconds since epoch.  Returns (time_t) -1 if the time is not
**  convertable.  Note that this function does not canonicalize the provided
**  struct tm, nor does it allow out of range values or years before 1970.
*/
static time_t
mktime_utc(const struct tm *tm)
{
    time_t result = 0;
    int i;

    /* We do allow some ill-formed dates, but we don't do anything special
       with them and our callers really shouldn't pass them to us.  Do
       explicitly disallow the ones that would cause invalid array accesses
       or other algorithm problems. */
    if (tm->tm_mon < 0 || tm->tm_mon > 11 || tm->tm_year < 70)
        return (time_t) -1;

    /* Convert to a time_t. */
    for (i = 1970; i < tm->tm_year + 1900; i++)
        result += 365 + ISLEAP(i);
    for (i = 0; i < tm->tm_mon; i++)
        result += MONTHDAYS[i];
    if (tm->tm_mon > 1 && ISLEAP(tm->tm_year + 1900))
        result++;
    result = 24 * (result + tm->tm_mday - 1) + tm->tm_hour;
    result = 60 * result + tm->tm_min;
    result = 60 * result + tm->tm_sec;
    return result;
}


/*
**  Check the ranges of values in a struct tm to make sure that the date was
**  well-formed.  Assumes that the year has already been correctly set to
**  something (but may be before 1970).
*/
static bool
valid_tm(const struct tm *tm)
{
    if (tm->tm_sec > 60 || tm->tm_min > 59 || tm->tm_hour > 23)
        return false;
    if (tm->tm_mday < 1 || tm->tm_mon < 0 || tm->tm_mon > 11)
        return false;

    /* Make sure that the day isn't past the end of the month, allowing for
       leap years. */
    if (tm->tm_mday > MONTHDAYS[tm->tm_mon]
        && (tm->tm_mon != 1 || tm->tm_mday > 29
            || !ISLEAP(tm->tm_year + 1900)))
        return false;

    /* We can't handle years before 1970. */
    if (tm->tm_year < 70)
        return false;

    return true;
}


/*
**  Parse a date in the format used in NNTP commands such as NEWGROUPS and
**  NEWNEWS.  The first argument is a string of the form YYYYMMDD and the
**  second a string of the form HHMMSS.  The third argument is a boolean
**  flag saying whether the date is specified in local time; if false, the
**  date is assumed to be in UTC.  Returns the time_t corresponding to the
**  given date and time or (time_t) -1 in the event of an error.
*/
time_t
parsedate_nntp(const char *date, const char *hour, bool local)
{
    const char *p;
    size_t datelen;
    time_t now, result;
    struct tm tm;
    struct tm *current;
    int century;

    /* Accept YYMMDD and YYYYMMDD.  The first is what RFC 977 requires.  The
       second is what the revision of RFC 977 will require. */
    datelen = strlen(date);
    if ((datelen != 6 && datelen != 8) || strlen(hour) != 6)
        return (time_t) -1;
    for (p = date; *p; p++)
        if (!CTYPE(isdigit, *p))
            return (time_t) -1;
    for (p = hour; *p; p++)
        if (!CTYPE(isdigit, *p))
            return (time_t) -1;

    /* Parse the date into a struct tm, skipping over the century part of
       the year, if any.  We'll deal with it in a moment. */
    tm.tm_isdst = -1;
    p = date + datelen - 6;
    tm.tm_year = (p[0] - '0') * 10 + p[1] - '0';
    tm.tm_mon  = (p[2] - '0') * 10 + p[3] - '0' - 1;
    tm.tm_mday = (p[4] - '0') * 10 + p[5] - '0';
    p = hour;
    tm.tm_hour = (p[0] - '0') * 10 + p[1] - '0';
    tm.tm_min  = (p[2] - '0') * 10 + p[3] - '0';
    tm.tm_sec  = (p[4] - '0') * 10 + p[5] - '0';

    /* Four-digit years are the easy case.

       For two-digit years, RFC 977 says "The closest century is assumed as
       part of the year (i.e., 86 specifies 1986, 30 specifies 2030, 99 is
       1999, 00 is 2000)."  draft-ietf-nntpext-base-10.txt simplifies this
       considerably and is what we implement:

         If the first two digits of the year are not specified, the year is
         to be taken from the current century if YY is smaller than or equal
         to the current year, otherwise the year is from the previous
         century.

       This implementation assumes "current year" means the last two digits
       of the current year.  Note that this algorithm interacts poorly with
       clients with a slightly fast clock around the turn of a century, as
       it may send 00 for the year when the year on the server is still xx99
       and have it taken to be 99 years in the past.  But 2000 has come and
       gone, and by 2100 news clients *really* should have started using UTC
       for everything like the new draft recommends. */
    if (datelen == 8) {
        tm.tm_year += (date[0] - '0') * 1000 + (date[1] - '0') * 100;
        tm.tm_year -= 1900;
    } else {
        now = time(NULL);
        current = local ? localtime(&now) : gmtime(&now);
        century = current->tm_year / 100;
        if (tm.tm_year > current->tm_year % 100)
            century--;
        tm.tm_year += century * 100;
    }

    /* Ensure that all of the date components are within valid ranges. */
    if (!valid_tm(&tm))
        return (time_t) -1;

    /* tm contains the broken-down date; convert it to a time_t.  mktime
       assumes the supplied struct tm is in the local time zone; if given a
       time in UTC, use our own routine instead. */
    result = local ? mktime(&tm) : mktime_utc(&tm);
    return result;
}


/*
**  Skip any amount of CFWS (comments and folding whitespace), the RFC 2822
**  grammar term for whitespace, CRLF pairs, and possibly nested comments that
**  may contain escaped parens.  We also allow simple newlines since we don't
**  always deal with wire-format messages.  Note that we do not attempt to
**  ensure that CRLF or a newline is followed by whitespace.  Returns the new
**  position of the pointer.
*/
static const char *
skip_cfws(const char *p)
{
    int nesting = 0;

    for (; *p != '\0'; p++) {
        switch (*p) {
        case ' ':
        case '\t':
        case '\n':
            break;
        case '\r':
            if (p[1] != '\n' && nesting == 0)
                return p;
            break;
        case '(':
            nesting++;
            break;
        case ')':
            if (nesting == 0)
                return p;
            nesting--;
            break;
        case '\\':
            if (nesting == 0 || p[1] == '\0')
                return p;
            p++;
            break;
        default:
            if (nesting == 0)
                return p;
            break;
        }
    }
    return p;
}


/*
**  Parse a single number.  Takes the parsing rule that we're applying and
**  returns a pointer to the new position of the parse stream.  If there
**  aren't enough digits, return NULL.
*/
static const char *
parse_number(const char *p, const struct rule *rule, int *value)
{
    int count;

    *value = 0;
    for (count = 0; *p != '\0' && count < rule->max; p++, count++) {
        if (*p < '0' || *p > '9')
            break;
        *value = *value * 10 + (*p - '0');
    }
    if (count < rule->min || count > rule->max)
        return NULL;
    return p;
}


/*
**  Parse a single string value that has to be done via table lookup.  Takes
**  the parsing rule that we're applying.  Puts the index number of the string
**  if found into the value pointerand returns the new position of the string,
**  or NULL if the string could not be found in the table.
*/
static const char *
parse_lookup(const char *p, const struct rule *rule, int *value)
{
    size_t i;

    for (i = 0; i < rule->size; i++)
        if (strncasecmp(rule->table[i], p, rule->max) == 0) {
            p += rule->max;
            *value = i;
            return p;
        }
    return NULL;
}


/*
**  Apply a set of date parsing rules to a string.  Returns the new position
**  in the parse string if this succeeds and NULL if it fails.  As part of the
**  parse, stores values into the value pointer in the array of rules that was
**  passed in.  Takes an array of rules and a count of rules in that array.
*/
static const char *
parse_by_rule(const char *p, const struct rule rules[], size_t count,
              int *values)
{
    size_t i;
    const struct rule *rule;

    for (i = 0; i < count; i++) {
        rule = &rules[i];

        switch (rule->type) {
        case TYPE_DELIM:
            if (*p != rule->delimiter)
                return NULL;
            p++;
            break;
        case TYPE_LOOKUP:
            p = parse_lookup(p, rule, &values[i]);
            if (p == NULL)
                return NULL;
            break;
        case TYPE_NUMBER:
            p = parse_number(p, rule, &values[i]);
            if (p == NULL)
                return NULL;
            break;
        }

        p = skip_cfws(p);
    }
    return p;
}


/*
**  Parse a legacy time zone.  This uses the parsing rules in RFC 2822,
**  including assigning an offset of 0 to all single-character military time
**  zones due to their ambiguity in practice.  Returns the new position in the
**  parse stream or NULL if we failed to parse the zone.
*/
static const char *
parse_legacy_timezone(const char *p, long *offset)
{
    const char *end;
    size_t max, i;

    for (end = p; *end != '\0' && !CTYPE(isspace, *end); end++)
        ;
    if (end == p)
        return NULL;
    max = end - p;
    for (i = 0; i < ARRAY_SIZE(ZONE_OFFSET); i++)
        if (strncasecmp(ZONE_OFFSET[i].name, p, max) == 0) {
            p += strlen(ZONE_OFFSET[i].name);
            *offset = ZONE_OFFSET[i].offset;
            return p;
        }
    if (max == 1 && CTYPE(isalpha, *p) && *p != 'J' && *p != 'j') {
        *offset = 0;
        return p + 1;
    }
    return NULL;
}


/*
**  Parse an RFC 2822 date, accepting the normal and obsolete syntax.  Takes a
**  pointer to the beginning of the date and the length.  Returns the
**  translated time in seconds since epoch, or (time_t) -1 on error.
*/
time_t
parsedate_rfc2822(const char *date)
{
    const char *p;
    int zone_sign;
    long zone_offset;
    struct tm tm;
    int values[8];
    time_t result;

    /* The basic rules.  Note that we don't bother to check whether the day of
       the week is accurate or not. */
    static const struct rule base_rule[] = {
        { TYPE_LOOKUP, 0,   WEEKDAY, 7,  3, 3 },
        { TYPE_DELIM,  ',', NULL,    0,  1, 1 },
        { TYPE_NUMBER, 0,   NULL,    0,  1, 2 },
        { TYPE_LOOKUP, 0,   MONTH,   12, 3, 3 },
        { TYPE_NUMBER, 0,   NULL,    0,  2, 4 },
        { TYPE_NUMBER, 0,   NULL,    0,  2, 2 },
        { TYPE_DELIM,  ':', NULL,    0,  1, 1 },
        { TYPE_NUMBER, 0,   NULL,    0,  2, 2 }
    };

    /* Optional seconds at the end of the time. */
    static const struct rule seconds_rule[] = {
        { TYPE_DELIM,  ':', NULL,    0,  1, 1 },
        { TYPE_NUMBER, 0,   NULL,    0,  2, 2 }
    };

    /* Numeric time zone. */
    static const struct rule zone_rule[] = {
        { TYPE_NUMBER, 0,   NULL,    0,  4, 4 }
    };

    /* Start with a clean slate. */
    memset(&tm, 0, sizeof(struct tm));
    memset(values, 0, sizeof(values));

    /* Parse the base part of the date.  The initial day of the week is
       optional. */
    p = skip_cfws(date);
    if (CTYPE(isalpha, *p))
        p = parse_by_rule(p, base_rule, ARRAY_SIZE(base_rule), values);
    else
        p = parse_by_rule(p, base_rule + 2, ARRAY_SIZE(base_rule) - 2,
                          values + 2);
    if (p == NULL)
        return (time_t) -1;

    /* Stash the results into a struct tm.  Values are associated with the
       rule number of the same index. */
    tm.tm_mday = values[2];
    tm.tm_mon = values[3];
    tm.tm_year = values[4];
    tm.tm_hour = values[5];
    tm.tm_min = values[7];

    /* Parse seconds if they're present. */
    if (*p == ':') {
        p = parse_by_rule(p, seconds_rule, ARRAY_SIZE(seconds_rule), values);
        if (p == NULL)
            return (time_t) -1;
        tm.tm_sec = values[1];
    }

    /* Time zone.  Unfortunately this is weird enough that we can't use nice
       parsing rules for it. */
    if (*p == '-' || *p == '+') {
        zone_sign = (*p == '+') ? 1 : -1;
        p = parse_by_rule(p + 1, zone_rule, ARRAY_SIZE(zone_rule), values);
        if (p == NULL)
            return (time_t) -1;
        zone_offset = ((values[0] / 100) * 60 + values[0] % 100) * 60;
        zone_offset *= zone_sign;
    } else {
        p = parse_legacy_timezone(p, &zone_offset);
        if (p == NULL)
            return (time_t) -1;
    }

    /* Fix up the year, using the RFC 2822 rules.  Remember that tm_year
       stores the year - 1900. */
    if (tm.tm_year < 50)
        tm.tm_year += 100;
    else if (tm.tm_year >= 1000)
        tm.tm_year -= 1900;

    /* Done parsing.  Make sure there's nothing left but CFWS and range-check
       our results and then convert the struct tm to seconds since epoch and
       then apply the time zone offset. */
    p = skip_cfws(p);
    if (*p != '\0')
        return (time_t) -1;
    if (!valid_tm(&tm))
        return (time_t) -1;
    result = mktime_utc(&tm);
    return (result == (time_t) -1) ? result : result - zone_offset;
}
