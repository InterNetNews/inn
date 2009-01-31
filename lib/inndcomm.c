/*  $Id$
**
**  Library routines to let other programs control innd.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/time.h"
#include "portable/socket.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
# include <sys/un.h>
#endif

#include "inn/innconf.h"
#include "inn/inndcomm.h"
#include "inn/libinn.h"
#include "inn/paths.h"

static char			*ICCsockname = NULL;
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
static struct sockaddr_un	ICCserv;
static struct sockaddr_un	ICCclient;
#endif
static int			ICCfd;
static int			ICCtimeout;
const char			*ICCfailure;


/*
**  Set the timeout.
*/
void
ICCsettimeout(int i)
{
    ICCtimeout = i;
}


/*
**  Get ready to talk to the server.
*/
int
ICCopen(void)
{
    int mask, oerrno, fd;
#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    int size = 65535;
#endif

    if (innconf == NULL) {
	if (!innconf_read(NULL)) {
	    ICCfailure = "innconf";
	    return -1;
	}
    }
    /* Create a temporary name.  mkstemp is complete overkill here and is used
       only because it's convenient.  We don't use it properly, since we
       actually need to create a socket or named pipe, so there is a race
       condition here.  It doesn't matter, since pathrun isn't world-writable
       (conceivably two processes could end up using the same temporary name
       at the same time, but the worst that will happen is that one process
       will delete the other's temporary socket). */
    if (ICCsockname == NULL)
	ICCsockname = concatpath(innconf->pathrun, INN_PATH_TEMPSOCK);
    fd = mkstemp(ICCsockname);
    if (fd < 0) {
        ICCfailure = "mkstemp";
        return -1;
    }
    close(fd);
    if (unlink(ICCsockname) < 0 && errno != ENOENT) {
	ICCfailure = "unlink";
	return -1;
    }

#ifdef HAVE_UNIX_DOMAIN_SOCKETS

    /* Make a socket and give it the name. */
    if ((ICCfd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
	ICCfailure = "socket";
	return -1;
    }

    /* Adjust the socket buffer size to accomodate large responses.  Ignore
       failure; the message may fit anyway, and if not, we'll fail below. */
    setsockopt(ICCfd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));

    memset(&ICCclient, 0, sizeof ICCclient);
    ICCclient.sun_family = AF_UNIX;
    strlcpy(ICCclient.sun_path, ICCsockname, sizeof(ICCclient.sun_path));
    mask = umask(0);
    if (bind(ICCfd, (struct sockaddr *) &ICCclient, SUN_LEN(&ICCclient)) < 0) {
	oerrno = errno;
	umask(mask);
	errno = oerrno;
	ICCfailure = "bind";
	return -1;
    }
    umask(mask);

    /* Name the server's socket. */
    memset(&ICCserv, 0, sizeof ICCserv);
    ICCserv.sun_family = AF_UNIX;
    strlcpy(ICCserv.sun_path, innconf->pathrun, sizeof(ICCserv.sun_path));
    strlcat(ICCserv.sun_path, "/", sizeof(ICCserv.sun_path));
    strlcat(ICCserv.sun_path, INN_PATH_NEWSCONTROL, sizeof(ICCserv.sun_path));

#else /* !HAVE_UNIX_DOMAIN_SOCKETS */

    /* Make a named pipe and open it. */
    mask = umask(0);
    if (mkfifo(ICCsockname, 0666) < 0) {
	oerrno = errno;
	umask(mask);
	errno = oerrno;
	ICCfailure = "mkfifo";
	return -1;
    }
    umask(mask);
    if ((ICCfd = open(ICCsockname, O_RDWR)) < 0) {
	ICCfailure = "open";
	return -1;
    }
#endif /* !HAVE_UNIX_DOMAIN_SOCKETS */

    ICCfailure = NULL;
    return 0;
}


/*
**  Close down.
*/
int
ICCclose(void)
{
    int		i;

    ICCfailure = NULL;
    i = 0;
    if (close(ICCfd) < 0) {
	ICCfailure = "close";
	i = -1;
    }
    if (unlink(ICCsockname) < 0 && errno != ENOENT) {
	ICCfailure = "unlink";
	i = -1;
    }
    return i;
}


/*
**  Get the server's pid.
*/
static pid_t
ICCserverpid(void)
{
    pid_t		pid;
    FILE		*F;
    char                *path;
    char		buff[SMBUF];

    pid = 1;
    path = concatpath(innconf->pathrun, INN_PATH_SERVERPID);
    F = fopen(path, "r");
    free(path);
    if (F != NULL) {
	if (fgets(buff, sizeof buff, F) != NULL)
	    pid = atol(buff);
	fclose(F);
    }
    return pid;
}


/*
**  See if the server is still there.  When in doubt, assume yes.  Cache the
**  PID since a rebooted server won't know about our pending message.
*/
static bool
ICCserveralive(pid_t pid)
{
    if (kill(pid, 0) > 0 || errno != ESRCH)
	return true;
    return false;
}


/*
**  Send an arbitrary command to the server.
**  
**  There is a protocol version (one-byte) on the front of the message,
**  followed by a two byte length count.  The length includes the protocol
**  byte and the length itself.  This differs from the protocol in much
**  earlier versions of INN.
*/
int
ICCcommand(char cmd, const char *argv[], char **replyp)
{
    char		*buff;
    char		*p;
    const char		*q;
    char		save;
    int			i ;
#ifndef HAVE_UNIX_DOMAIN_SOCKETS
    int			fd;
    char                *path;
#endif
    int			len;
    fd_set		Rmask;
    struct timeval	T;
    pid_t		pid;
    ICC_MSGLENTYPE      rlen;
    ICC_PROTOCOLTYPE    protocol;
    size_t bufsiz = 64 * 1024 - 1;

    /* Is server there? */
    pid = ICCserverpid();
    if (!ICCserveralive(pid)) {
	ICCfailure = "dead server";
	return -1;
    }

    /* Get the length of the buffer. */
    buff = xmalloc(bufsiz);
    if (replyp)
	*replyp = NULL;

    /* Advance to leave space for length + protocol version info. */
    buff += HEADER_SIZE;
    bufsiz -= HEADER_SIZE;

    /* Format the message. */
    snprintf(buff, bufsiz, "%s%c%c", ICCsockname, SC_SEP, cmd);
    for (p = buff + strlen(buff), i = 0; (q = argv[i]) != NULL; i++) {
	*p++ = SC_SEP;
        *p = '\0';
        strlcat(buff, q, bufsiz);
        p += strlen(q);
    }

    /* Send message. */
    ICCfailure = NULL;
    len = p - buff + HEADER_SIZE;
    rlen = htons(len);

    /* now stick in the protocol version and the length. */
    buff -= HEADER_SIZE;
    bufsiz += HEADER_SIZE;
    protocol = ICC_PROTOCOL_1;
    memcpy(buff, &protocol, sizeof(protocol));
    memcpy(buff + sizeof(protocol), &rlen, sizeof(rlen));

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    if (sendto(ICCfd, buff, len, 0,(struct sockaddr *) &ICCserv,
               SUN_LEN(&ICCserv)) < 0) {
	free(buff);
	ICCfailure = "sendto";
	return -1;
    }
#else /* !HAVE_UNIX_DOMAIN_SOCKETS */
    path = concatpath(innconf->pathrun, INN_PATH_NEWSCONTROL);
    fd = open(path, O_WRONLY);
    free(path);
    if (fd < 0) {
	free(buff);
	ICCfailure = "open";
	return -1;
    }
    if (write(fd, buff, len) != len) {
	i = errno;
	free(buff);
	close(fd);
	errno = i;
	ICCfailure = "write";
	return -1;
    }
    close(fd);
#endif /* !HAVE_UNIX_DOMAIN_SOCKETS */

    /* Possibly get a reply. */
    switch (cmd) {
    default:
	if (ICCtimeout >= 0)
	    break;
	/* FALLTHROUGH */
    case SC_SHUTDOWN:
    case SC_XABORT:
    case SC_XEXEC:
	free(buff);
	return 0;
    }

    /* Wait for the reply. */
    for ( ; ; ) {
	FD_ZERO(&Rmask);
	FD_SET(ICCfd, &Rmask);
	T.tv_sec = ICCtimeout ? ICCtimeout : 120;
	T.tv_usec = 0;
	i = select(ICCfd + 1, &Rmask, NULL, NULL, &T);
	if (i < 0) {
	    free(buff);
	    ICCfailure = "select";
	    return -1;
	}
	if (i > 0 && FD_ISSET(ICCfd, &Rmask))
	    /* Server reply is there; go handle it. */
	    break;

	/* No data -- if we timed out, return. */
	if (ICCtimeout) {
	    free(buff);
	    errno = ETIMEDOUT;
	    ICCfailure = "timeout";
	    return -1;
	}

	if (!ICCserveralive(pid)) {
	    free(buff);
	    ICCfailure = "dead server";
	    return -1;
	}
    }

#ifdef HAVE_UNIX_DOMAIN_SOCKETS
    
    /* Read the reply. */
    i = RECVorREAD(ICCfd, buff, bufsiz);
    if ((unsigned int) i < HEADER_SIZE) {
        free(buff);
        ICCfailure = "read";
        return -1;
    }
    memcpy(&protocol, buff, sizeof(protocol));
    memcpy(&rlen, buff + sizeof(protocol), sizeof(rlen));
    rlen = ntohs(rlen);

    if (i != rlen) {
        free(buff);
        ICCfailure = "short read";
        return -1;
    }

    if (protocol != ICC_PROTOCOL_1) {
        free(buff);
        ICCfailure = "protocol mismatch";
        return -1;
    }

    memmove(buff, buff + HEADER_SIZE, rlen - HEADER_SIZE);
    i -= HEADER_SIZE;

    buff[i] = '\0';

#else /* !HAVE_UNIX_DOMAIN_SOCKETS */

    i = RECVorREAD(ICCfd, buff, HEADER_SIZE);
    if (i != HEADER_SIZE)
	return -1;

    memcpy(&protocol, buff, sizeof(protocol));
    memcpy(&rlen, buff + sizeof(protocol), sizeof(rlen));
    rlen = ntohs(rlen) - HEADER_SIZE;
    if (rlen > bufsiz) {
        ICCfailure = "bad length";
        return -1;
    }
    
    i = RECVorREAD(ICCfd, buff, rlen);
    if (i != rlen) {
        ICCfailure = "short read";
	return -1;
    }

    buff[i] = '\0';

    if (protocol != ICC_PROTOCOL_1) {
        ICCfailure = "protocol mismatch";
        return -1;
    }

#endif /* !HAVE_UNIX_DOMAIN_SOCKETS */
    
    /* Parse the rest of the reply; expected to be like
       <exitcode><space><text>" */
    i = 0;
    if (CTYPE(isdigit, buff[0])) {
	for (p = buff; *p && CTYPE(isdigit, *p); p++)
	    continue;
	if (*p) {
	    save = *p;
	    *p = '\0';
	    i = atoi(buff);
	    *p = save;
	}
    }
    if (replyp)
	*replyp = buff;
    else
	free(buff);

    return i;
}


/*
**  Send a "cancel" command.
*/
int
ICCcancel(const char *msgid)
{
    const char	*args[2];

    args[0] = msgid;
    args[1] = NULL;
    return ICCcommand(SC_CANCEL, args, NULL);
}


/*
**  Send a "go" command.
*/
int
ICCgo(const char *why)
{
    const char	*args[2];

    args[0] = why;
    args[1] = NULL;
    return ICCcommand(SC_GO, args, NULL);
}


/*
**  Send a "pause" command.
*/
int
ICCpause(const char *why)
{
    const char	*args[2];

    args[0] = why;
    args[1] = NULL;
    return ICCcommand(SC_PAUSE, args, NULL);
}


/*
**  Send a "reserve" command.
*/
int
ICCreserve(const char *why)
{
    const char	*args[2];

    args[0] = why;
    args[1] = NULL;
    return ICCcommand(SC_RESERVE, args, NULL);
}
