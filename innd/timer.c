/* $Id$
**
** Timer functions, to gather profiling data.
*/
#include "clibrary.h"
#include "innd.h"

unsigned	start[TMR_MAX];
unsigned	cumulative[TMR_MAX];
unsigned	count[TMR_MAX];

unsigned	last_summary;

int		maxtimer = 0;

/*
 * This package is a quick hack to allow me to draw some conclusions
 * about INN and "where the time goes" without having to run profiling
 * and continually stop/restart the server.
 *
 * Functions that should have their time monitored need to call
 * TMRstart(n) at the beginning of the segment of code and TMRstop(n)
 * at the end.  The time spent will be accumulated and added to the
 * total for the counter 'n'.  'n' is probably one of the constants in 
 * timer.h.  You probably want to add a description to TMRsummary
 * explaining what your 'n' represents.
 *
 * There are no sanity checks to see if you have failed to start a timer,
 * etc., etc.  It is assumed that the user is moderately intelligent and
 * can properly deploy this code.
 *
 * Recursion is not allowed on a given timer.  Setting multiple timers
 * is fine (i.e., you may have a timer for the total time to write an
 * article, how long the disk write takes, how long the history update
 * takes, blah blah.. which are components of the total art write time)
 *
 * This package is written with the assumption that TMRmainloophook()
 * will be called with none of the timers started.  The statistics are
 * generated here.  It may not be fatal to have a timer running, but
 * it will not be properly accounted for (at least within that time slice).
 */

/*
 * This function is designed to report the number of milliseconds since
 * the first invocation.  I wanted better resolution than time(), and
 * something easier to work with than gettimeofday()'s struct timeval's.
 */

static unsigned gettime(void)
{
    static int			init = 0;
    static struct timeval	start_tv;
    struct timeval		tv;
    
    if (! init) {
	gettimeofday(&start_tv, NULL);
	init++;
    }
    gettimeofday(&tv, NULL);
    return((tv.tv_sec - start_tv.tv_sec) * 1000 + (tv.tv_usec - start_tv.tv_usec) / 1000);
}

void TMRinit(void)
{
    int i;
    
    last_summary = gettime();	/* First invocation */
    
    for (i = 0; i < TMR_MAX; i++) {
	count[i] = start[i] = cumulative[i] = 0;
    }
}


static void dosummary(unsigned secs)
{
    char buffer[8192];
    char buf[256];
    char *str;
    int i;
    
    sprintf(buffer, "ME time %d ", secs);
    for (i = 0; i < maxtimer; i++) {
	str = "???";
	switch (i) {
	case TMR_IDLE:		str = "idle";  break;
	case TMR_ARTWRITE:	str = "artwrite";  break;
	case TMR_ARTLINK:	str = "artlink";  break;
	case TMR_HISWRITE:	str = "hiswrite";  break;
	case TMR_HISSYNC:	str = "hissync";  break;
	case TMR_SITESEND:	str = "sitesend";  break;
	case TMR_ARTCTRL:	str = "artctrl";  break;
	case TMR_ARTCNCL:	str = "artcncl";  break;
	case TMR_HISHAVE:	str = "hishave";  break;
	case TMR_HISGREP:	str = "hisgrep";  break;
	case TMR_OVERV:		str = "overv";  break;
	case TMR_PERL:		str = "perl";  break;
	case TMR_PYTHON:	str = "python";  break;
	}
	sprintf(buf, "%s %d(%d) ", str, cumulative[i], count[i]);
	cumulative[i] = count[i] = 0;
	strcat(buffer, buf);
    }
    syslog(L_NOTICE, "%s", buffer);
}


int
TMRmainloophook(void)
{
    unsigned now;
    
    if (!innconf->timer)
	return 0;
    now = gettime();
    
    if (now - last_summary >= (innconf->timer * 1000)) {
	dosummary(now - last_summary);
	last_summary = now;
        return 0;
    }
    return innconf->timer - (now - last_summary) / 1000;
}


void		TMRstart(TMRTYPE t)
{
    if (!innconf->timer)
	return;
    if (!maxtimer)
	maxtimer = TMR_MAX;
    start[t] = gettime();
}


void		TMRstop(TMRTYPE t)
{
    if (!innconf->timer)
	return;
    cumulative[t] += gettime() - start[t];
    count[t]++;
}
