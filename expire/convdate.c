/*  $Id$
**
**  Convert date strings and numbers to numbers and strings.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <time.h>

#include "libinn.h"

static const char usage[] = "\
Usage: convdate -n date [date ...]\n\
       convdate [-dl] -c time [time ...]\n\
       convdate [-dl] [-s] date [date ...]\n\
\n\
convdate -n converts a date (in any format parseable by parsedate) to the\n\
number of seconds since epoch.  convdate -s does the same, but converts\n\
to a date string.  convdate -c converts seconds since epoch to a date\n\
string.  The default date string is the output of ctime (normally the\n\
same format as returned by the date command).  If -d is given, the output\n\
is formatted as a valid Usenet article Date header.  If -l is given with\n\
-d, format the time in local time rather than UTC.  If no options are\n\
given, the -s behavior is the default.";


/*
**  Return true if the given string is entirely digits.
*/
static BOOL
isdigits(const char *p)
{
    for (; *p; p++)
        if (!CTYPE(isdigit, (unsigned char) *p))
            return FALSE;
    return TRUE;
}


/*
**  Print date corresponding to the provided time_t.  By default, the output
**  of ctime is printed, but if the second argument is true, makedate is
**  used instead.  If the third argument is true, format in local time;
**  otherwise, use UTC (only meaningful if the second argument is true).
**  Returns success.
*/
static BOOL
print_date(time_t date, BOOL format, BOOL local)
{
    char date_buffer[128];
    char *result;

    if (format) {
        if (!makedate(date, local, date_buffer, 128)) {
            warn("can't format %ld", (long) date);
            return FALSE;
        } else {
            printf("%s\n", date_buffer);
        }
    } else {
        result = ctime(&date);
        if (result == NULL) {
            warn("can't format %ld", (long) date);
            return FALSE;
        } else {
            printf("%s", result);
        }
    }
}


int
main(int argc, char *argv[])
{
    int         option, status;
    int         mode = 0;
    BOOL        date_format = FALSE;
    BOOL        date_local = FALSE;
    char *      date;
    time_t      seconds;

    error_program_name = "convdate";

    /* Parse options. */
    while ((option = getopt(argc, argv, "cdhlns")) != EOF) {
        switch (option) {
        case 'h':
            printf("%s\n", usage);
            break;
        case 'd':
            date_format = TRUE;
            break;
        case 'l':
            date_local = TRUE;
            break;
        case 'c':
        case 'n':
        case 's':
            if (mode != 0) die("only one of -c, -n, or -s is allowed");
            mode = option;
            break;
        default:
            die("%s", usage);
            break;
        }
    }
    if (mode == 0) mode = 's';
    argc -= optind;
    argv += optind;
    if (argc == 0) die("no date to convert given");

    /* Perform the desired action for each provided argument. */
    for (date = *argv, status = 0; date != NULL; date = *++argv) {
        switch (mode) {
        default:
        case 's':
            seconds = parsedate(date, NULL);
            if (seconds == -1) {
                warn("can't convert \"%s\"", date);
                status++;
            } else {
                if (!print_date(seconds, date_format, date_local))
                    status++;
            }
            break;
        case 'n':
            seconds = parsedate(date, NULL);
            if (seconds == -1) {
                warn("can't convert \"%s\"", date);
                status++;
            } else {
                printf("%ld\n", seconds);
            }
            break;
        case 'c':
            if (!isdigits(date)) {
                warn("\"%s\" doesn't look like a number", date);
                status++;
            } else {
                seconds = (time_t) atol(date);
                if (!print_date(seconds, date_format, date_local))
                    status++;
            }
            break;
        }
    }
    exit(status);
}
