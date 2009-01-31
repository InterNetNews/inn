/*  $Id$
**
**  Routines for the local connect channel.  Create a Unix-domain stream
**  socket that processes on the local server connect to.  Once the
**  connection is set up, we speak NNTP.  The connect channel is used only
**  by rnews to feed in articles from the UUCP sites.
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "innd.h"


#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>

static char	*LCpath = NULL;
static CHANNEL	*LCchan;


/*
**  Read function.  Accept the connection and create an NNTP channel.
*/
static void
LCreader(CHANNEL *cp)
{
    int		fd;
    CHANNEL	*new;

    if (cp != LCchan) {
	syslog(L_ERROR, "%s internal LCreader wrong channel 0x%p not 0x%p",
	    LogName, (void *)cp, (void *)LCchan);
	return;
    }

    if ((fd = accept(cp->fd, NULL, NULL)) < 0) {
	syslog(L_ERROR, "%s cant accept CCreader %m", LogName);
	return;
    }
    if ((new = NCcreate(fd, false, true)) != NULL) {
	memset( &new->Address, 0, sizeof( new->Address ) );
	syslog(L_NOTICE, "%s connected %d", "localhost", new->fd);
	NCwritereply(new, (char *)NCgreeting);
    }
}


/*
**  Write-done function.  Shouldn't happen.
*/
static void
LCwritedone(CHANNEL *unused)
{
    unused = unused;		/* ARGSUSED */
    syslog(L_ERROR, "%s internal LCwritedone", LogName);
}

#endif /* HAVE_UNIX_DOMAIN_SOCKETS */


/*
**  Create the channel.
*/
void
LCsetup(void)
{
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    int			i;
    struct sockaddr_un	server;

    if (LCpath == NULL)
	LCpath = concatpath(innconf->pathrun, INN_PATH_NNTPCONNECT);
    /* Remove old detritus. */
    if (unlink(LCpath) < 0 && errno != ENOENT) {
	syslog(L_FATAL, "%s cant unlink %s %m", LogName, LCpath);
	exit(1);
    }

    /* Create a socket and name it. */
    if ((i = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
	syslog(L_FATAL, "%s cant socket %s %m", LogName, LCpath);
	exit(1);
    }
    memset(&server, 0, sizeof server);
    server.sun_family = AF_UNIX;
    strlcpy(server.sun_path, LCpath, sizeof(server.sun_path));
    if (bind(i, (struct sockaddr *) &server, SUN_LEN(&server)) < 0) {
	syslog(L_FATAL, "%s cant bind %s %m", LogName, LCpath);
	exit(1);
    }

    /* Set it up to wait for connections. */
    if (listen(i, MAXLISTEN) < 0) {
	syslog(L_FATAL, "%s cant listen %s %m", LogName, LCpath);
	exit(1);
    }
    LCchan = CHANcreate(i, CTlocalconn, CSwaiting, LCreader, LCwritedone);
    syslog(L_NOTICE, "%s lcsetup %s", LogName, CHANname(LCchan));
    RCHANadd(LCchan);
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
}


/*
**  Cleanly shut down the channel.
*/
void
LCclose(void)
{
#if	defined(HAVE_UNIX_DOMAIN_SOCKETS)
    CHANclose(LCchan, CHANname(LCchan));
    LCchan = NULL;
    if (unlink(LCpath) < 0)
	syslog(L_ERROR, "%s cant unlink %s %m", LogName, LCpath);
    free(LCpath);
#endif	/* defined(HAVE_UNIX_DOMAIN_SOCKETS) */
}
