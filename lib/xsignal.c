/*  $Id$
**
**  Provides a 'reliable' implementation of signal() for SYSV-derived
**  systems.
**
**  Be aware that there's weird declaration stuff going on here; a signal
**  handler is a pointer to a function taking an int and returning void.
**  We typedef this as SIG_HANDLER_T for clearer code.
*/
#include "config.h"
#include "libinn.h"
#include <signal.h>

typedef void (*SIG_HANDLER_T)(int);

#ifdef HAVE_SIGACTION

SIG_HANDLER_T
xsignal(int signum, SIG_HANDLER_T sigfunc)
{
    struct sigaction act, oact;

    act.sa_handler = sigfunc;
    sigemptyset(&act.sa_mask);

    /* Try to restart system calls if possible. */
#ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#else
    act.sa_flags = 0;
#endif

    if (sigaction(signum, &act, &oact) < 0)
        return SIG_ERR;
    return oact.sa_handler;
}

#else /* !HAVE_SIGACTION */

SIG_HANDLER_T
xsignal(int signum, SIG_HANDLER_T sigfunc)
{
    return signal(signum, sigfunc);
}

#endif /* !HAVE_SIGACTION */
