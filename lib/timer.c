/*  $Id$
**
**  Timer functions, to gather profiling data.
**
**  These functions log profiling information about where the server spends
**  its time.  While this doesn't provide as detailed of information as a
**  profiling build would, it's much faster and simpler, and since it's fast
**  enough to always leave on even on production servers, it can gather
**  information *before* it's needed and show long-term trends.
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
**  Calls are sanity-checked to some degree and errors reported via
**  warn/die, so all callers should have the proper warn and die handlers
**  set up, if appropriate.
**
**  Recursion is not allowed on a given timer.  Setting multiple timers
**  at once is fine (i.e., you may have a timer for the total time to write
**  an article, how long the disk write takes, how long the history update
**  takes, etc. which are components of the total article write time).  If a
**  timer is started while another timer is running, the new timer is
**  considered to be a sub-timer of the running timer, and must be stopped
**  before the parent timer is stopped.  Note that the same timer number can
**  be a sub-timer of more than one timer or a timer without a parent, and
**  each of those counts will be reported separately.
**
**  Note that this code is not thread-safe and in fact would need to be
**  completely overhauled for a threaded server (since the idea of global
**  timing statistics doesn't make as much sense when different tasks are
**  done in different threads).
*/

#include "config.h"
#include "clibrary.h"
#include "portable/time.h"
#include <syslog.h>

#include "inn/messages.h"
#include "inn/timer.h"
#include "inn/libinn.h"

/* Timer values are stored in a series of trees.  This allows use to use
   nested timers.  Each nested timer node is linked to three of its
   neighbours to make lookups easy and fast.  The current position in the
   graph is given by timer_current.

   As an optimization, since most timers aren't nested, timer_list holds an
   array of pointers to non-nested timers that's filled in as TMRstart is
   called so that the non-nested case remains O(1).  That array is stored in
   timers.  This is the "top level" of the timer trees; if timer_current is
   NULL, any timer that's started is found in this array.  If timer_current
   isn't NULL, there's a running timer, and starting a new timer adds to
   that tree.

   Note that without the parent pointer, this is a tree.  id is the
   identifier of the timer.  start stores the time (relative to the last
   summary) at which TMRstart was last called for each timer.  total is
   the total time accrued by that timer since the last summary.  count is
   the number of times the timer has been stopped since the last summary. */
struct timer {
    unsigned int id;
    unsigned long start;
    unsigned long total;
    unsigned long count;

    struct timer *parent;
    struct timer *brother;
    struct timer *child;
};
static struct timer **timers = NULL;
static struct timer *timer_current = NULL;
unsigned int timer_count = 0;

/* Names for all of the timers.  These must be given in the same order
   as the definition of the enum in timer.h. */
static const char *const timer_name[TMR_APPLICATION] = {
    "hishave", "hisgrep", "hiswrite", "hissync",
};


/*
**  Returns the current time as a double.  This is not used by any of the
**  other timer code, but is used by various programs right now to keep track
**  of elapsed time.
*/
double
TMRnow_double(void)
{
    struct timeval tv;

    gettimeofday(&tv, NULL);
    return (tv.tv_sec + tv.tv_usec * 1.0e-6);
}


/*
**  Returns the number of milliseconds since the base time.  This gives
**  better resolution than time, but the return value is a lot easier to
**  work with than a struct timeval.  If the argument is true, also reset
**  the base time.
*/
static unsigned long
TMRgettime(bool reset)
{
    unsigned long now;
    struct timeval tv;

    /* The time of the last summary, used as a base for times returned by
       TMRnow.  Formerly, times were relative to the last call to TMRinit,
       which was only called once when innd was starting up; with that
       approach, times may overflow a 32-bit unsigned long about 50 days
       after the server starts up.  While this may still work due to unsigned 
       arithmetic, this approach is less confusing to follow. */
    static struct timeval base;

    gettimeofday(&tv, NULL);
    now = (tv.tv_sec - base.tv_sec) * 1000;
    now += (tv.tv_usec - base.tv_usec) / 1000;
    if (reset)
        base = tv;
    return now;
}


/*
**  Initialize the timer.  Zero out even variables that would initially be
**  zero so that this function can be called multiple times if wanted.
*/
void
TMRinit(unsigned int count)
{
    unsigned int i;

    /* TMRinit(0) disables all timers. */
    TMRfree();
    if (count != 0) {
        timers = xmalloc(count * sizeof(struct timer *));
        for (i = 0; i < count; i++)
            timers[i] = NULL;
        TMRgettime(true);
    }
    timer_count = count;
}


/*
**  Recursively destroy a timer node.
*/
static void
TMRfreeone(struct timer *timer)
{
    if (timer == NULL)
        return;
    if (timer->child != NULL)
        TMRfreeone(timer->child);
    if (timer->brother != NULL)
        TMRfreeone(timer->brother);
    free(timer);
}


/*
**  Free all timers and the resources devoted to them.
*/
void
TMRfree(void)
{
    unsigned int i;

    if (timers != NULL)
        for (i = 0; i < timer_count; i++)
            TMRfreeone(timers[i]);
    free(timers);
    timers = NULL;
    timer_count = 0;
}


/*
**  Allocate a new timer node.  Takes the id and the parent pointer.
*/
static struct timer *
TMRnew(unsigned int id, struct timer *parent)
{
    struct timer *timer;

    timer = xmalloc(sizeof(struct timer));
    timer->parent = parent;
    timer->brother = NULL;
    timer->child = NULL;
    timer->id = id;
    timer->start = 0;
    timer->total = 0;
    timer->count = 0;
    return timer;
}


/*
**  Start a particular timer.  If no timer is currently running, start one
**  of the top-level timers in the timers array (creating a new one if
**  needed).  Otherwise, search for the timer among the children of the
**  currently running timer, again creating a new timer if necessary.
*/
void
TMRstart(unsigned int timer)
{
    struct timer *search;

    if (timer_count == 0) {
        /* this should happen if innconf->timer == 0 */
        return;
    }
    if (timer >= timer_count) {
        warn("timer %u is larger than the maximum timer %u, ignored",
             timer, timer_count - 1);
        return;
    }

    /* timers will be non-NULL if timer_count > 0. */
    if (timer_current == NULL) {
        if (timers[timer] == NULL)
            timers[timer] = TMRnew(timer, NULL);
        timer_current = timers[timer];
    } else {
        search = timer_current;

        /* Go to the "child" level and look for the good "brother"; the
           "brothers" are a simple linked list. */
        if (search->child == NULL) {
            search->child = TMRnew(timer, search);
            timer_current = search->child;
        } else {
            search = search->child;
            while (search->id != timer && search->brother != NULL)
                search = search->brother;
            if (search->id != timer) {
                search->brother = TMRnew(timer, search->parent);
                timer_current = search->brother;
            } else {
                timer_current = search;
            }
        }
    }
    timer_current->start = TMRgettime(false);
}


/*
**  Stop a particular timer, adding the total time to total and incrementing
**  the count of times that timer has been invoked.
*/
void
TMRstop(unsigned int timer)
{
    if (timer_count == 0) {
        /* this should happen if innconf->timer == 0 */
        return;
    }
    if (timer_current == NULL)
        warn("timer %u stopped when no timer was running", timer);
    else if (timer != timer_current->id)
        warn("timer %u stopped doesn't match running timer %u", timer,
             timer_current->id);
    else {
        timer_current->total += TMRgettime(false) - timer_current->start;
        timer_current->count++;
        timer_current = timer_current->parent;
    }
}


/*
**  Return the current time in milliseconds since the last summary or the
**  initialization of the timer.  This is intended for use by the caller to
**  determine when next to call TMRsummary.
*/
unsigned long
TMRnow(void)
{
    return TMRgettime(false);
}


/*
**  Return the label associated with timer number id.  Used internally
**  to do the right thing when fetching from the timer_name or labels
**  arrays
*/
static const char *
TMRlabel(const char *const *labels, unsigned int id)
{
    if (id >= TMR_APPLICATION)
        return labels[id - TMR_APPLICATION];
    else
        return timer_name[id];
}



/*
**  Recursively summarize a single timer tree into the supplied buffer,
**  returning the number of characters added to the buffer.
*/
static size_t
TMRsumone(const char *const *labels, struct timer *timer, char *buf,
          size_t len)
{
    struct timer *node;
    size_t off = 0;

    /* This results in "child/parent nn(nn)" instead of the arguably more
       intuitive "parent/child" but it's easy.  Since we ensure sane snprintf 
       semantics, it's safe to defer checking for overflow until after
       formatting all of the timer data. */
    for (node = timer; node != NULL; node = node->parent)
        off += snprintf(buf + off, len - off, "%s/",
                        TMRlabel(labels, node->id));
    off--;
    off += snprintf(buf + off, len - off, " %lu(%lu) ", timer->total,
                    timer->count);
    if (off == len) {
        warn("timer log too long while processing %s",
             TMRlabel(labels, timer->id));
        return 0;
    }

    timer->total = 0;
    timer->count = 0;
    if (timer->child != NULL)
        off += TMRsumone(labels, timer->child, buf + off, len - off);
    if (timer->brother != NULL)
        off += TMRsumone(labels, timer->brother, buf + off, len - off);
    return off;
}


/*
**  Summarize the current timer statistics, report them to syslog, and then
**  reset them for the next polling interval.
*/
void
TMRsummary(const char *prefix, const char *const *labels)
{
    char *buf;
    unsigned int i;
    size_t len, off;

    /* To find the needed buffer size, note that a 64-bit unsigned number can 
       be up to 20 digits long, so each timer can be 52 characters.  We also
       allow another 27 characters for the introductory timestamp, plus some
       for the prefix.  We may have timers recurring at multiple points in
       the structure, so this may not be long enough, but this is over-sized
       enough that it shouldn't be a problem.  We use snprintf, so if the
       buffer isn't large enough it will just result in logged errors. */
    len = 52 * timer_count + 27 + (prefix == NULL ? 0 : strlen(prefix)) + 1;
    buf = xmalloc(len);
    if (prefix == NULL)
        off = 0;
    else
        off = snprintf(buf, len, "%s ", prefix);
    off += snprintf(buf + off, len - off, "time %lu ", TMRgettime(true));
    for (i = 0; i < timer_count; i++)
        if (timers[i] != NULL)
            off += TMRsumone(labels, timers[i], buf + off, len - off);
    syslog(LOG_NOTICE, "%s", buf);
    free(buf);
}
