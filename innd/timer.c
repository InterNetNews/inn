/*  $Id$
**
**  Timer functions, to gather profiling data.
**
**  These functions log profiling information about where the server spends
**  its time, and are fast enough to be left always on.  While it doesn't
**  provide as detailed of information as a profiling build would, it's
**  much faster and simpler.
**
**  Functions that should have their time monitored need to call
**  TMRstart(n) at the beginning of the segment of code and TMRstop(n)
**  at the end.  The time spent will be accumulated and added to the
**  total for the counter n, where n should be one of the constants in
**  innd.h.  If you add new timers, add them to innd.h and also add a
**  description to TMRsummary, and also add them to innreport.
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
**  TMRmainloophook() must be called with none of the timers started.  If
**  there is a timer running at the time that it's called, that timer may
**  report strange results later (such as extremely large times).
**
**  Note that this code is not thread-safe and in fact would need to be
**  completely overhauled for a threaded server (since the idea of global
**  timing statistics doesn't make as much sense when different tasks are
**  done in different threads).
*/

#include "clibrary.h"
#include "innd.h"

/* Names for all of the timers.  These must be given in the same order as
   the definition of enum timer in innd.h. */
static const char * const timer_name[TMR_MAX] = {
    "idle", "hishave", "hisgrep", "hiswrite", "hissync",
    "artclean", "artwrite", "artctrl", "artcncl",
    "sitesend", "overv", "perl", "python"
};

/* Timer values.  start stores the time (relative to the last summary) at
   which TMRstart was last called for each timer, or 0 if the timer hasn't
   been started.  cumulative is the total time accrued by that timer since
   the last summary.  count is the number of times the timer has been
   stopped since the last summary. */
static unsigned long start[TMR_MAX];
static unsigned long cumulative[TMR_MAX];
static unsigned long count[TMR_MAX];

/* The time of the last summary, used as a base for times returned by
   get_time.  Formerly, times were relative to the last call to TMRinit,
   which was only called once when innd was starting up; with that approach,
   times may overflow a 32-bit unsigned long about 50 days after the server
   starts up.  While this may still work due to unsigned arithmetic, this
   approach is less confusing to follow. */
static struct timeval base;


/*
**  Returns the number of milliseconds since the base time.  This gives
**  better resolution than time, but the return value is a lot easier to
**  work with than a struct timeval.  If the argument is true, also reset
**  the base time.
*/
static unsigned long
get_time(bool reset)
{
    struct timeval tv;
    unsigned long now;

    gettimeofday(&tv, NULL);
    now = (tv.tv_sec - base.tv_sec) * 1000;
    now += (tv.tv_usec - base.tv_usec) / 1000;
    if (reset)
        base = tv;
    return now;
}


/*
**  Summarize the current timer statistics and then reset them for the next
**  polling interval.  To find the needed buffer size, note that a 64-bit
**  unsigned number can be up to 20 digits long, so each timer can be 52
**  characters.  Allowing another 29 characters for the introductory
**  message, a buffer size of 2048 allows for 38 timers and should still be
**  small enough to fit safely on the stack.
*/
static void
summarize(void)
{
    char buffer[2048];
    char result[256];
    int i;
    int length = SIZEOF(buffer) - 1;

    length -= sprintf(buffer, "ME time %ld ", get_time(true));
    for (i = 0; i < TMR_MAX; i++) {
	if (cumulative[i] != 0 || count[i] != 0) {
	    length -= sprintf(result, "%s %lu(%lu) ", timer_name[i],
			      cumulative[i], count[i]);
	    if (length < 0)
		break;
	    strcat(buffer, result);
	    cumulative[i] = 0;
	    count[i] = 0;
        }
    }
    syslog(LOG_NOTICE, "%s", buffer);
}


/*
**  Initialize the timer.  Zero out even variables that would initially be
**  zero so that this function can be called multiple times if wanted.
*/
void
TMRinit(void)
{
    int i;

    /* Establish a base time. */
    get_time(true);

    for (i = 0; i < TMR_MAX; i++) {
        start[i] = 0;
        cumulative[i] = 0;
	count[i] = 0;
    }
}


/*
**  This function is called every time through innd's main loop.  It checks
**  to see if innconf->timer seconds have passed since the last summary,
**  and if so calls summarize.  It returns the number of seconds until the
**  next summary should occur (so that the main loop can use this as a
**  timeout value to select).
*/
int
TMRmainloophook(void)
{
    unsigned long now;

    if (!innconf->timer)
	return 0;
    now = get_time(false);

    if (now >= (unsigned int)innconf->timer * 1000) {
        summarize();
        return innconf->timer;
    } else {
        return innconf->timer - now / 1000;
    }
}


/*
**  Start a particular timer.
*/
void
TMRstart(enum timer timer)
{
    if (!innconf->timer)
	return;
    start[timer] = get_time(false);
}


/*
**  Stop a particular timer, adding the total time to cumulative and
**  incrementing the count of times that timer has been invoked.
*/
void
TMRstop(enum timer timer)
{
    if (!innconf->timer)
	return;
    cumulative[timer] += get_time(false) - start[timer];
    count[timer]++;
}
