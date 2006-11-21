/*  $Id$
**
**  A reliable implementation of signal for System V systems.
**
**  Two functions are provided, xsignal and xsignal_norestart.  The former
**  attempts to set system calls to be restarted and the latter does not.
**
**  Be aware that there's weird declaration stuff going on here; a signal
**  handler is a pointer to a function taking an int and returning void.
**  We typedef this as sig_handler_type for clearer code.
*/

#include "config.h"
#include "inn/libinn.h"
#include <signal.h>

typedef void (*sig_handler_type)(int);

#ifdef HAVE_SIGACTION

sig_handler_type
xsignal(int signum, sig_handler_type sigfunc)
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

sig_handler_type
xsignal_norestart(int signum, sig_handler_type sigfunc)
{
    struct sigaction act, oact;

    act.sa_handler = sigfunc;
    sigemptyset(&act.sa_mask);

    /* Try not to restart system calls. */
#ifdef SA_INTERRUPT
    act.sa_flags = SA_INTERRUPT;
#else
    act.sa_flags = 0;
#endif

    if (sigaction(signum, &act, &oact) < 0)
        return SIG_ERR;
    return oact.sa_handler;
}

#else /* !HAVE_SIGACTION */

sig_handler_type
xsignal(int signum, sig_handler_type sigfunc)
{
    return signal(signum, sigfunc);
}

sig_handler_type
xsignal_norestart(int signum, sig_handler_type sigfunc)
{
    return signal(signum, sigfunc);
}

#endif /* !HAVE_SIGACTION */
