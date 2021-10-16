/*
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
#include <errno.h>
#include <signal.h>

typedef void (*sig_handler_type)(int);

#ifdef HAVE_SIGACTION

/* We mask signals when not in select().
 *
 * This excludes the possibility of a signal handler accessing data that
 * the main code is mutating.
 *
 * signals_masked is signal mask we run with outside select(). It is
 * initialised to the signal mask that held on entry, with the signals
 * we handle added.
 *
 * signals_unmasked is signal mask we run with during select(). It is
 * initialised to the signal mask that held on entry, with the signals
 * we handle removed.
 *
 * Both sets are intended to be arguments to SIG_SETMASK, not
 * to SIG_BLOCK or SIG_UNBLOCK.
 *
 * Signals that are ignored or set to SIG_DFL are never masked.
 */
static bool signal_masking = false;
static int signal_max;
static sigset_t signals_masked, signals_unmasked;

/* If signal masking is enabled, add/remove signum from
 * signals_masked, and update the current mask to match.
 */
static void
set_signal_handled(int signum, sig_handler_type sigfunc)
{
    if (signal_masking) {
        if (signum > signal_max) {
            /* We track the maximum handled signal so that
             * we can efficiently reconfigure signals in
             * xsignal_forked(). */
            signal_max = signum;
        }
        if (sigfunc != SIG_IGN && sigfunc != SIG_DFL) {
            /* Block handled signals except during select, when
             * we permit them. */
            sigaddset(&signals_masked, signum);
            sigdelset(&signals_unmasked, signum);
        } else {
            /* Never block ignored/defaulted signals. */
            sigdelset(&signals_masked, signum);
            sigdelset(&signals_unmasked, signum);
        }
        xsignal_mask();
    }
}

sig_handler_type
xsignal(int signum, sig_handler_type sigfunc)
{
    struct sigaction act, oact;

    act.sa_handler = sigfunc;
    sigemptyset(&act.sa_mask);

    /* Try to restart system calls if possible. */
#    ifdef SA_RESTART
    act.sa_flags = SA_RESTART;
#    else
    act.sa_flags = 0;
#    endif

    if (sigaction(signum, &act, &oact) < 0)
        return SIG_ERR;
    set_signal_handled(signum, sigfunc);
    return oact.sa_handler;
}

sig_handler_type
xsignal_norestart(int signum, sig_handler_type sigfunc)
{
    struct sigaction act, oact;

    act.sa_handler = sigfunc;
    sigemptyset(&act.sa_mask);

    /* Try not to restart system calls. */
#    ifdef SA_INTERRUPT
    act.sa_flags = SA_INTERRUPT;
#    else
    act.sa_flags = 0;
#    endif

    if (sigaction(signum, &act, &oact) < 0)
        return SIG_ERR;
    set_signal_handled(signum, sigfunc);
    return oact.sa_handler;
}

/* Mask (block) all handled signals. */
void
xsignal_mask(void)
{
    int save_errno = errno;
    sigprocmask(SIG_SETMASK, &signals_masked, NULL);
    errno = save_errno;
}

/* Unmask (unblock) all handled signals. */
void
xsignal_unmask(void)
{
    int save_errno = errno;
    sigprocmask(SIG_SETMASK, &signals_unmasked, NULL);
    errno = save_errno;
}

/* Enable signal masking.
 *
 * The purpose of this is to ensure that signal handlers only run when
 * nothing else is going on, i.e. so they never access any data
 * structure while it is in an inconsistent state.
 */
void
xsignal_enable_masking(void)
{
    sigprocmask(SIG_SETMASK, NULL, &signals_masked);
    sigprocmask(SIG_SETMASK, NULL, &signals_unmasked);
    signal_masking = true;
}

/* Clean up signal mask for a forked process. */
void
xsignal_forked(void)
{
    if (signal_masking) {
        int n;

        /* Remove handlers for signals we handled.  The reason for this is that
         * the child process could in principle receive one of these signals
         * after the mask is released.  If this happens, we don't want the
         * handler to be run (even, or perhaps especially, inside the child) -
         * chaos would ensue.
         */
        for (n = 0; n < signal_max; ++n) {
            if (sigismember(&signals_masked, n)
                && !sigismember(&signals_unmasked, n)) {
                signal(n, SIG_DFL);
            }
        }
        /* Now it's OK to unblock signals we handled. */
        xsignal_unmask();
    }
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

/* Obsolete systems just have to put up with unsafe signal behaviour.
 * Sorry.
 */
void
xsignal_mask(void)
{
}

void
xsignal_unmask(void)
{
}

void
xsignal_enable_masking(void)
{
}

void
xsignal_forked(void)
{
}

#endif /* !HAVE_SIGACTION */
