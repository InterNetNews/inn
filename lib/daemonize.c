/*  $Id$
**
**  Become a long-running daemon.
**
**  Usage:
**
**      daemonize(path);
**
**  Performs all of the various system-specific stuff required to become a
**  long-running daemon.  Also chdir to the provided path (which is where
**  core dumps will go on most systems).
*/

#include "config.h"
#include "clibrary.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include "inn/messages.h"
#include "inn/libinn.h"

void
daemonize(const char *path)
{
    int status;
    int fd;

    /* Fork and exit in the parent to disassociate from the current process
       group and become the leader of a new process group. */
    status = fork();
    if (status < 0)
        sysdie("cant fork");
    else if (status > 0)
        _exit(0);

    /* setsid() should take care of disassociating from the controlling
       terminal, and FreeBSD at least doesn't like TIOCNOTTY if you don't
       already have a controlling terminal.  So only use the older TIOCNOTTY
       method if setsid() isn't available. */
#if HAVE_SETSID
    if (setsid() < 0)
        syswarn("cant become session leader");
#elif defined(TIOCNOTTY)
    fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        if (ioctl(fd, TIOCNOTTY, NULL) < 0)
            syswarn("cant disassociate from the terminal");
        close(fd);
    }
#endif /* defined(TIOCNOTTY) */

    if (chdir(path) < 0)
        syswarn("cant chdir to %s", path);

    fd = open("/dev/null", O_RDWR, 0);
    if (fd != -1) {
	dup2(fd, STDIN_FILENO);
	dup2(fd, STDOUT_FILENO);
	dup2(fd, STDERR_FILENO);
	if (fd > 2)
	    close(fd);
    }
}
