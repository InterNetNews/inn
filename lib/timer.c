/*  $Id$
**
**  Timer functions, to gather profiling data.
**
**  These functions log profiling information about where the server spends
**  its time, and are fast enough to be left always on.  While it doesn't
**  provide as detailed of information as a profiling build would, it's
**  much faster and simpler.
**
**  Functions that should have their time monitored need to call TMRstart(n)
**  at the beginning of the segment of code and TMRstop(n) at the end.  The
**  time spent will be accumulated and added to the total for the counter n,
**  where n should be one of the constants in timer.h or defined in your
**  application.  If you add new timers in the library code, add them to
**  timer.h and also add a description to TMRsummary; if you add them in
**  your application add them to your own description array.  Also add them
**  to innreport.
**
**  There are no sanity checks to see if you have failed to start a timer,
**  etc., etc.  It is assumed that the user is moderately intelligent and
**  can properly deploy this code.
**
**  Recursion is not allowed on a given timer.  Setting multiple timers
**  at once is fine (i.e., you may have a timer for the total time to write
**  an article, how long the disk write takes, how long the history update
**  takes, etc. which are components of the total article write time).
**  innreport wouldn't handle such nested timers correctly without some
**  changes, however.
**
**  Note that this code is not thread-safe and in fact would need to be
**  completely overhauled for a threaded server (since the idea of global
**  timing statistics doesn't make as much sense when different tasks are
**  done in different threads).
*/

#include "config.h"
#include "clibrary.h"
#include "libinn.h"
#include "inn/timer.h"
#include <syslog.h>

/* Timer values.  start stores the time (relative to the last summary) at
   which TMRstart was last called for each timer, or 0 if the timer hasn't
   been started.  cumulative is the total time accrued by that timer since
   the last summary.  count is the number of times the timer has been
   stopped since the last summary. */
static unsigned long *start;
static unsigned long *cumulative;
static unsigned long *count;
static unsigned int timer_count;

/* Names for all of the timers.  These must be given in the same order
   as the definition of the enum in timer.h. */
static const char * const timer_name[TMR_APPLICATION] = {
    "hishave", "hisgrep", "hiswrite", "hissync",
};


/*
**  Returns the number of milliseconds since the base time.  This gives
**  better resolution than time, but the return value is a lot easier to
**  work with than a struct timeval.  If the argument is true, also reset
**  the base time.
*/
static unsigned long
get_time(bool reset)
{
    unsigned long now;
    struct timeval tv;

    /* The time of the last summary, used as a base for times returned
       by TMRnow.  Formerly, times were relative to the last call to
       TMRinit, which was only called once when innd was starting up;
       with that approach, times may overflow a 32-bit unsigned long
       about 50 days after the server starts up.  While this may still
       work due to unsigned arithmetic, this approach is less
       confusing to follow. */
    static struct timeval base;

    gettimeofday(&tv, NULL);
    now = (tv.tv_sec - base.tv_sec) * 1000;
    now += (tv.tv_usec - base.tv_usec) / 1000;
    if (reset)
        base = tv;
    return now;
}

/*
**  Return the current time in milliseconds since the last summary or the
**  initialization of the timer.  This is intended for use by the caller to
**  determine when next to call TMRsummary.
*/
unsigned long
TMRnow(void)
{
    return get_time(false);
}

/*
**  Summarize the current timer statistics and then reset them for the next
**  polling interval.  To find the needed buffer size, note that a 64-bit
**  unsigned number can be up to 20 digits long, so each timer can be 52
**  characters.  We also allow another 29 characters for the introductory
**  message.
*/
void
TMRsummary(const char *const *labels)
{
    char result[256];
    char *buffer;
    unsigned int i;
    ssize_t length;

    length = 52 * timer_count + 29;
    buffer = xmalloc(length + 1);
    length -= snprintf(buffer, length + 1, "ME time %ld ", get_time(true));
    for (i = 0; i < timer_count; i++) {
        if (cumulative[i] != 0 || count[i] != 0) {
            const char *name;

            if (i < TMR_APPLICATION)
                name = timer_name[i];
            else
                name = labels[i - TMR_APPLICATION];
            length -= snprintf(result, sizeof(result), "%s %lu(%lu) ",
                               name, cumulative[i], count[i]);
            if (length < 0)
                break;
            strcat(buffer, result);
            cumulative[i] = 0;
            count[i] = 0;
        }
    }
    syslog(LOG_NOTICE, "%s", buffer);
    free(buffer);
}


/*
**  Initialize the timer.  Zero out even variables that would initially be
**  zero so that this function can be called multiple times if wanted.
*/
void
TMRinit(unsigned int timers)
{
    int i;

    /* Make sure we're multiple call safe and free any arrays we had. */
    if (start) {
        free(start);
        start = NULL;
    }
    if (cumulative) {
        free(cumulative);
        cumulative = NULL;
    }
    if (count) {
        free(count);
        count = NULL;
    }

    /* To disable the timers you can do TMRinit(0), so watch for that
       case. */ 
    if (timers != 0) {
        start = xmalloc(timers * sizeof(*start));
        cumulative = xmalloc(timers * sizeof(*cumulative));
        count = xmalloc(timers * sizeof(*count));

        for (i = 0; i < timers; i++) {
            start[i] = 0;
            cumulative[i] = 0;
            count[i] = 0;
        }
        get_time(true);
    }
    timer_count = timers;
}

/*
**  Start a particular timer.
*/
void
TMRstart(unsigned int timer)
{
    if (timer < timer_count)
        start[timer] = get_time(false);
}


/*
**  Stop a particular timer, adding the total time to cumulative and
**  incrementing the count of times that timer has been invoked.
*/
void
TMRstop(unsigned int timer)
{
    if (timer < timer_count) {
        cumulative[timer] += get_time(false) - start[timer];
        count[timer]++;
    }
}
