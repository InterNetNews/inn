/*  $Id$
**
**  Portable non-blocking wait for a child process to exit.
**
**  Prefer waitpid, fall back on wait3.  Per autoconf advice, define our own
**  versions of the wait macros used if they aren't defined, even if
**  HAVE_SYS_WAIT_H isn't defined.  Never use the union wait code.
*/
#include "config.h"
#include <sys/types.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifndef HAVE_WAITPID
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# endif
# include <sys/resource.h>
#endif

#ifndef WEXITSTATUS
# define WEXITSTATUS(status)    ((((unsigned)(status)) >> 8) & 0xFF)
#endif

pid_t
waitnb(int *statusp)
{
    int         status;
    pid_t       pid;

#ifdef HAVE_WAITPID
    pid = waitpid(-1, &status, WNOHANG);
#else
    pid = wait3(&status, WNOHANG, (struct rusage *) 0);
#endif

    if (statusp && pid > 0)
        *statusp = WEXITSTATUS(status);
    return pid;
}
