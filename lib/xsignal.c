/*
**  Provides a 'reliable' implementation of signal() for SYSV-derived systems
*/
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include "configdata.h"
#include "clibrary.h"

#ifdef USE_SIGACTION

void (*xsignal(int signum, void (*sigfunc)(int )))(int )
{
	struct sigaction act, oact;

	act.sa_handler = sigfunc;
	sigemptyset(&act.sa_mask);

#ifdef SA_RESTART
	act.sa_flags = SA_RESTART;
#else
	act.sa_flags = 0;
#endif

	if (sigaction(signum, &act, &oact) < 0) {
		return SIG_ERR;
	}
	return oact.sa_handler;
}

#else

void (*xsignal(int signum, void (*sigfunc)(int )))(int )
{
	return signal(signum, sigfunc);
}

#endif
