/*  $Id$
**
**  NNTP server for readers (NNRP) for InterNetNews.
**
**  This server doesn't do any real load-limiting, except for what has
**  proven empirically necesary (i.e., look at GRPscandir).
*/

#include "config.h"
#include "clibrary.h"
#include "portable/wait.h"
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
#include <stdarg.h>
#if defined(_HPUX_SOURCE)
# include <sys/pstat.h>
#endif
#if HAVE_GETSPNAM
# include <shadow.h>
#endif

#ifdef HAVE_SSL
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/bio.h>
# include <openssl/pem.h>
# include "tls.h"
# include "sasl_config.h"
extern SSL *tls_conn;
int nnrpd_starttls_done = 0;
#endif 

#if NEED_HERRNO_DECLARATION
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
    const char *        Name;
    void                (*Function)(int, char **);
    bool                Needauth;
    int                 Minac;
    int                 Maxac;
    const char *        Help;
} CMDENT;


char	NOACCESS[] = NNTP_ACCESS;
char	*ACTIVE = NULL;
char	*ACTIVETIMES = NULL;
char	*HISTORY = NULL;
char	*NEWSGROUPS = NULL;
char	*NNRPACCESS = NULL;

bool	ForceReadOnly = FALSE;
static char 	*LocalLogFileName = NULL;
static char 	*LocalLogDirName;

struct history *History;
static double	STATstart;
static double	STATfinish;
static char	*PushedBack;
#if	!defined(_HPUX_SOURCE)
static char	*TITLEstart;
static char	*TITLEend;
#endif	/* !defined(_HPUX_SOURCE) */
static sig_atomic_t	ChangeTrace;
bool	DaemonMode = FALSE;
bool	ForeGroundMode = FALSE;
#if HAVE_GETSPNAM
static const char	*ShadowGroup;
#endif
static const char 	*HostErrorStr;
bool GetHostByAddr = TRUE;      /* formerly DO_NNRP_GETHOSTBYADDR */

#ifdef DO_PERL
bool   PerlLoaded = FALSE;
#endif /* DO_PERL */

bool LLOGenable;

static char	CMDfetchhelp[] = "[MessageID|Number]";

static CMDENT	CMDtable[] = {
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
    {	"hdr",		CMDpat,		TRUE,	3,	3,
	"header range|MessageID" },
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
    {	"over",		CMDxover,	TRUE,	1,	2,
	"[range]" },
    {	"post",		CMDpost,	TRUE,	1,	1,
	NULL },
    {	"slave",	CMD_unimp,	FALSE,	1,	1,
	NULL },
    {	"stat",		CMDfetch,	TRUE,	1,	2,
	CMDfetchhelp },
    {	"xgtitle",	CMDxgtitle,	TRUE,	1,	2,
	"[group_pattern]" },
    {	"xhdr",		CMDpat,		TRUE,	2,	3,
	"header [range|MessageID]" },
    {	"xover",	CMDxover,	TRUE,	1,	2,
	"[range]" },
    {	"xpat",		CMDpat,		TRUE,	4,	CMDany,
	"header range|MessageID pat [morepat...]" },
    {	"xpath",	CMDxpath,	TRUE,	2,	2,
	"MessageID" },
    {	NULL }
};


/*
**  Log a summary status message and exit.
*/
void
ExitWithStats(int x, bool readconf)
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
    syslog(L_NOTICE, "%s times user %.3f system %.3f idle %.3f elapsed %.3f",
	ClientHost, usertime, systime, IDLEtime, STATfinish - STATstart);
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
     	shutdown(STDIN_FILENO, 2);
     	shutdown(STDOUT_FILENO, 2);
     	shutdown(STDERR_FILENO, 2);
 	close(STDIN_FILENO);
 	close(STDOUT_FILENO);
 	close(STDERR_FILENO);
     }
    
    OVclose();

#ifdef DO_PYTHON
    if (innconf->nnrppythonauth) {
        if (PY_close() < 0) {
	    syslog(L_NOTICE, "PY_close(): close method not invoked because it is not defined in Python authenticaton module.");
	}
    }
#endif	/* DO_PYTHON */

    HISclose(History);

    if (LocalLogFileName != NULL)
	DISPOSE(LocalLogFileName);
    exit(x);
}


/*
**  The "help" command.
*/
/* ARGSUSED0 */
void
CMDhelp(int ac UNUSED, char *av[] UNUSED)
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
void
CMD_unimp(ac, av)
    int		ac UNUSED;
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
static void
TITLEset(const char* what)
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


#ifndef	INADDR_LOOPBACK
#define	INADDR_LOOPBACK	0x7f000001
#endif	/* INADDR_LOOPBACK */
/*
**  Convert an IP address to a hostname.  Don't trust the reverse lookup,
**  since anyone can fake .in-addr.arpa entries.
*/
static bool
Address2Name(INADDR *ap, char *hostname, int i)
{
    char		*p;
    struct hostent	*hp;
    static char		mismatch_error[] = "reverse lookup validation failed";
#if	defined(h_addr)
    char		**pp;
#endif

    /* Get the official hostname, store it away. */
    if ((hp = gethostbyaddr((char *)ap, sizeof *ap, AF_INET)) == NULL) {
	HostErrorStr = hstrerror(h_errno);
	return FALSE;
    }
    (void)strncpy(hostname, hp->h_name, i);
    hostname[i - 1] = '\0';

    /* Get addresses for this host. */
    if ((hp = gethostbyname(hostname)) == NULL) {
	HostErrorStr = hstrerror(h_errno);
	return FALSE;
    }

    /* Make sure one of those addresses is the address we got. */
#if	defined(h_addr)
    /* We have many addresses */
    for (pp = hp->h_addr_list; *pp; pp++)
	if (EQn((const char *)&ap->s_addr, *pp, hp->h_length))
	    break;
    if (*pp == NULL)
    {
	HostErrorStr = mismatch_error;
	return FALSE;
    }
#else
    /* We have one address. */
    if (!EQn(&ap->s_addr, hp->h_addr, hp->h_length))
    {
	HostErrorStr = mismatch_error;
	return FALSE;
    }
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

/*
**  Convert an IPv6 address to a hostname.  Don't trust the reverse lookup,
**  since anyone can fake .ip6.arpa entries.
*/
#ifdef HAVE_INET6
static bool
Address2Name6(struct sockaddr *sa, char *hostname, int i)
{
    static char		mismatch_error[] = "reverse lookup validation failed";
    int ret;
    bool valid = 0;
    struct addrinfo hints, *res, *res0;

    /* Get the official hostname, store it away. */
    ret = getnameinfo( sa, SA_LEN( sa ), hostname, i, NULL, 0, NI_NAMEREQD );
    if( ret != 0 )
    {
	HostErrorStr = gai_strerror( ret );
	return FALSE;
    }

    /* Get addresses for this host. */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET6;
    if( ( ret = getaddrinfo( hostname, NULL, &hints, &res0 ) ) != 0 )
    {
	HostErrorStr = gai_strerror( ret );
	return FALSE;
    }

    /* Make sure one of those addresses is the address we got. */
    for( res = res0; res; res = res->ai_next )
    {
#ifdef HAVE_BROKEN_IN6_ARE_ADDR_EQUAL
	if( ! memcmp( &(((struct sockaddr_in6 *)sa)->sin6_addr),
		    &(((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr),
		    sizeof( struct in6_addr ) ) )
#else
	if( IN6_ARE_ADDR_EQUAL( &(((struct sockaddr_in6 *)sa)->sin6_addr),
		    &(((struct sockaddr_in6 *)(res->ai_addr))->sin6_addr) ) )
#endif
	{
	    valid = 1;
	    break;
	}
    }

    freeaddrinfo( res0 );

    if( valid ) return TRUE;
    else
    {
	HostErrorStr = mismatch_error;
	return FALSE;
    }
}
#endif


static bool
Sock2String( struct sockaddr *sa, char *string, int len, bool lookup )
{
    struct sockaddr_in *sin = (struct sockaddr_in *)sa;

#ifdef HAVE_INET6
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)sa;
    struct sockaddr_in temp;

    if( sa->sa_family == AF_INET6 )
    {
	if( ! IN6_IS_ADDR_V4MAPPED(&sin6->sin6_addr) )
	{
	    if( lookup )
	    {
		return Address2Name6(sa, string, len);
	    } else {
		strncpy( string, sprint_sockaddr( sa ), len );
		return TRUE;
	    }
	} else {
	    temp.sin_family = AF_INET;
	    memcpy( &temp.sin_addr, sin6->sin6_addr.s6_addr + 12, 4 );
	    temp.sin_port = sin6->sin6_port;
	    sin = &temp;
	    /* fall through to AF_INET case */
	}
    }
#endif
    if( lookup ) {
	return Address2Name(&sin->sin_addr, string, len);
    } else {
	strncpy( string, inet_ntoa(sin->sin_addr), len );
	return TRUE;
    }
}

/*
**  Determine access rights of the client.
*/
static void StartConnection(void)
{
    struct sockaddr_storage	ssc, sss;
    socklen_t		length;
#ifdef DO_PYTHON
    char		accesslist[BIG_BUFFER];
    int                 code;
    static ACCESSGROUP	*authconf;
#endif
    const char		*default_host_error = "unknown error";

    ClientIpAddr = 0L;
    ClientHost[0] = '\0';
    ClientIpString[0] = '\0';
    ClientPort = 0;
    ServerHost[0] = '\0';
    ServerIpString[0] = '\0';
    ServerPort = 0;

    /* Get the peer's name. */
    length = sizeof ssc;
    if (getpeername(STDIN_FILENO, (struct sockaddr *)&ssc, &length) < 0) {
      if (!isatty(STDIN_FILENO)) {
	    syslog(L_TRACE, "%s cant getpeername %m", "?");
            (void)strcpy(ClientHost, "?"); /* so stats generation looks correct. */
	    Printf("%d I can't get your name.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}
	(void)strcpy(ClientHost, "stdin");
    }

    else {
#ifdef HAVE_INET6
	if ( ssc.ss_family != AF_INET && ssc.ss_family != AF_INET6) {
#else
	if ( ssc.ss_family != AF_INET ) {
#endif
	    syslog(L_ERROR, "%s bad_address_family %ld",
		"?", (long)ssc.ss_family);
	    Printf("%d Bad address family.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}

	length = sizeof sss;
	if (getsockname(STDIN_FILENO, (struct sockaddr *)&sss, &length) < 0) {
	    syslog(L_NOTICE, "%s can't getsockname %m", ClientHost);
	    Printf("%d Can't figure out where you connected to.  Goodbye\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}

	/* figure out client's IP address/hostname */
	HostErrorStr = default_host_error;
	if( ! Sock2String( (struct sockaddr *)&ssc, ClientIpString,
				sizeof( ClientIpString ), FALSE ) ) {
            syslog(L_NOTICE, "? cant get client numeric address: %s", HostErrorStr);
	    ExitWithStats(1, TRUE);
	}
	if(GetHostByAddr) {
	    HostErrorStr = default_host_error;
	    if( ! Sock2String( (struct sockaddr *)&ssc, ClientHost,
				    sizeof( ClientHost ), TRUE ) ) {
                syslog(L_NOTICE,
                       "? reverse lookup for %s failed: %s -- using IP address for access",
                       ClientIpString, HostErrorStr);
	        strcpy( ClientHost, ClientIpString );
	    }
	} else strcpy( ClientHost, ClientIpString );

	/* figure out server's IP address/hostname */
	HostErrorStr = default_host_error;
	if( ! Sock2String( (struct sockaddr *)&sss, ServerIpString,
				sizeof( ServerIpString ), FALSE ) ) {
            syslog(L_NOTICE, "? cant get server numeric address: %s", HostErrorStr);
	    ExitWithStats(1, TRUE);
	}
	if(GetHostByAddr) {
	    HostErrorStr = default_host_error;
	    if( ! Sock2String( (struct sockaddr *)&sss, ServerHost,
				    sizeof( ServerHost ), TRUE ) ) {
                syslog(L_NOTICE,
                       "? reverse lookup for %s failed: %s -- using IP address for access",
                       ServerIpString, HostErrorStr);
	        strcpy( ServerHost, ServerIpString );
	    }
	} else strcpy( ServerHost, ServerIpString );

	/* get port numbers */
	switch( ssc.ss_family ) {
	    case AF_INET:
		ClientPort = ntohs( ((struct sockaddr_in *)&ssc)->sin_port );
		ServerPort = ntohs( ((struct sockaddr_in *)&sss)->sin_port );
		break;
#ifdef HAVE_INET6
	    case AF_INET6:
		ClientPort = ntohs( ((struct sockaddr_in6 *)&ssc)->sin6_port );
		ServerPort = ntohs( ((struct sockaddr_in6 *)&sss)->sin6_port );
		break;
#endif
	}
    }

    strncpy (LogName,ClientHost,sizeof(LogName) - 1) ;
    LogName[sizeof(LogName) - 1] = '\0';

    syslog(L_NOTICE, "%s (%s) connect", ClientHost, ClientIpString);
#ifdef DO_PYTHON
    if (innconf->nnrppythonauth) {
        if ((code = PY_authenticate(ClientHost, ClientIpString, ServerHost, NULL, NULL, accesslist)) < 0) {
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
	PERMgetaccess(NNRPACCESS);
	PERMgetpermissions();
#ifdef DO_PYTHON
    }
#endif /* DO_PYTHON */
}


/*
**  Send a reply, possibly with debugging output.
*/
void
Reply(const char *fmt, ...)
{
    va_list     args;
    int         oerrno;
    char *      p;
    char        buff[2048];

#ifdef HAVE_SSL
    if (tls_conn) {
      va_start(args, fmt);
      vsprintf(buff,fmt, args);
      va_end(args);
      SSL_write(tls_conn, buff, strlen(buff));
    } else {
      va_start(args, fmt);
      vprintf(fmt, args);
      va_end(args);
    }
#else
      va_start(args, fmt);
      vprintf(fmt, args);
      va_end(args);
#endif
    if (Tracing) {
        oerrno = errno;
        va_start(args, fmt);

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
Printf(const char *fmt, ...)
{
    va_list     args;
    char        buff[2048];

    if (tls_conn) {
      va_start(args, fmt);
      vsprintf(buff, fmt, args);
      va_end(args);
      SSL_write(tls_conn, buff, strlen(buff));
    } else {
      va_start(args, fmt);
      vprintf(fmt, args);
      va_end(args);
    }
}
#endif /* HAVE_SSL */


#ifdef HAVE_SIGACTION
#define NO_SIGACTION_UNUSED UNUSED
#else
#define NO_SIGACTION_UNUSED
#endif
/*
**  Got a signal; toggle tracing.
*/
static RETSIGTYPE
ToggleTrace(int s NO_SIGACTION_UNUSED)
{
    ChangeTrace = TRUE;
#ifndef HAVE_SIGACTION
    xsignal(s, ToggleTrace);
#endif
}

/*
** Got a SIGPIPE; exit cleanly
*/
static RETSIGTYPE
CatchPipe(int s UNUSED)
{
    ExitWithStats(0, FALSE);
}

/*
**  Got a signal; wait for children.
*/
static RETSIGTYPE
WaitChild(int s NO_SIGACTION_UNUSED)
{
    int pid;

    for (;;) {
       pid = waitpid(-1, NULL, WNOHANG);
       if (pid <= 0)
       	    break;
    }
#ifndef HAVE_SIGACTION
    xsignal(s, WaitChild);
#endif
}

static void SetupDaemon(void) {
    bool                val;
    time_t statinterval;

#ifdef	DO_PYTHON
    /* Load the Python code */
    if (innconf->nnrppythonauth) {
        PY_setup();
    }
#endif /* defined(DO_PYTHON) */

    History = HISopen(HISTORY, innconf->hismethod, HIS_RDONLY);
    if (!History) {
	syslog(L_NOTICE, "cant initialize history");
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
    statinterval = 30;
    HISctl(History, HISCTLS_STATINTERVAL, &statinterval);

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
    if (!OVctl(OVCACHEKEEP, &val)) {
	syslog(L_NOTICE, "cant enable overview cache %m");
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
}

/*
**  Print a usage message and exit.
*/
static void
Usage(void)
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
    socklen_t		clen;
    struct sockaddr_in	ssa, csa;
    struct stat		Sb;
    pid_t		pid = -1;
    gid_t               NewsGID;
    uid_t               NewsUID;
    int                 one = 1;
    FILE                *pidfile;
    struct passwd	*pwd;
    int			clienttimeout;
    char		*ConfFile = NULL;
    char                *path;
#if HAVE_GETSPNAM
    struct group	*grp;
    gid_t		shadowgid;
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
    while ((i = getopt(argc, argv, "c:b:Dfi:g:nop:Rr:s:tS")) != EOF)
#else
    while ((i = getopt(argc, argv, "c:b:Dfi:g:nop:Rr:s:t")) != EOF)
#endif /* HAVE_SSL */
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'c':		/* use alternate readers.conf */
	    ConfFile = concatpath(innconf->pathetc, optarg);
	    break;
 	case 'b':			/* bind to a certain address in
 	        			   daemon mode */
            if (inet_aton(optarg, &ssa.sin_addr))
                ListenAddr = ssa.sin_addr.s_addr;
            else
 	    	ListenAddr = htonl(INADDR_ANY);
 	    break;
 	case 'D':			/* standalone daemon mode */
 	    DaemonMode = TRUE;
 	    break;
 	case 'f':			/* Don't fork on daemon mode */
 	    ForeGroundMode = TRUE;
 	    break;
#if HAVE_GETSPNAM
	case 'g':
	    ShadowGroup = optarg;
	    break;
#endif /* HAVE_GETSPNAM */
	case 'i':			/* Initial command */
	    PushedBack = COPY(optarg);
	    break;
	case 'n':			/* No DNS lookups */
	    GetHostByAddr = FALSE;
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

    HISTORY = concatpath(innconf->pathdb, _PATH_HISTORY);
    ACTIVE = concatpath(innconf->pathdb, _PATH_ACTIVE);
    ACTIVETIMES = concatpath(innconf->pathdb, _PATH_ACTIVETIMES);
    NEWSGROUPS = concatpath(innconf->pathdb, _PATH_NEWSGROUPS);
    if(ConfFile)
        NNRPACCESS = ConfFile;
    else
        NNRPACCESS = concatpath(innconf->pathetc,_PATH_NNRPACCESS);
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
	if (!ForeGroundMode) {
	  if ((pid = fork()) < 0) {
	    fprintf(stderr, "%s: can't fork: %s\n", argv[0], strerror(errno));
	    syslog(L_FATAL, "cant fork: %m");
	    exit(1);
	  } else if (pid != 0) 
	    exit(0);
	}

	setsid();

	if (ListenPort == NNTP_PORT)
	    strcpy(buff, "nnrpd.pid");
	else
	    sprintf(buff, "nnrpd-%d.pid", ListenPort);
        path = concatpath(innconf->pathrun, buff);
        pidfile = fopen(path, "w");
        free(path);
	if (pidfile == NULL) {
	    syslog(L_ERROR, "cannot write %s %m", buff);
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
    ClientSSL = FALSE;
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
      ClientSSL = TRUE;
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

    if ((PERMaccessconf && PERMaccessconf->readertrack)
        || (!PERMaccessconf && innconf->readertrack)) {
	int len;
	syslog(L_NOTICE, "%s Tracking Enabled (%s)", ClientHost, Username);
	pid=getpid();
	gettimeofday(&tv,NULL);
	count += pid;
	vid = tv.tv_sec ^ tv.tv_usec ^ pid ^ count;
	len = strlen("innconf->pathlog") + strlen("/tracklogs/log-") + BUFSIZ;
	LocalLogFileName = NEW(char, len);
	sprintf(LocalLogFileName, "%s/tracklogs/log-%d", innconf->pathlog, vid);
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
    for (timeout = INITIAL_TIMEOUT, av = NULL, ac = 0; ;
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
