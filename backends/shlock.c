/*  $Id$
**
**  Produce reliable locks for shell scripts, by Peter Honeyman as told
**  to Rich $alz.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#include "inn/messages.h"


static bool	BinaryLock;


/*
**  See if the process named in an existing lock still exists by
**  sending it a null signal.
*/
static bool
ValidLock(char *name, bool JustChecking)
{
    int	fd;
    int	i;
    pid_t		pid;
    char		buff[BUFSIZ];

    /* Open the file. */
    if ((fd = open(name, O_RDONLY)) < 0) {
	if (JustChecking)
	    return FALSE;
        syswarn("cannot open %s", name);
	return TRUE;
    }

    /* Read the PID that is written there. */
    if (BinaryLock) {
	if (read(fd, (char *)&pid, sizeof pid) != sizeof pid) {
	    (void)close(fd);
	    return FALSE;
	}
    }
    else {
	if ((i = read(fd, buff, sizeof buff - 1)) <= 0) {
	    (void)close(fd);
	    return FALSE;
	}
	buff[i] = '\0';
	pid = (pid_t) atol(buff);
    }
    (void)close(fd);
    if (pid <= 0)
	return FALSE;

    /* Send the signal. */
    if (kill(pid, 0) < 0 && errno == ESRCH)
	return FALSE;

    /* Either the kill worked, or we're optimistic about the error code. */
    return TRUE;
}


/*
**  Unlink a file, print a message on error, and exit.
*/
static void
UnlinkAndExit(char *name, int x)
{
    if (unlink(name) < 0)
        syswarn("cannot unlink %s", name);
    exit(x);
}


/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage: shlock [-u|-b] -f file -p pid\n");
    exit(1);
}


int
main(int ac, char *av[])
{
    int	i;
    char	*p;
    int	fd;
    char		tmp[BUFSIZ];
    char		buff[BUFSIZ];
    char		*name;
    pid_t		pid;
    bool		ok;
    bool		JustChecking;

    /* Establish our identity. */
    message_program_name = "shlock";

    /* Set defaults. */
    pid = 0;
    name = NULL;
    JustChecking = FALSE;
    (void)umask(NEWSUMASK);

    /* Parse JCL. */
    while ((i = getopt(ac, av, "bcup:f:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'b':
	case 'u':
	    BinaryLock = TRUE;
	    break;
	case 'c':
	    JustChecking = TRUE;
	    break;
	case 'p':
	    pid = (pid_t) atol(optarg);
	    break;
	case 'f':
	    name = optarg;
	    break;
	}
    ac -= optind;
    av += optind;
    if (ac || pid == 0 || name == NULL)
	Usage();

    /* Create the temp file in the same directory as the destination. */
    if ((p = strrchr(name, '/')) != NULL) {
	*p = '\0';
	snprintf(tmp, sizeof(tmp), "%s/shlock%ld", name, (long)getpid());
	*p = '/';
    }
    else
	snprintf(tmp, sizeof(tmp), "shlock%ld", (long)getpid());

    /* Loop until we can open the file. */
    while ((fd = open(tmp, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0)
	switch (errno) {
	default:
	    /* Unknown error -- give up. */
            sysdie("cannot open %s", tmp);
	case EEXIST:
	    /* If we can remove the old temporary, retry the open. */
	    if (unlink(tmp) < 0)
                sysdie("cannot unlink %s", tmp);
	    break;
	}

    /* Write the process ID. */
    if (BinaryLock)
	ok = write(fd, &pid, sizeof pid) == sizeof pid;
    else {
	snprintf(buff, sizeof(buff), "%ld\n", (long) pid);
	i = strlen(buff);
	ok = write(fd, buff, i) == i;
    }
    if (!ok) {
        syswarn("cannot write PID to %s", tmp);
	(void)close(fd);
	UnlinkAndExit(tmp, 1);
    }

    (void)close(fd);

    /* Handle the "-c" flag. */
    if (JustChecking) {
	if (ValidLock(name, TRUE))
	    UnlinkAndExit(tmp, 1);
	UnlinkAndExit(tmp, 0);
    }

    /* Try to link the temporary to the lockfile. */
    while (link(tmp, name) < 0)
	switch (errno) {
	default:
	    /* Unknown error -- give up. */
            syswarn("cannot link %s to %s", tmp, name);
	    UnlinkAndExit(tmp, 1);
	    /* NOTREACHED */
	case EEXIST:
	    /* File exists; if lock is valid, give up. */
	    if (ValidLock(name, FALSE))
		UnlinkAndExit(tmp, 1);
	    if (unlink(name) < 0) {
                syswarn("cannot unlink %s", name);
		UnlinkAndExit(tmp, 1);
	    }
	}

    UnlinkAndExit(tmp, 0);
    /* NOTREACHED */
    return 1;
}
