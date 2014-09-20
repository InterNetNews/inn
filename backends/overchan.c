/*  $Id$
**
**  Add entries to the overview database.
**
**  overchan is intended to run as a channel feed to store data into the
**  overview database if INN doesn't write that data out directly.  Sometimes
**  it can be useful to offload the work of writing overview to a separate
**  program that's not part of INN's main processing loop.  overchan can also
**  be used to add batches of data to overview when needed, such as when
**  running makehistory -O.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <syslog.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/overview.h"
#include "inn/qio.h"
#include "inn/libinn.h"
#include "inn/paths.h"

/* Statistics kept while overchan is running. */
struct statistics {
    unsigned long articles;
    unsigned long busy;
};


/*
**  Take a line of data and parse it into the provided overview_data struct.
**  Returns true if the data is well-formed, false otherwise.  Malformed data
**  is also reported via warn.
**
**  Each line must be of the form <token> <arrived> <expires> <data>, with
**  the fields separated by a single space.
*/
static bool
parse_line(char *line, struct overview_data *data)
{
    char *start, *end;
    const char *fields[3];
    int i;

    /* We don't want to split the whole line on spaces, since that will mangle
       the overview data, so we can't just use a vector.  Instead, find the
       first three spaces and insert terminators there, treating the rest of
       the line as the data. */
    for (start = line, i = 0; i < 3; i++) {
        end = strchr(start, ' ');
        if (end == NULL) {
            warn("truncated input line after %s", start);
            return false;
        }
        *end = '\0';
        fields[i] = start;
        start = end + 1;
    }
    data->overview = start;
    data->overlen = strlen(start);
    if (!IsToken(fields[0])) {
        warn("malformed token %s", fields[0]);
        return false;
    }
    data->token = TextToToken(fields[0]);
    errno = 0;
    data->arrived = strtoul(fields[1], &end, 10);
    if (errno != 0 || end != fields[2] - 1) {
        warn("malformed arrival time %s", fields[1]);
        return false;
    }
    data->expires = strtoul(fields[2], &end, 10);
    if (errno != 0 || end != data->overview - 1) {
        warn("malformed expires time %s", fields[2]);
        return false;
    }
    return true;
}


/*
**  Write the supplied data to the overview database and update the
**  statistics.  The ugly code is to locate the Xref data to tell the overview
**  API what groups and article numbers to use.
*/
static void
write_overview(struct overview *overview, struct overview_data *data,
               struct statistics *statistics)
{
    struct timeval start, end;
    const char *p;
    const char *xref = NULL;

    statistics->articles++;
    for (p = data->overview + data->overlen - 1; p > data->overview + 5; p--)
        if (*p == ':' && strncasecmp(p - 5, "\tXref", 5) == 0) {
            xref = p + 2;
            break;
        }
    if (xref == NULL) {
        warn("no Xref found in overview data %s", data->overview);
        return;
    }
    gettimeofday(&start, NULL);
    if (!overview_add_xref(overview, xref, data))
        warn("cannot write overview data for %s", TokenToText(data->token));
    gettimeofday(&end, NULL);
    statistics->busy += (end.tv_sec  - start.tv_sec)  * 1000;
    statistics->busy += (end.tv_usec - start.tv_usec) / 1000;
}


/*
**  Process a single file.  Takes the open overview struct, the file name
**  (which may be - to process standard intput), and the statistics struct and
**  calls parse_line and write_overview for each line.
*/
static void
process_file(struct overview *overview, const char *file,
             struct statistics *statistics)
{
    char *line;
    struct overview_data data;
    QIOSTATE *qp;

    if (strcmp(file, "-") == 0)
        qp = QIOfdopen(STDIN_FILENO);
    else
        qp = QIOopen(file);
    if (qp == NULL) {
        syswarn("cannot open %s", file);
        return;
    }

    while (1) {
        line = QIOread(qp);
        if (line == NULL) {
            if (QIOtoolong(qp)) {
                warn("input line too long, skipping");
                continue;
            }
            break;
        }
        if (!parse_line(line, &data))
            continue;
        write_overview(overview, &data, statistics);
    }
    QIOclose(qp);
}


int
main(int argc, char *argv[])
{
    struct timeval start, end;
    struct statistics statistics;
    struct overview *overview;
    unsigned long total;

    /* First thing, set up our identity. */
    message_program_name = "overchan";

    /* Log warnings and fatal errors to syslog unless we were given command
       line arguments, since we're probably running under innd. */
    if (argc == 1) {
        openlog("overchan", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
        message_handlers_warn(1, message_log_syslog_err);
        message_handlers_die(1, message_log_syslog_err);
        message_handlers_notice(1, message_log_syslog_notice);
    }

    /* Skip the program name.  Any subsequent arguments will be taken to be
       names of input files. */
    argc -= 1;
    argv += 1;

    /* Set defaults. */
    if (!innconf_read(NULL))
        exit(1);
    umask(NEWSUMASK);
    memset(&statistics, 0, sizeof(statistics));
    if (innconf->enableoverview && !innconf->useoverchan)
        warn("overchan is running while innd is creating overview data (you"
             " can ignore this message if you are running makehistory -F)");
    overview = overview_open(OV_WRITE);
    if (overview == NULL)
        die("cannot open overview");
    gettimeofday(&start, NULL);

    /* Process the files. */
    if (argc == 0)
        process_file(overview, "-", &statistics);
    else
        for (; *argv != NULL; argv++)
            process_file(overview, *argv, &statistics);
    overview_close(overview);
    gettimeofday(&end, NULL);
    total = (end.tv_sec - start.tv_sec) * 1000;
    total += (end.tv_usec - start.tv_usec) / 1000;
    notice("timings %lu arts %lu of %lu ms", statistics.articles,
           statistics.busy, total);
    exit(0);
}
