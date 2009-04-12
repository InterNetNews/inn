/*  $Id$
**
**  Process control routines.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"

#include "innd.h"


static PROCESS	*PROCtable;
static int	PROCtablesize;
static PROCESS	PROCnull = { PSfree, 0, 0, 0, 0, 0 };


/*
**  Collect dead processes.
*/
static void
PROCreap(void)
{
    int		status;
    PROCESS	*pp;
    int         i;
    pid_t	pid;

    for ( ; ; ) {
	pid = waitpid(-1, &status, WNOHANG);
	if (pid == 0)
	    break;
	if (pid < 0) {
	    if (errno != ECHILD)
		syslog(L_ERROR, "%s cant wait %m", LogName);
	    break;
	}
	for (pp = PROCtable, i = PROCtablesize; --i >= 0; pp++)
	    if (pp->Pid == pid) {
		PROCneedscan = true;
		pp->Status = WEXITSTATUS(status);
		pp->State = PSdead;
		pp->Collected = Now.tv_sec;
		break;
	    }
    }
}


/*
**  Signal handler that collects the processes, then resets the signal.
*/
static void
PROCcatchsignal(int s)
{
    PROCreap();

#ifndef HAVE_SIGACTION
    xsignal(s, PROCcatchsignal);
#else
    s = s;			/* ARGSUSED */
#endif
}


/*
**  Synchronous version that notifies a site when its process went away.
*/
void
PROCscan(void)
{
    PROCESS	*pp;
    int	i;

    for (pp = PROCtable, i = PROCtablesize; --i >= 0; pp++)
	if (pp->State == PSdead) {
	    if (pp->Site >= 0)
		SITEprocdied(&Sites[pp->Site], pp - PROCtable, pp);
	    pp->State = PSfree;
	}
    PROCneedscan = false;
}


#if	0
/*
**  Close down all processes.
*/
void
PROCclose(Quickly)
    bool		Quickly;
{
    int	sig;
    PROCESS	*pp;
    int	i;

    /* What signal are we sending? */
    sig = Quickly ? SIGKILL : SIGTERM;

    /* Send the signal to all living processes. */
    for (pp = PROCtable, i = PROCtablesize; --i >= 0; pp++) {
	if (pp->State != PSrunning)
	    continue;
	if (kill(pp->Pid, sig) < 0 && errno != ESRCH)
	    syslog(L_ERROR, "%s cant kill %s %ld %m",
		LogName, Quickly ? "KILL" : "TERM", (long) pp->Pid);
    }

    /* Collect any who might have died. */
    PROCreap();
    for (pp = PROCtable, i = PROCtablesize; --i >= 0; pp++)
	if (pp->State == PSdead)
	    *pp = PROCnull;
}
#endif	/* 0 */


/*
**  Stop watching a process -- we don't care about it any more.
*/
void
PROCunwatch(int process)
{
    if (process < 0 || process >= PROCtablesize) {
	syslog(L_ERROR, "%s internal PROCunwatch %d", LogName, process);
	return;
    }
    PROCtable[process].Site = -1;
}


/*
**  Add a pid to the list of processes we watch.
*/
int
PROCwatch(pid_t pid, int site)
{
    PROCESS     *pp;
    int         i;

    /* Find a free slot for this process. */
    for (pp = PROCtable, i = PROCtablesize; --i >= 0; pp++)
	if (pp->State == PSfree)
	    break;
    if (i < 0) {
	/* Ran out of room -- grow the table. */
        PROCtable = xrealloc(PROCtable, (PROCtablesize + 20) * sizeof(PROCESS));
        for (pp = &PROCtable[PROCtablesize], i=20; --i >= 0; pp++)
          *pp = PROCnull;
	pp = &PROCtable[PROCtablesize];
	PROCtablesize += 20;
    }

    pp->State = PSrunning;
    pp->Pid = pid;
    pp->Started = Now.tv_sec;
    pp->Site = site;
    return pp - PROCtable;
}


/*
**  Setup.
*/
void
PROCsetup(int i)
{
    PROCESS	*pp;

    if (PROCtable)
	free(PROCtable);
    PROCtablesize = i;
    PROCtable = xmalloc(PROCtablesize * sizeof(PROCESS));
    for (pp = PROCtable, i = PROCtablesize; --i >= 0; pp++)
	*pp = PROCnull;

#if	defined(SIGCHLD)
    xsignal(SIGCHLD, PROCcatchsignal);
#endif	/* defined(SIGCHLD) */
    xsignal(SIGPIPE, PROCcatchsignal);
}
