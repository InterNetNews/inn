/*  $Revision$
**
**  NNTP server for readers (NNRP) for InterNetNews.
**  This server doesn't do any real load-limiting, except for what has
**  proven empirically necesary (i.e., look at GRPscandir).
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include "libinn.h"
#include "ov.h"
#define MAINLINE
#include "nnrpd.h"
#include <netdb.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#if	defined(_HPUX_SOURCE)
#include <sys/pstat.h>
#endif	/* defined(_HPUX_SOURCE) */
#if HAVE_GETSPNAM
#  include <shadow.h>
#endif /* HAVE_GETSPNAM */

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include "tls.h"
extern SSL *tls_conn;
int nnrpd_starttls_done = 0;
#endif 

#if defined(hpux) || defined(__hpux) || defined(_SCO_DS)
extern int h_errno;
#endif

/*
** Here is some defensive code to protect the news server from hosts,
** mostly PC's, that sometimes make a connection and then never give
** any commands.  The connection is abandoned, but we're never told
** about it.  The first time the connection is read, it will have a
** timeout value of INITIAL_TIMEOUT seconds.  All subsequent reads
** will have the standard timeout of CLIENT_TIMEOUT seconds.
*/
#if !defined(INITIAL_TIMEOUT)
#define INITIAL_TIMEOUT	10
#endif

#define MAXPATTERNDEFINE	10

#define CMDany		-1


typedef struct _CMDENT {
    STRING	Name;
    FUNCPTR	Function;
    BOOL	Needauth;
    int		Minac;
    int		Maxac;
    STRING	Help;
} CMDENT;


char	NOACCESS[] = NNTP_ACCESS;
char	*ACTIVE = NULL;
char	*ACTIVETIMES = NULL;
char	*HISTORY = NULL;
char	*NEWSGROUPS = NULL;
char	*NNRPACCESS = NULL;

BOOL	ForceReadOnly = FALSE;
STATIC	char 	*LocalLogFileName = NULL;
STATIC	char 	*LocalLogDirName;

STATIC double	STATstart;
STATIC double	STATfinish;
STATIC char	*PushedBack;
#if	!defined(_HPUX_SOURCE)
STATIC char	*TITLEstart;
STATIC char	*TITLEend;
#endif	/* !defined(_HPUX_SOURCE) */
STATIC sig_atomic_t	ChangeTrace;
BOOL	DaemonMode = FALSE;
#if HAVE_GETSPNAM
STATIC char	*ShadowGroup;
#endif
#if	defined(DO_NNRP_GETHOSTBYADDR)
STATIC char 	*HostErrorStr;
#endif	/* defined(DO_NNRP_GETHOSTBYADDR) */

extern FUNCTYPE	CMDauthinfo();
extern FUNCTYPE	CMDdate();
extern FUNCTYPE	CMDfetch();
extern FUNCTYPE	CMDgroup();
STATIC FUNCTYPE	CMDhelp();
extern FUNCTYPE	CMDlist();
extern FUNCTYPE	CMDmode();
extern FUNCTYPE	CMDnewgroups();
extern FUNCTYPE	CMDnewnews();
extern FUNCTYPE	CMDnextlast();
extern FUNCTYPE	CMDpost();
extern FUNCTYPE	CMDxgtitle();
extern FUNCTYPE	CMDxhdr();
extern FUNCTYPE	CMDxover();
extern FUNCTYPE	CMDxpat();
extern FUNCTYPE	CMDxpath();
extern FUNCTYPE	CMD_unimp();
#ifdef HAVE_SSL
extern FUNCTYPE	CMDstarttls();
#endif

BOOL LLOGenable;
extern int TrackClient();

STATIC char	CMDfetchhelp[] = "[MessageID|Number]";

STATIC CMDENT	CMDtable[] = {
    {	"authinfo",	CMDauthinfo,	FALSE,	3,	CMDany,
	"user Name|pass Password|generic <prog> <args>" },
#ifdef HAVE_SSL
    {	"starttls",	CMDstarttls,	FALSE,	1,	1,
	NULL },
#endif
    {	"article",	CMDfetch,	TRUE,	1,	2,
	CMDfetchhelp },
    {	"body",		CMDfetch,	TRUE,	1,	2,
	CMDfetchhelp },
    {	"date",		CMDdate,	FALSE,	1,	1,
	NULL },
    {	"group",	CMDgroup,	TRUE,	2,	2,
	"newsgroup" },
    {	"head",		CMDfetch,	TRUE,	1,	2,
	CMDfetchhelp },
    {	"help",		CMDhelp,	FALSE,	1,	CMDany,
	NULL },
    {	"ihave",	CMD_unimp,	TRUE,	1,	2,
	NULL },
    {	"last",		CMDnextlast,	TRUE,	1,	1,
	NULL },
    {	"list",		CMDlist,	TRUE,	1,	3,
	"[active|active.times|newsgroups|distributions|distrib.pats|overview.fmt|subscriptions|motd]" },
    {	"listgroup",	CMDgroup,	TRUE,	1,	2,
	"newsgroup" },
    {	"mode",		CMDmode,	FALSE,	2,	2,
	"reader" },
    {	"newgroups",	CMDnewgroups,	TRUE,	3,	5,
	"[YY]yymmdd hhmmss [\"GMT\"|\"UTC\"] [<distributions>]" },
    {	"newnews",	CMDnewnews,	TRUE,	4,	6,
	"newsgroups [YY]yymmdd hhmmss [\"GMT\"|\"UTC\"] [<distributions>]" },
    {	"next",		CMDnextlast,	TRUE,	1,	1,
	NULL },
    {	"post",		CMDpost,	TRUE,	1,	1,
	NULL },
    {	"slave",	CMD_unimp,	FALSE,	1,	1,
	NULL },
    {	"stat",		CMDfetch,	TRUE,	1,	2,
	CMDfetchhelp },
    {	"xgtitle",	CMDxgtitle,	TRUE,	1,	2,
	"[group_pattern]" },
    {	"xhdr",		CMDxhdr,	TRUE,	2,	3,
	"header [range|MessageID]" },
    {	"xover",	CMDxover,	TRUE,	1,	2,
	"[range]" },
    {	"xpat",		CMDxpat,	TRUE,	4,	CMDany,
	"header range|MessageID pat [morepat...]" },
    {	"xpath",	CMDxpath,	TRUE,	2,	2,
	"MessageID" },
    {	NULL }
};


/*
**  Log a summary status message and exit.
*/
NORETURN
ExitWithStats(int x, BOOL readconf)
{
    TIMEINFO		Now;
    double		usertime;
    double		systime;

    (void)fflush(stdout);
    (void)GetTimeInfo(&Now);
    STATfinish = TIMEINFOasDOUBLE(Now);
    if (GetResourceUsage(&usertime, &systime) < 0) {
	usertime = 0;
	systime = 0;
    }

    GRPreport();
    if (ARTcount)
        syslog(L_NOTICE, "%s exit articles %ld groups %ld", 
    	    ClientHost, ARTcount, GRPcount);
    if (POSTreceived ||  POSTrejected)
	syslog(L_NOTICE, "%s posts received %ld rejected %ld",
	   ClientHost, POSTreceived, POSTrejected);
    syslog(L_NOTICE, "%s times user %.3f system %.3f elapsed %.3f",
	ClientHost, usertime, systime, STATfinish - STATstart);
    /* Tracking code - Make entries in the logfile(s) to show that we have
	finished with this session */
    if (!readconf && PERMaccessconf &&  PERMaccessconf->readertrack) {
	syslog(L_NOTICE, "%s Tracking Disabled (%s)", ClientHost, Username);
	if (LLOGenable) {
		fprintf(locallog, "%s Tracking Disabled (%s)\n", ClientHost, Username);
		fclose(locallog);
		syslog(L_NOTICE,"%s Local Logging ends (%s) %s",ClientHost, Username, LocalLogFileName);
	}
    }
    if (ARTget)
        syslog(L_NOTICE, "%s artstats get %d time %d size %ld", ClientHost,
            ARTget, ARTgettime, ARTgetsize);
    if (!readconf && PERMaccessconf && PERMaccessconf->nnrpdoverstats && OVERcount)
        syslog(L_NOTICE, "%s overstats count %d hit %d miss %d time %d size %d dbz %d seek %d get %d artcheck %d", ClientHost,
            OVERcount, OVERhit, OVERmiss, OVERtime, OVERsize, OVERdbz, OVERseek, OVERget, OVERartcheck);

     if (DaemonMode) {
     	shutdown(STDIN, 2);
     	shutdown(STDOUT, 2);
     	shutdown(STDERR, 2);
 	close(STDIN);    	
 	close(STDOUT);
 	close(STDERR);    	
     }
    
    OVclose();

#ifdef DO_PYTHON
    if (innconf->nnrppythonauth) {
        if (PY_close() < 0) {
	    syslog(L_NOTICE, "PY_close(): close method not invoked because it is not defined in Python authenticaton module.");
	}
    }
#endif	/* DO_PYTHON */

    if (LocalLogFileName != NULL);
	DISPOSE(LocalLogFileName);

    exit(x);
}


/*
**  The "help" command.
*/
/* ARGSUSED0 */
STATIC FUNCTYPE
CMDhelp(int ac, char *av[])
{
    CMDENT	*cp;
    char	*p, *q;

    Reply("%s\r\n", NNTP_HELP_FOLLOWS);
    for (cp = CMDtable; cp->Name; cp++)
	if (cp->Help == NULL)
	    Printf("  %s\r\n", cp->Name);
	else
	    Printf("  %s %s\r\n", cp->Name, cp->Help);
    if (PERMaccessconf && (VirtualPathlen > 0)) {
	if (PERMaccessconf->newsmaster) {
	    if (strchr(PERMaccessconf->newsmaster, '@') == NULL) {
		Printf("Report problems to <%s@%s>\r\n",
		    PERMaccessconf->newsmaster, PERMaccessconf->domain);
	    } else {
		Printf("Report problems to <%s>\r\n",
		    PERMaccessconf->newsmaster);
	    }
	} else {
	    /* sigh, pickup from NEWSMASTER anyway */
	    if ((p = strchr(NEWSMASTER, '@')) == NULL)
		Printf("Report problems to <%s@%s>\r\n",
		    NEWSMASTER, PERMaccessconf->domain);
	    else {
		q = NEW(char, p - NEWSMASTER + 1);
		strncpy(q, NEWSMASTER, p - NEWSMASTER);
		q[p - NEWSMASTER] = '\0';
		Printf("Report problems to <%s@%s>\r\n",
		    q, PERMaccessconf->domain);
		DISPOSE(q);
	    }
	}
    } else {
	if (strchr(NEWSMASTER, '@') == NULL)
	    Printf("Report problems to <%s@%s>\r\n",
		NEWSMASTER, innconf->fromhost);
	else
	    Printf("Report problems to <%s>\r\n",
		NEWSMASTER);
    }
    Reply(".\r\n");
}


/*
**  Unimplemented catch-all.
*/
/* ARGSUSED0 */
FUNCTYPE
CMD_unimp(ac, av)
    int		ac;
    char	*av[];
{
    if (caseEQ(av[0], "ihave"))
	Reply("%d Transfer permission denied\r\n", NNTP_AUTH_NEEDED_VAL);
    else if (caseEQ(av[0], "slave"))
	/* Somebody sends us this?  I don't believe it! */
	Reply("%d Unsupported\r\n", NNTP_SLAVEOK_VAL);
    else
	Reply("%d %s not implemented; try help\r\n",
	    NNTP_BAD_COMMAND_VAL, av[0]);
}


/*
**  Overwrite the original argv so that ps will show what's going on.
*/
STATIC void
TITLEset(what)
    char		*what;
{
#if defined(HAVE_SETPROCTITLE)
    setproctitle("%s %s", ClientHost, what);
#else
#if	!defined(_HPUX_SOURCE)
    register char	*p;
    register int	i;
    char		buff[BUFSIZ];

    /* Make ps think we're swapped out so we get "(nnrpd)" in the output. */
    p = TITLEstart;
    *p++ = '-';

    (void)sprintf(buff, "%s %s", ClientHost, what);
    i = strlen(buff);
    if (i > TITLEend - p - 2) {
	i = TITLEend - p - 2;
	buff[i] = '\0';
    }
    (void)strcpy(p, buff);
    for (p += i; p < TITLEend; )
	*p++ = ' ';
#else
    char		buff[BUFSIZ];
    union pstun un;
    
    (void)sprintf(buff, "(nnrpd) %s %s", ClientHost, what);
    un.pst_command = buff;
    (void)pstat(PSTAT_SETCMD, un, strlen(buff), 0, 0);
#endif	/* !defined(_HPUX_SOURCE) */
#endif	/* defined(HAVE_SETPROCTITLE) */
}


#if	defined(DO_NNRP_GETHOSTBYADDR)
#ifndef	INADDR_LOOPBACK
#define	INADDR_LOOPBACK	0x7f000001
#endif	/* INADDR_LOOPBACK */
/*
**  Convert an IP address to a hostname.  Don't trust the reverse lookup,
**  since anyone can fake .in-addr.arpa entries.
*/
STATIC BOOL
Address2Name(INADDR *ap, char *hostname, int i)
{
    char		*p;
    struct hostent	*hp;
#if	defined(h_addr)
    char		**pp;
#endif

    /* Get the official hostname, store it away. */
    if ((hp = gethostbyaddr((char *)ap, sizeof *ap, AF_INET)) == NULL) {
	HostErrorStr = (char *)hstrerror(h_errno);
	return FALSE;
    }
    (void)strncpy(hostname, hp->h_name, i);
    hostname[i - 1] = '\0';

    /* Get addresses for this host. */
    if ((hp = gethostbyname(hostname)) == NULL) {
	HostErrorStr = (char *)hstrerror(h_errno);
	return FALSE;
    }

    /* Make sure one of those addresses is the address we got. */
#if	defined(h_addr)
    /* We have many addresses */
    for (pp = hp->h_addr_list; *pp; pp++)
	if (memcmp((POINTER)&ap->s_addr, (POINTER)*pp,
		(SIZE_T)hp->h_length) == 0)
	    break;
    if (*pp == NULL)
	return FALSE;
#else
    /* We have one address. */
    if (memcmp((POINTER)&ap->s_addr, (POINTER)hp->h_addr,
	    (SIZE_T)hp->h_length) != 0)
	return FALSE;
#endif

    /* Only needed for misconfigured YP/NIS systems. */
    if (ap->s_addr != INADDR_LOOPBACK && strchr(hostname, '.') == NULL
     && (p = innconf->domain) != NULL) {
	(void)strcat(hostname, ".");
	(void)strcat(hostname, p);
    }

    /* Make all lowercase, for wildmat. */
    for (p = hostname; *p; p++)
	if (CTYPE(isupper, (int)*p))
	    *p = tolower(*p);
    return TRUE;
}
#endif	/* defined(DO_NNRP_GETHOSTBYADDR) */


/*
**  Determine access rights of the client.
*/
STATIC void StartConnection()
{
    struct sockaddr_in	sin;
    ARGTYPE		length;
    char		buff[SMBUF];
    char		*ClientAddr;
    char		accesslist[BIG_BUFFER];
    int                 code;
    static ACCESSGROUP	*authconf;

    /* Get the peer's name. */
    length = sizeof sin;
    ClientAddr = NULL;
    if (getpeername(STDIN, (struct sockaddr *)&sin, &length) < 0) {
      if (!isatty(STDIN)) {
	    syslog(L_TRACE, "%s cant getpeername %m", "?");
            (void)strcpy(ClientHost, "?"); /* so stats generation looks correct. */
	    Printf("%d I can't get your name.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}
	(void)strcpy(ClientHost, "stdin");
        ClientIP = 0L;
	ServerHost[0] = '\0';
    }

    else {
	if (sin.sin_family != AF_INET) {
	    syslog(L_ERROR, "%s bad_address_family %ld",
		"?", (long)sin.sin_family);
	    Printf("%d Bad address family.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}

	/* Get client's name. */
#if	defined(DO_NNRP_GETHOSTBYADDR)
	HostErrorStr = NULL;
	if (!Address2Name(&sin.sin_addr, ClientHost, (int)sizeof ClientHost)) {
	    (void)strcpy(ClientHost, inet_ntoa(sin.sin_addr));
	    if (HostErrorStr == NULL) {
		syslog(L_NOTICE,
		    "? cant gethostbyaddr %s %m -- using IP address for access",
		    ClientHost);
	    } else {
		syslog(L_NOTICE,
		    "? cant gethostbyaddr %s %s -- using IP address for access",
		    ClientHost, HostErrorStr);
	    }
	    ClientAddr = ClientHost;
            ClientIP = inet_addr(ClientHost);
	}
	else {
	    ClientAddr = buff;
	    (void)strcpy(buff, inet_ntoa(sin.sin_addr));
            ClientIP = inet_addr(buff);
	}
#else
	(void)strcpy(ClientHost, inet_ntoa(sin.sin_addr));
        ClientIP = inet_addr(ClientHost);
#endif /* defined(DO_NNRP_GETHOSTBYADDR) */
	(void)strncpy(ClientIp, inet_ntoa(sin.sin_addr), sizeof(ClientIp));
	length = sizeof sin;
	if (getsockname(STDIN, (struct sockaddr *)&sin, &length) < 0) {
	    syslog(L_NOTICE, "%s can't getsockname %m", ClientHost);
	    Printf("%d Can't figure out where you connected to.  Goodbye\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}
#ifdef DO_NNRP_GETHOSTBYADDR
	HostErrorStr = NULL;
	if (!Address2Name(&sin.sin_addr, ServerHost, sizeof(ServerHost))) {
	    strcpy(ServerHost, inet_ntoa(sin.sin_addr));
	    /* suppress error reason */
	}
#else
        strcpy(ServerHost, inet_ntoa(sin.sin_addr));
#endif /* DO_NNRP_GETHOSTBYADDR */
    }

    strncpy (LogName,ClientHost,sizeof(LogName) - 1) ;
    LogName[sizeof(LogName) - 1] = '\0';

    syslog(L_NOTICE, "%s connect", ClientHost);
#ifdef DO_PERL
    if (innconf->nnrpperlauth) {
	if ((code = perlConnect(ClientHost, ClientIp, ServerHost, accesslist)) == 502) {
	    syslog(L_NOTICE, "%s no_access", ClientHost);
	    Printf("%d You are not in my access file. Goodbye.\r\n",
		   NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}
	PERMspecified = NGgetlist(&PERMreadlist, accesslist);
	PERMpostlist = PERMreadlist;
	if (!authconf)
	    authconf = NEW(ACCESSGROUP, 1);
	PERMaccessconf = authconf;
	SetDefaultAccess(PERMaccessconf);
    } else {
#endif	/* DO_PERL */

#ifdef DO_PYTHON
    if (innconf->nnrppythonauth) {
        if ((code = PY_authenticate(ClientHost, ClientIp, ServerHost, NULL, NULL, accesslist)) < 0) {
	    syslog(L_NOTICE, "PY_authenticate(): authentication skipped due to no Python authentication method defined.");
	} else {
	    if (code == 502) {
	        syslog(L_NOTICE, "%s no_access", ClientHost);
		Printf("%d You are not in my access file. Goodbye.\r\n",
		       NNTP_ACCESS_VAL);
		ExitWithStats(1, TRUE);
	    }
	    PERMspecified = NGgetlist(&PERMreadlist, accesslist);
	    PERMpostlist = PERMreadlist;
	}
	if (!authconf)
	    authconf = NEW(ACCESSGROUP, 1);
	PERMaccessconf = authconf;
	SetDefaultAccess(PERMaccessconf);
    } else {
#endif	/* DO_PYTHON */
	PERMgetaccess();
	PERMgetpermissions();
#ifdef DO_PYTHON
    }
#endif /* DO_PYTHON */
#ifdef DO_PERL
    }
#endif /* DO_PERL */
}


#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
# include <stdarg.h>
# define VA_PARAM(type, param)  (type param, ...)
# define VA_START(param)        (va_start(args, param))
#else
# ifdef HAVE_VARARGS_H
#  include <varargs.h>
#  define VA_PARAM(type, param) (param, va_alist) type param; va_dcl
#  define VA_START(param)       (va_start(args))
# endif
#endif

/* Only compile this function if we have a variadic function mechanism. */
#ifdef VA_PARAM

/*
**  Send a reply, possibly with debugging output.
*/
void
Reply VA_PARAM(const char *, fmt)
{
    va_list     args;
    int         oerrno;
    char *      p;
    char        buff[2048];

#ifdef HAVE_SSL
    if (tls_conn) {
      VA_START(fmt);
      vsprintf(buff,fmt, args);
      va_end(args);
      SSL_write(tls_conn, buff, strlen(buff));
    } else {
      VA_START(fmt);
      vprintf(fmt, args);
      va_end(args);
    }
#else
      VA_START(fmt);
      vprintf(fmt, args);
      va_end(args);
#endif
    if (Tracing) {
        oerrno = errno;
        VA_START(fmt);

        /* Copy output, but strip trailing CR-LF.  Note we're assuming here
           that no output line can ever be longer than 2045 characters. */
        vsprintf(buff, fmt, args);
        va_end(args);
        p = buff + strlen(buff) - 1;
        while (p >= buff && (*p == '\n' || *p == '\r'))
            *p-- = '\0';
        syslog(L_TRACE, "%s > %s", ClientHost, buff);

        errno = oerrno;
    }
}

#ifdef HAVE_SSL
void
Printf VA_PARAM(const char *, fmt)
{
    va_list     args;
    char        buff[2048];

    if (tls_conn) {
      VA_START(fmt);
      vsprintf(buff,fmt, args);
      va_end(args);
      SSL_write(tls_conn, buff, strlen(buff));
    } else {
      VA_START(fmt);
      vprintf(fmt, args);
      va_end(args);
    }
}
#endif /* HAVE_SSL */
#endif /* VA_PARAM */


/*
**  Got a signal; toggle tracing.
*/
STATIC SIGHANDLER
ToggleTrace(s)
    int		s;
{
    ChangeTrace = TRUE;
#ifndef HAVE_SIGACTION
    xsignal(s, ToggleTrace);
#endif
}

/*
** Got a SIGPIPE; exit cleanly
*/
STATIC SIGHANDLER
CatchPipe(s)
    int		s;
{
    ExitWithStats(0, FALSE);
}

/*
**  Got a signal; wait for children.
*/
STATIC SIGHANDLER
WaitChild(s)
    int		s;
{
    int status;
    int pid;

    for (;;) {
       pid = waitnb(&status);
       if (pid <= 0)
       	    break;
    }
#ifndef HAVE_SIGACTION
    xsignal(s, WaitChild);
#endif
}

STATIC void SetupDaemon(void) {
    BOOL                val;
    
#if defined(DO_PERL)
    /* Load the Perl code */
    PERLsetup(NULL, cpcatpath(innconf->pathfilter, _PATH_PERL_FILTER_NNRPD), "filter_post");
    if (innconf->nnrpperlauth) {
	PERLsetup(NULL, cpcatpath(innconf->pathfilter, _PATH_PERL_AUTH), "authenticate");
	PerlFilter(TRUE);
	perlAuthInit();
    } else {
	PerlFilter(TRUE);
    }
#endif /* defined(DO_PERL) */

#ifdef	DO_PYTHON
    /* Load the Python code */
    if (innconf->nnrppythonauth) {
        PY_setup();
    }
#endif /* defined(DO_PYTHON) */
    
    val = TRUE;
    if (SMsetup(SM_PREOPEN, (void *)&val) && !SMinit()) {
	syslog(L_NOTICE, "cant initialize storage method, %s", SMerrorstr);
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
    if (!ARTreadschema()) {
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
    if (!OVopen(OV_READ)) {
	/* This shouldn't really happen. */
	syslog(L_NOTICE, "cant open overview %m");
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
}

/*
**  Print a usage message and exit.
*/
STATIC void
Usage()
{
    (void)fprintf(stderr, "Usage error.\n");
    exit(1);
}


/* ARGSUSED0 */
int
main(int argc, char *argv[])
{
#if	NNRP_LOADLIMIT > 0
    int			load;
#endif	/* NNRP_LOADLIMIT > 0 */
    CMDENT		*cp;
    char		buff[NNTP_STRLEN];
    char		**av;
    int			ac;
    READTYPE		r;
    TIMEINFO		Now;
    int			i;
    char		*Reject;
    int			timeout;
    unsigned int	vid=0; 
    int 		count=123456789;
    struct		timeval tv;
    unsigned short	ListenPort = NNTP_PORT;
    unsigned long	ListenAddr = htonl(INADDR_ANY);
    int			lfd, fd;
    ARGTYPE		clen;
    struct sockaddr_in	ssa, csa;
    struct stat		Sb;
    PID_T		pid = -1;
    GID_T               NewsGID;
    UID_T               NewsUID;
    int                 one = 1;
    FILE                *pidfile;
    struct passwd	*pwd;
    int			clienttimeout;
#if HAVE_GETSPNAM
    struct group	*grp;
    GID_T		shadowgid;
#endif /* HAVE_GETSPNAM */

#ifdef HAVE_SSL
    int ssl_result;
#endif /* HAVE_SSL */

#if	!defined(_HPUX_SOURCE)
    /* Save start and extent of argv for TITLEset. */
    TITLEstart = argv[0];
    TITLEend = argv[argc - 1] + strlen(argv[argc - 1]) - 1;
#endif	/* !defined(_HPUX_SOURCE) */

    /* Parse arguments.   Must COPY() optarg if used because the
     * TITLEset() routine would clobber it! */
    Reject = NULL;
    LLOGenable = FALSE;
    GRPcur = NULL;
    MaxBytesPerSecond = 0;
    strcpy(Username, "unknown");

    openlog("nnrpd", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if (ReadInnConf() < 0) exit(1);

#ifdef HAVE_SSL
    while ((i = getopt(argc, argv, "b:Di:g:op:Rr:s:tS")) != EOF)
#else
    while ((i = getopt(argc, argv, "b:Di:g:op:Rr:s:t")) != EOF)
#endif /* HAVE_SSL */
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
 	case 'b':			/* bind to a certain address in
 	        			   daemon mode */
 	    ListenAddr = inet_addr(optarg);
 	    if (ListenAddr == -1)
 	    	ListenAddr = htonl(INADDR_ANY);
 	    break;
 	case 'D':			/* standalone daemon mode */
 	    DaemonMode = TRUE;
 	    break;
#if HAVE_GETSPNAM
	case 'g':
	    ShadowGroup = optarg;
	    break;
#endif /* HAVE_GETSPNAM */
	case 'i':			/* Initial command */
	    PushedBack = COPY(optarg);
	    break;
	case 'o':
	    Offlinepost = TRUE;		/* Offline posting only */
	    break;
 	case 'p':			/* tcp port for daemon mode */
 	    ListenPort = atoi(optarg);
 	    break;
	case 'R':			/* Ignore 'P' option in access file */
	    ForceReadOnly = TRUE;
	    break;
	case 'r':			/* Reject connection message */
	    Reject = COPY(optarg);
	    break;
	case 's':			/* Unused title string */
	    break;
	case 't':			/* Tracing */
	    Tracing = TRUE;
	    break;
#ifdef HAVE_SSL
	case 'S':			/* SSL negotiation as soon as connected */
	    initialSSL = TRUE;
	    break;
#endif /* HAVE_SSL */
	}
    argc -= optind;
    if (argc)
	Usage();

    /*
     * Make other processes happier if someone is reading
     * This allows other processes like 'overchan' to keep up when
     * there are lots of readers. Note that this is cumulative with
     * 'nicekids'
    */
    if (innconf->nicennrpd > 0)
	nice(innconf->nicennrpd);

    HISTORY = COPY(cpcatpath(innconf->pathdb, _PATH_HISTORY));
    ACTIVE = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVE));
    ACTIVETIMES = COPY(cpcatpath(innconf->pathdb, _PATH_ACTIVETIMES));
    NEWSGROUPS = COPY(cpcatpath(innconf->pathdb, _PATH_NEWSGROUPS));
    NNRPACCESS = COPY(cpcatpath(innconf->pathetc, _PATH_NNRPACCESS));
    SPOOLlen = strlen(innconf->patharticles);

    if (DaemonMode) {
	if ((lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    syslog(L_FATAL, "can't open socket (%m)");
	    exit(1);
	}

	if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0) {
	    syslog(L_FATAL, "can't setsockopt(SO_REUSEADDR) (%m)");
	    exit(1);
	}

	memset(&ssa, '\0', sizeof(ssa));
	ssa.sin_family = AF_INET;
	ssa.sin_addr.s_addr = ListenAddr;
	ssa.sin_port = htons(ListenPort);
	
	if (bind(lfd, (struct sockaddr *) &ssa, sizeof(ssa)) < 0) {
	    fprintf(stderr, "%s: can't bind (%s)\n", argv[0], strerror(errno));
	    syslog(L_FATAL, "can't bind local address (%m)");
	    exit(1);
	}

	/* If started as root, switch to news uid */
	if (getuid() == 0) {
	    if (stat(innconf->pathrun, &Sb) < 0 || !S_ISDIR(Sb.st_mode)) {
		syslog(L_FATAL, "nnrpd cant stat %s %m", innconf->pathrun);
		exit(1);
	    }
	    if (Sb.st_uid == 0) {
		syslog(L_FATAL, "nnrpd %s must not be owned by root", innconf->pathrun);
		exit(1);
	    }
	    pwd = getpwnam(NEWSUSER);
	    if (pwd == (struct passwd *)NULL) {
		syslog(L_FATAL, "nnrpd getpwnam(%s): %s", NEWSUSER, strerror(errno));
		exit(1);
	    } else if (pwd->pw_gid != Sb.st_gid) {
		syslog(L_FATAL, "nnrpd %s must have group %s", innconf->pathrun, NEWSGRP);
		exit(1);
	    } else if (pwd->pw_uid != Sb.st_uid) {
		syslog(L_FATAL, "nnrpd % must be owned by %s", innconf->pathrun, NEWSUSER);
		exit(1);
	    }

#if HAVE_GETSPNAM
	    shadowgid = (gid_t) -1;
	    /* Find shadowgroup gid if needed */
	    if (ShadowGroup != NULL) {
		if ((grp = getgrnam(ShadowGroup)) == NULL)
		    syslog(L_ERROR, "nnrpd cannot find group %s",
				ShadowGroup);
		else
		    shadowgid = grp->gr_gid;
	    } else if ((grp = getgrnam("shadow")) != NULL) {
		/* found default group "shadow" */
		shadowgid = grp->gr_gid;
		ShadowGroup = "shadow";
	    }
	    /* If we have a shadowgid, try to set it as an extra group. */
	    if (shadowgid != (gid_t) -1) {
		if (setgroups(1, &shadowgid) < 0)
		   syslog(L_ERROR, "nnrpd cannot set supplementary group %s %m",
			ShadowGroup);
		else
		   syslog(L_NOTICE, "nnrpd added supplementary group %s",
			ShadowGroup);
	    }
#endif /* HAVE_GETSPNAM */

	    NewsUID = Sb.st_uid;
	    NewsGID = Sb.st_gid;
	    (void)setgid(NewsGID);
	    if (getgid() != NewsGID)
		syslog(L_ERROR, "nnrpd cant setgid to %d %m", NewsGID);
	    (void)setuid(NewsUID);
	    if (getuid() != NewsUID)
		syslog(L_ERROR, "nnrpd cant setuid to %d %m", NewsUID);
	}

	/* Detach */
	if ((pid = fork()) < 0) {
	    fprintf(stderr, "%s: can't fork: %s\n", argv[0], strerror(errno));
	    syslog(L_FATAL, "cant fork: %m");
	    exit(1);
	} else if (pid != 0) 
	    exit(0);

	setsid();

	if ((pidfile = fopen(cpcatpath(innconf->pathrun, "nnrpd.pid"),
                                                "w")) == NULL) {
	    syslog(L_ERROR, "cannot write nnrpd.pid %m");
            exit(1);
	}
	fprintf(pidfile,"%d\n", getpid());
	fclose(pidfile);

	/* Set signal handle to care for dead children */
	(void)xsignal(SIGCHLD, WaitChild);

	/* Arrange to toggle tracing. */
	(void)xsignal(SIGHUP, ToggleTrace);
 
	TITLEset("nnrpd: accepting connections");
 	
	listen(lfd, 128);	

	do {
	    clen = sizeof(csa);
	    fd = accept(lfd, (struct sockaddr *) &csa, &clen);
	    if (fd < 0)
		continue;
	    
	    for (i = 0; (pid = fork()) < 0; i++) {
		if (i == MAX_FORKS) {
		    syslog(L_FATAL, "cant fork (dropping connection): %m");
		    continue;
		}
		syslog(L_NOTICE, "cant fork (waiting): %m");
		sleep(1);
	    }
	    if (ChangeTrace) {
		Tracing = Tracing ? FALSE : TRUE;
		syslog(L_TRACE, "trace %sabled", Tracing ? "en" : "dis");
		ChangeTrace = FALSE;
	    }
	    if (pid != 0)
		close(fd);
	} while (pid != 0);

	/* child process starts here */
	TITLEset("nnrpd: connected");
	close(lfd);
	dup2(fd, 0);
	close(fd);
	dup2(0, 1);
	dup2(0, 2);
	SetupDaemon();

	/* if we are a daemon innd didn't make us nice, so be nice kids */
	if (innconf->nicekids) {
	    if (nice(innconf->nicekids) < 0)
		syslog(L_ERROR, "Could not nice child to %d: %m", innconf->nicekids);
	}

	/* Only automatically reap children in the listening process */
	(void)xsignal(SIGCHLD, SIG_DFL);
 
    } else {
	SetupDaemon();
	/* Arrange to toggle tracing. */
	(void)xsignal(SIGHUP, ToggleTrace);
    }/* DaemonMode */

    /* Setup. */
    if (GetTimeInfo(&Now) < 0) {
	syslog(L_FATAL, "cant gettimeinfo %m");
	OVclose();
	exit(1);
    }
    STATstart = TIMEINFOasDOUBLE(Now);

#ifdef HAVE_SSL
    if (initialSSL) {
      sasl_config_read();
      ssl_result=tls_init_serverengine(5,        /* depth to verify */
				       0,        /* can client auth? */
				       0,        /* required client to auth? */
				       (char *)sasl_config_getstring("tls_ca_file", ""),
				       (char *)sasl_config_getstring("tls_ca_path", ""),
				       (char *)sasl_config_getstring("tls_cert_file", ""),
				       (char *)sasl_config_getstring("tls_key_file", ""));
      if (ssl_result == -1) {
	Reply("%d Error initializing TLS\r\n", NNTP_STARTTLS_BAD_VAL);
	
	syslog(L_ERROR, "error initializing TLS: "
	       "[CA_file: %s] [CA_path: %s] [cert_file: %s] [key_file: %s]",
	       (char *) sasl_config_getstring("tls_ca_file", ""),
	       (char *) sasl_config_getstring("tls_ca_path", ""),
	       (char *) sasl_config_getstring("tls_cert_file", ""),
	       (char *) sasl_config_getstring("tls_key_file", ""));
	ExitWithStats(1, FALSE);
      }

      ssl_result=tls_start_servertls(0, /* read */
				     1); /* write */
      if (ssl_result==-1) {
	Reply("%d SSL connection failed\r\n", NNTP_STARTTLS_BAD_VAL);
	ExitWithStats(1, FALSE);
      }

      nnrpd_starttls_done=1;
    }
#endif /* HAVE_SSL */

#if	NNRP_LOADLIMIT > 0
    if ((load = GetLoadAverage()) > NNRP_LOADLIMIT) {
	syslog(L_NOTICE, "load %d > %d", load, NNRP_LOADLIMIT);
	Reply("%d load at %d, try later\r\n", NNTP_GOODBYE_VAL, load);
	ExitWithStats(1, TRUE);
    }
#endif	/* NNRP_LOADLIMIT > 0 */

    strcpy (LogName, "?");

    /* Catch SIGPIPE so that we can exit out of long write loops */
    (void)xsignal(SIGPIPE, CatchPipe);

    /* Get permissions and see if we can talk to this client */
    StartConnection();
    if (!PERMcanread && !PERMcanpost && !PERMneedauth) {
	syslog(L_NOTICE, "%s no_permission", ClientHost);
	Printf("%d You have no permission to talk.  Goodbye.\r\n",
	       NNTP_ACCESS_VAL);
	ExitWithStats(1, FALSE);
    }

    /* Proceed with initialization. */
    TITLEset("connect");

    /* Were we told to reject connections? */
    if (Reject) {
	syslog(L_NOTICE, "%s rejected %s", ClientHost, Reject);
	Reply("%s %s\r\n", NNTP_GOODBYE, Reject);
	ExitWithStats(0, FALSE);
    }

    if (PERMaccessconf) {
	if (PERMaccessconf->readertrack)
	    PERMaccessconf->readertrack=TrackClient(ClientHost,Username);
    } else {
	if (innconf->readertrack)
	    innconf->readertrack=TrackClient(ClientHost,Username);
    }

    if (PERMaccessconf && PERMaccessconf->readertrack || !PERMaccessconf && innconf->readertrack) {
	int len;
	syslog(L_NOTICE, "%s Tracking Enabled (%s)", ClientHost, Username);
	pid=getpid();
	gettimeofday(&tv,NULL);
	count += pid;
	vid = tv.tv_sec ^ tv.tv_usec ^ pid ^ count;
	len = strlen("innconf->pathlog") + strlen("/tracklogs/log-") + BUFSIZ;
	LocalLogFileName = NEW(char, len);
	sprintf(LocalLogFileName, "%s/tracklogs/log-%ld", innconf->pathlog, vid);
	if ((locallog = fopen(LocalLogFileName, "w")) == NULL) {
	    LocalLogDirName = NEW(char, len);
	    sprintf(LocalLogDirName, "%s/tracklogs", innconf->pathlog);
	    MakeDirectory(LocalLogDirName, FALSE);
	    DISPOSE(LocalLogDirName);
	}
	if (locallog == NULL && (locallog = fopen(LocalLogFileName, "w")) == NULL) {
	    syslog(L_ERROR, "%s Local Logging failed (%s) %s: %m", ClientHost, Username, LocalLogFileName);
	} else {
	    syslog(L_NOTICE, "%s Local Logging begins (%s) %s",ClientHost, Username, LocalLogFileName);
	    fprintf(locallog, "%s Tracking Enabled (%s)\n", ClientHost, Username);
	    fflush(locallog);
	    LLOGenable = TRUE;
	}
    }

    if (PERMaccessconf) {
        Reply("%d %s InterNetNews NNRP server %s ready (%s).\r\n",
	   PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL,
           PERMaccessconf->pathhost, inn_version_string,
	   PERMcanpost ? "posting ok" : "no posting");
	clienttimeout = PERMaccessconf->clienttimeout;
    } else {
        Reply("%d %s InterNetNews NNRP server %s ready (%s).\r\n",
	   PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL,
           innconf->pathhost, inn_version_string,
	   PERMcanpost ? "posting ok" : "no posting");
	clienttimeout = innconf->clienttimeout;
    }

    /* Main dispatch loop. */
    for (timeout = INITIAL_TIMEOUT, av = NULL; ;
			timeout = clienttimeout) {
	(void)fflush(stdout);
	if (ChangeTrace) {
	    Tracing = Tracing ? FALSE : TRUE;
	    syslog(L_TRACE, "trace %sabled", Tracing ? "en" : "dis");
	    ChangeTrace = FALSE;
	}
	if (PushedBack) {
	    if (PushedBack[0] == '\0')
		continue;
	    if (Tracing)
		syslog(L_TRACE, "%s < %s", ClientHost, PushedBack);
	    ac = Argify(PushedBack, &av);
	    r = RTok;
	}
	else
	    switch (r = READline(buff, (int)sizeof buff, timeout)) {
	    default:
		syslog(L_ERROR, "%s internal %d in main", ClientHost, r);
		/* FALLTHROUGH */
	    case RTtimeout:
		if (timeout < clienttimeout)
		    syslog(L_NOTICE, "%s timeout short", ClientHost);
		else
		    syslog(L_NOTICE, "%s timeout", ClientHost);
		Printf("%d Timeout after %d seconds, closing connection.\r\n",
		       NNTP_TEMPERR_VAL, timeout);
		ExitWithStats(1, FALSE);
		break;
	    case RTlong:
		Reply("%d Line too long\r\n", NNTP_BAD_COMMAND_VAL);
		continue;
	    case RTok:
		/* Do some input processing, check for blank line. */
		if (Tracing)
		    syslog(L_TRACE, "%s < %s", ClientHost, buff);
		if (buff[0] == '\0')
		    continue;
		ac = Argify(buff, &av);
		break;
	    case RTeof:
		/* Handled below. */
		break;
	    }

	/* Client gone? */
	if (r == RTeof)
	    break;
	if (ac == 0 || caseEQ(av[0], "quit"))
	    break;

	/* Find command. */
	for (cp = CMDtable; cp->Name; cp++)
	    if (caseEQ(cp->Name, av[0]))
		break;
	if (cp->Name == NULL) {
	    if ((int)strlen(buff) > 40)
		syslog(L_NOTICE, "%s unrecognized %.40s...", ClientHost, buff);
	    else
		syslog(L_NOTICE, "%s unrecognized %s", ClientHost, buff);
	    Reply("%d What?\r\n", NNTP_BAD_COMMAND_VAL);
	    continue;
	}

	/* Check usage. */
	if ((cp->Minac != CMDany && ac < cp->Minac)
	 || (cp->Maxac != CMDany && ac > cp->Maxac)) {
	    Reply("%d %s\r\n",
		NNTP_SYNTAX_VAL,  cp->Help ? cp->Help : "Usage error");
	    continue;
	}

	/* Check permissions and dispatch. */
	if (cp->Needauth && PERMneedauth) {
	    Reply("%d Authentication required for command\r\n",
		NNTP_AUTH_NEEDED_VAL);
	    continue;
	}
	TITLEset(av[0]);
	(*cp->Function)(ac, av);
	if (PushedBack)
	    break;
    }

    Reply("%s\r\n", NNTP_GOODBYE_ACK);

    ExitWithStats(0, FALSE);
    /* NOTREACHED */
    return 1;
}
