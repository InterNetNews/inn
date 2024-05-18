/*
**  Convert date strings and numbers to numbers and strings.
*/

#include "portable/system.h"

#include <ctype.h>
#include <time.h>

#include "inn/libinn.h"
#include "inn/messages.h"

static const char usage[] = "\
Usage: convdate -n [date ...]\n\
       convdate [-dl] -c [time ...]\n\
       convdate [-dl] [-s] [date ...]\n\
\n\
convdate -n converts a date in RFC 5322 format to the number of seconds\n\
since epoch.  convdate -s does the same, but converts to a date string.\n\
convdate -c converts seconds since epoch to a date string.  The default\n\
output is the output of ctime (normally the same format as returned by the\n\
date command).  If -d is given, the output is formatted as a valid Usenet\n\
article Date header field.  If -l is given with -d, format the time in local\n\
time rather than UTC.  If no options are given, the -s behavior is the\n\
default; if no dates are given, the current time is used.\n";

/* Whether to format the output as a Date header field. */
static bool date_format = false;

/* Whether to use local time instead of UTC. */
static bool date_local = false;


/*
**  Return true if the given string is entirely digits.
*/
static bool
isdigits(const char *p)
{
    for (; *p; p++)
        if (!isdigit((unsigned char) *p))
            return false;
    return true;
}


/*
**  Print date corresponding to the provided time_t.  By default, the output
**  of ctime is printed, but if date_format is true, makedate is used
**  instead.  If date_local is true, format in local time; otherwise, use
**  UTC.  Returns success.
*/
static bool
print_date(time_t date)
{
    char date_buffer[128];
    char *result;

    if (date_format) {
        if (!makedate(date, date_local, date_buffer, sizeof(date_buffer))) {
            warn("can't format %lu", (unsigned long) date);
            return false;
        } else {
            printf("%s\n", date_buffer);
        }
    } else {
        result = ctime(&date);
        if (result == NULL) {
            warn("can't format %lu", (unsigned long) date);
            return false;
        } else {
            printf("%s", result);
        }
    }
    return true;
}


/*
**  The core function.  Given a string representing a date (in some format
**  given by the mode) and a mode ('s', 'n', or 'c', corresponding to the
**  basic three options to the program), convert the date and print the
**  output.  date may be NULL, in which case the current date is used.
**  Returns true if conversion was successful, false otherwise.
*/
static bool
convdate(const char *date, char mode)
{
    time_t seconds;

    /* Convert the given date to seconds or obtain the current time. */
    if (date == NULL) {
        seconds = time(NULL);
    } else if (mode == 'c') {
        if (!isdigits(date)) {
            warn("\"%s\" doesn't look like a number", date);
            return false;
        } else {
            seconds = (time_t) atoll(date);
        }
    } else {
        seconds = parsedate_rfc5322_lax(date);
        if (seconds == (time_t) -1) {
            warn("can't convert \"%s\"", date);
            return false;
        }
    }

    /* Output the resulting date. */
    if (mode == 'n') {
        printf("%lu\n", (unsigned long) seconds);
        return true;
    } else {
        return print_date(seconds);
    }
}


int
main(int argc, char *argv[])
{
    int option, status;
    char *date;
    char mode = '\0';

    message_program_name = "convdate";

    /* Parse options. */
    while ((option = getopt(argc, argv, "cdhlns")) != EOF) {
        switch (option) {
        case 'h':
            printf("%s\n", usage);
            exit(0);
            /* NOTREACHED */
        case 'd':
            date_format = true;
            break;
        case 'l':
            date_local = true;
            break;
        case 'c':
        case 'n':
        case 's':
            if (mode != 0)
                die("only one of -c, -n, or -s is allowed");
            mode = option;
            break;
        default:
            fprintf(stderr, "%s", usage);
            exit(1);
            /* NOTREACHED */
        }
    }
    if (mode == '\0')
        mode = 's';
    argc -= optind;
    argv += optind;

    /* Perform the desired action for each provided argument. */
    if (argc == 0) {
        exit(convdate(NULL, mode) ? 0 : 1);
    } else {
        for (date = *argv, status = 0; date != NULL; date = *++argv)
            status += (convdate(date, mode) ? 0 : 1);
        exit(status);
    }
}
