/* timer.c */

/* A timer for innfeed to gather profiling data. The code is based upon
   the original innd timer. See ../innd/timer.c for details. */
/* Fabien Tassin <fta@sofaraway.org> 2001-01-16 */

#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>

#include "clibrary.h"
#include "innfeed.h"
#include "libinn.h"

unsigned        start[TMR_MAX];
unsigned        cumulative[TMR_MAX];
unsigned        count[TMR_MAX];

int             maxtimer = 0;
unsigned        last_summary;

static unsigned gettime(void) {
    static int                  init = 0;
    static struct timeval       start_tv;
    struct timeval              tv;
    
    if (! init) {
        gettimeofday(&start_tv, NULL);
        init++;
    }
    gettimeofday(&tv, NULL);
    return((tv.tv_sec  - start_tv.tv_sec)  * 1000 +
           (tv.tv_usec - start_tv.tv_usec) / 1000);
}

void TMRinit(void) {
    int i;
    
    for (i = 0; i < TMR_MAX; i++)
        count[i] = start[i] = cumulative[i] = 0;
}

void TMRdosummary(unsigned secs) {
    char buffer[8192];
    char buf[256];
    char *str;
    int i;
    
    sprintf(buffer, "ME time %d ", secs);
    for (i = 0; i < maxtimer; i++) {
        str = "???";
        switch (i) {
        case TMR_IDLE:          str = "idle";		break;
	case TMR_BACKLOGSTATS:	str = "blstats";	break;
	case TMR_STATUSFILE:	str = "stsfile";	break;
        case TMR_NEWARTICLE:	str = "newart";		break;
	case TMR_READART:	str = "readart";	break;
	case TMR_PREPART:	str = "prepart";	break;
	case TMR_READ:		str = "read";		break;
	case TMR_WRITE:		str = "write";		break;
	case TMR_CALLBACK:	str = "cb";		break;
        }
        sprintf(buf, "%s %d(%d) ", str, cumulative[i], count[i]);
        cumulative[i] = count[i] = 0;
        strcat(buffer, buf);
    }
    syslog(L_NOTICE, "%s", buffer);
}

void TMRstart(TMRTYPE t) {
    if (!innconf->timer)
        return;
    if (!maxtimer)
        maxtimer = TMR_MAX;
    start[t] = gettime();
}


void TMRstop(TMRTYPE t) {
    if (!innconf->timer)
        return;
    cumulative[t] += gettime() - start[t];
    count[t]++;
}

void TMRmainloophook(void) {
    unsigned now;

    if (!innconf->timer)
        return;
    now = gettime();
    if (now - last_summary > (innconf->timer * 1000)) {
        TMRdosummary(now - last_summary);
        last_summary = now;
    }
}

