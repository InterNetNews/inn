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
#include "libinn.h"

void
daemonize(const char *path)
{
    int status;
    int fd UNUSED;

    /* Fork and exit in the parent to disassociate from the current process
       group and become the leader of a new process group. */
    status = fork();
    if (status < 0)
        sysdie("cant fork");
    else if (status > 0)
        _exit(0);

#ifdef TIOCNOTTY
    fd = open("/dev/tty", O_RDWR);
    if (fd >= 0) {
        if (ioctl(fd, TIOCNOTTY, NULL) < 0)
            syswarn("cant disassociate from the terminal");
        close(fd);
    }
#endif /* TIOCNOTTY */

#if HAVE_SETSID
    setsid();
#endif

    if (chdir(path) < 0)
        syswarn("cant chdir to %s", path);
}
