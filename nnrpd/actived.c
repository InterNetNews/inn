/*  $Revision$
**
**  Active file server for readers (NNRP) for InterNetNews.
*/

#include	<stdio.h>
#include <sys/types.h>
#ifdef HAVE_WAIT_H
# include <wait.h>
#else
# include <sys/wait.h>
#endif
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#define MAINLINE
#include "nnrpd.h"
#include <netdb.h>
#include <pwd.h>
#if     defined(HPUX)
#include <sys/pstat.h>
#endif  /* defined(HPUX) */
#include	"protocol.h"

#define		MAINLINE

char	*ACTIVE = NULL;
char    *NNRPACCESS = NULL;
char    *HISTORY = NULL;
char    *ACTIVETIMES = NULL;
char    NOACCESS[] = NNTP_ACCESS;
char    *NEWSGROUPS = NULL;

void
#if     defined(VAR_VARARGS)
Reply(va_alist)
    va_dcl
#endif  /* defined(VAR_VARARGS) */
#if     defined(VAR_STDARGS)
Reply(char *fmt, ...)
#endif  /* defined(VAR_STDARGS) */
{
}

BOOL
PERMinfile(hp, ip, user, pass, accesslist, accessfile)
    char                *hp;
    char                *ip;
    char                *user;
    char                *pass;
    char                *accesslist;
    char                *accessfile;
{
}

NORETURN
ExitWithStats(x)
    int                 x;
{
}

int
handle(s)
    int			s;
{
    GROUPENTRY *gp;
    struct sockaddr saddr;
    struct sockaddr_in *sin = (struct sockaddr_in *)&saddr;
    int saddrsiz = sizeof(saddr);
    struct wireprotocol buffer;
    int len = sizeof(buffer);
    int rval;
    char	*st;
    int		i;

    if ((rval = recvfrom(s, (char *)&buffer, len, 0, &saddr, &saddrsiz)) < 0) {
	syslog(L_ERROR, "cant recvfrom %m");
	return(-1);
    }

    if (rval != sizeof(buffer)) {
	syslog(L_ERROR, "message size wrong");
	return(-1);
    }

    /* XXX If not 127.0.0.1, then reject and return */


    switch (buffer.RequestType) {
	case 	REQ_AYT:
	    buffer.RequestType = REQ_AYTACK;
	    if (sendto(s, (char *)&buffer, len, 0, &saddr, saddrsiz) < 0) {
		syslog(L_ERROR, "cant sendto %m");
		return(-1);
	    }
	    return(0);
	case 	REQ_FIND:
	    buffer.RequestType = REQ_FINDRESP;

	    buffer.Name[sizeof(buffer.Name) - 1] = '\0';
	    gp = GRPlocalfind(buffer.Name);
	    if (gp == NULL) {
		buffer.Success = 0;
	    } else {
		buffer.Success = 1;
		st = GPNAME(gp);
		if (st) {
		    i = strlen(st);
		    buffer.NameNull = 0;
		    strncpy(buffer.Name, st, i);
		    buffer.Name[i] = '\0';
		} else {
		    buffer.NameNull = 1;
		}
		buffer.High = (long)GPHIGH(gp);
		buffer.Low = (long)GPLOW(gp);
		buffer.Flag = GPFLAG(gp);
		st = GPALIAS(gp);
		if (st) {
		    i = strlen(st);
		    buffer.AliasNull = 0;
		    strncpy(buffer.Alias, st, i);
		    buffer.Alias[i] = '\0';
		} else {
		    buffer.AliasNull = 1;
		}
	    }
	    if (sendto(s, (char *)&buffer, len, 0, &saddr, saddrsiz) < 0) {
		syslog(L_ERROR, "cant sendto %m");
		return(-1);
	    }
	    return(0);
    }
    syslog(L_ERROR, "unknown requesttype %d", buffer.RequestType);
    return(-1);
}





/* ARGSUSED0 */
int
main(argc, argv)
    int			argc;
    char		*argv[];
{
    int s, i;
    time_t last_active_update = time(NULL), now;
    fd_set fdset;
    struct timeval tv;
    int daemonmode = 1;
    FILE *pidfile;

    openlog("actived", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if (ReadInnConf() < 0) { exit(1); }

    ACTIVE = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    innconf->activedenable = 0;		/* This is the daemon */

    if ((s = create_udp_socket(innconf->activedport, 0)) < 0) {
	syslog(L_ERROR, "cant createudpsocket %m");
	exit(1);
    }
    if (fcntl(s, F_SETFL, O_NDELAY) < 0) {
	syslog(L_ERROR, "cant fcntl O_NDELAY socket %m");
	exit(1);
    }
    if (!GetLocalGroupList()) {
	/* This shouldn't really happen. */
	syslog(L_ERROR, "cant getgrouplist %m");
	exit(1);
    }
    if (daemonmode) i = fork();
    else i = 0;
    if (i < 0)
        syslog(L_ERROR, "daemon: cannot fork");
    if (i != 0)
        exit(0);
    if ((pidfile = fopen(cpcatpath(innconf->pathrun, "actived.pid"),
						"w")) == NULL) {
	syslog(L_ERROR, "cannot write actived.pid %m");
	exit(1);
    }
    fprintf(pidfile,"%d", getpid());
    fclose(pidfile);
    syslog(L_NOTICE,"started");
    for (;;) {
	FD_ZERO(&fdset);
	FD_SET(s, &fdset);
	tv.tv_sec = 3;
	tv.tv_usec = 300000;
	if (select(s + 1, &fdset, NULL, NULL, &tv) < 0) {
	    syslog(L_ERROR, "cant select %m");
	    exit(1);
	}
	now = time(NULL);
	if (now > last_active_update + innconf->activedupdate) {
	    last_active_update = now;
	    if (!GetGroupList()) {
		/* This shouldn't really happen. */
		syslog(L_ERROR, "cant getgrouplist %m");
		exit(1);
	    }
	}
	if (FD_ISSET(s, &fdset)) {
		handle(s);
		loads(0, 1);
	}
    }
    /* NOTREACHED */
}
