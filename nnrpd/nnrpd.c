/*  $Id$
**
**  NNTP server for readers (NNRP) for InterNetNews.
**
**  This server doesn't do any real load-limiting, except for what has
**  proven empirically necesary (i.e., look at GRPscandir).
*/

#include "config.h"
#include "clibrary.h"
#include "portable/setproctitle.h"
#include "portable/wait.h"
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>

#if HAVE_GETSPNAM
# include <shadow.h>
#endif

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"
#include "ov.h"
#define MAINLINE
#include "nnrpd.h"

#include "tls.h"
#include "sasl_config.h"

#ifdef HAVE_SSL
#include <openssl/e_os.h>
extern SSL *tls_conn;
int nnrpd_starttls_done = 0;
#endif 

#if NEED_HERRNO_DECLARATION
extern int h_errno;
#endif

/* If we have getloadavg, include the appropriate header file.  Otherwise,
   just assume that we always have a load of 0. */
#if HAVE_GETLOADAVG
# if HAVE_SYS_LOADAVG_H
#  include <sys/loadavg.h>
# endif
#else
static int
getloadavg(double loadavg[], int nelem)
{
    int i;

    for (i = 0; i < nelem && i < 3; i++)
        loadavg[i] = 0;
    return i;
}
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
static sig_atomic_t	ChangeTrace;
bool	DaemonMode = FALSE;
bool	ForeGroundMode = FALSE;
#if HAVE_GETSPNAM
static const char	*ShadowGroup;
#endif
static const char 	*HostErrorStr;
bool GetHostByAddr = TRUE;      /* formerly DO_NNRP_GETHOSTBYADDR */
const char *NNRPinstance = "";

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
    {	"head",		CMDfetch,	TRUE,	1,	2,
	CMDfetchhelp },
    {	"help",		CMDhelp,	FALSE,	1,	CMDany,
	NULL },
    {	"ihave",	CMDpost,	TRUE,	2,	2,
	"MessageID" },
    {	"last",		CMDnextlast,	TRUE,	1,	1,
	NULL },
    {	"list",		CMDlist,	TRUE,	1,	3,
	"[active|active.times|extensions|newsgroups|distributions|distrib.pats|overview.fmt|subscriptions|motd]" },
    {	"listgroup",	CMDgroup,	TRUE,	1,	2,
	"newsgroup" },
    {	"mode",		CMDmode,	FALSE,	2,	2,
	"reader" },
    {	"newgroups",	CMDnewgroups,	TRUE,	3,	5,
	"[YY]yymmdd hhmmss [\"GMT\"]" },
    {	"newnews",	CMDnewnews,	TRUE,	4,	5,
	"newsgroups [YY]yymmdd hhmmss [\"GMT\"]" },
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
    {	"xhdr",		CMDpat,		TRUE,	2,	3,
	"header [range|MessageID]" },
    {	"xover",	CMDxover,	TRUE,	1,	2,
	"[range]" },
    {	"xpat",		CMDpat,		TRUE,	4,	CMDany,
	"header range|MessageID pat [morepat...]" },
    {	"xpath",	CMDxpath,	TRUE,	2,	2,
	"MessageID" },
    {	NULL,           CMD_unimp,      FALSE,  0,      0,
        NULL }
};


static const char *const timer_name[] = {
    "idle",
    "newnews",
    "readart",
    "checkart",
    "nntpread",
    "nntpwrite",
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

    line_free(&NNTPline);
    fflush(stdout);
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
        syslog(L_NOTICE, "%s artstats get %ld time %ld size %ld", ClientHost,
            ARTget, ARTgettime, ARTgetsize);
    if (!readconf && PERMaccessconf && PERMaccessconf->nnrpdoverstats && OVERcount)
        syslog(L_NOTICE, "%s overstats count %ld hit %ld miss %ld time %ld size %ld dbz %ld seek %ld get %ld artcheck %ld", ClientHost,
            OVERcount, OVERhit, OVERmiss, OVERtime, OVERsize, OVERdbz, OVERseek, OVERget, OVERartcheck);

#ifdef HAVE_SSL
     if (tls_conn) {
        SSL_shutdown(tls_conn);
        SSL_free(tls_conn);
        tls_conn = NULL;
     } 
#endif

     if (DaemonMode) {
     	shutdown(STDIN_FILENO, 2);
     	shutdown(STDOUT_FILENO, 2);
     	shutdown(STDERR_FILENO, 2);
 	close(STDIN_FILENO);
 	close(STDOUT_FILENO);
 	close(STDERR_FILENO);
     }
    
    OVclose();
    SMshutdown();

#ifdef DO_PYTHON
    if (innconf->nnrppythonauth)
        PY_close();
#endif

    if (History)
	HISclose(History);

    TMRsummary(ClientHost, timer_name);
    TMRfree();

    if (LocalLogFileName != NULL)
	DISPOSE(LocalLogFileName);
    closelog();
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
    static const char *newsmaster = NEWSMASTER;

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
	    /* sigh, pickup from newsmaster anyway */
	    if ((p = strchr(newsmaster, '@')) == NULL)
		Printf("Report problems to <%s@%s>\r\n",
		    newsmaster, PERMaccessconf->domain);
	    else {
		q = NEW(char, p - newsmaster + 1);
		strncpy(q, newsmaster, p - newsmaster);
		q[p - newsmaster] = '\0';
		Printf("Report problems to <%s@%s>\r\n",
		    q, PERMaccessconf->domain);
		DISPOSE(q);
	    }
	}
    } else {
	if (strchr(newsmaster, '@') == NULL)
	    Printf("Report problems to <%s@%s>\r\n",
		newsmaster, innconf->fromhost);
	else
	    Printf("Report problems to <%s>\r\n",
		newsmaster);
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
    if (caseEQ(av[0], "slave"))
	/* Somebody sends us this?  I don't believe it! */
	Reply("%d Unsupported\r\n", NNTP_SLAVEOK_VAL);
    else
	Reply("%d %s not implemented; try help\r\n",
	    NNTP_BAD_COMMAND_VAL, av[0]);
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
    char		**pp;

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
    for (pp = hp->h_addr_list; *pp; pp++)
	if (EQn((const char *)&ap->s_addr, *pp, hp->h_length))
	    break;
    if (*pp == NULL)
    {
	HostErrorStr = mismatch_error;
	return FALSE;
    }

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
    struct sockaddr_in *sin4 = (struct sockaddr_in *)sa;

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
	    sin4 = &temp;
	    /* fall through to AF_INET case */
	}
    }
#endif
    if( lookup ) {
	return Address2Name(&sin4->sin_addr, string, len);
    } else {
	strncpy( string, inet_ntoa(sin4->sin_addr), len );
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
            strcpy(ClientHost, "?"); /* so stats generation looks correct. */
	    Printf("%d I can't get your name.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1, TRUE);
	}
        strcpy(ClientHost, "stdin");
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
	        strncpy( ClientHost, ClientIpString, sizeof(ClientHost) - 1 );
                ClientHost[sizeof(ClientHost) - 1] = '\0';
	    }
	} else {
            strncpy( ClientHost, ClientIpString, sizeof(ClientHost) - 1 );
            ClientHost[sizeof(ClientHost) - 1] = '\0';
        }

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
	        strncpy( ServerHost, ServerIpString, sizeof(ServerHost) -1 );
                ServerHost[sizeof(ServerHost) - 1] = '\0';
	    }
	} else {
            strncpy( ServerHost, ServerIpString, sizeof(ServerHost) - 1 );
            ServerHost[sizeof(ServerHost) - 1] = '\0';
        }

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
      int r;

      va_start(args, fmt);
      vsnprintf(buff, sizeof(buff), fmt, args);
      va_end(args);
      TMRstart(TMR_NNTPWRITE);
Again:
      r = SSL_write(tls_conn, buff, strlen(buff));
      switch (SSL_get_error(tls_conn, r)) {
      case SSL_ERROR_NONE:
        break;
      case SSL_ERROR_WANT_WRITE:
        goto Again;
        break;
      case SSL_ERROR_SYSCALL:
        errno = get_last_socket_error();
        break;
      case SSL_ERROR_SSL:
        SSL_shutdown(tls_conn);
        tls_conn = NULL;
        errno = ECONNRESET;
        break;
      case SSL_ERROR_ZERO_RETURN:
        break;
      }
      TMRstop(TMR_NNTPWRITE);
    } else {
      va_start(args, fmt);
      TMRstart(TMR_NNTPWRITE);
      vprintf(fmt, args);
      TMRstop(TMR_NNTPWRITE);
      va_end(args);
    }
#else
      va_start(args, fmt);
      TMRstart(TMR_NNTPWRITE);
      vprintf(fmt, args);
      TMRstop(TMR_NNTPWRITE);
      va_end(args);
#endif
    if (Tracing) {
        oerrno = errno;
        va_start(args, fmt);

        /* Copy output, but strip trailing CR-LF.  Note we're assuming here
           that no output line can ever be longer than 2045 characters. */
        vsnprintf(buff, sizeof(buff), fmt, args);
        va_end(args);
        p = buff + strlen(buff) - 1;
        while (p >= buff && (*p == '\n' || *p == '\r'))
            *p-- = '\0';
        syslog(L_TRACE, "%s > %s", ClientHost, buff);

        errno = oerrno;
    }
}

void
Printf(const char *fmt, ...)
{
    va_list     args;
    char        buff[2048];

#ifdef HAVE_SSL
    if (tls_conn) {
      int r;

      va_start(args, fmt);
      vsnprintf(buff, sizeof(buff), fmt, args);
      va_end(args);
      TMRstart(TMR_NNTPWRITE);
Again:
      r = SSL_write(tls_conn, buff, strlen(buff));
      switch (SSL_get_error(tls_conn, r)) {
      case SSL_ERROR_NONE:
        break;
      case SSL_ERROR_WANT_WRITE:
        goto Again;
        break;
      case SSL_ERROR_SYSCALL:
        errno = get_last_socket_error();
        break;
      case SSL_ERROR_SSL:
        SSL_shutdown(tls_conn);
        tls_conn = NULL;
        errno = ECONNRESET;
        break;
      case SSL_ERROR_ZERO_RETURN:
        break;
      }
      TMRstop(TMR_NNTPWRITE);
    } else {
#endif /* HAVE_SSL */
      va_start(args, fmt);
      TMRstart(TMR_NNTPWRITE);
      vprintf(fmt, args);
      TMRstop(TMR_NNTPWRITE);
      va_end(args);
#ifdef HAVE_SSL
    }
#endif /* HAVE_SSL */
}


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

    val = TRUE;
    if (SMsetup(SM_PREOPEN, (void *)&val) && !SMinit()) {
	syslog(L_NOTICE, "cant initialize storage method, %s", SMerrorstr);
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
    OVextra = overview_extra_fields();
    if (OVextra == NULL) {
	/* overview_extra_fields should already have logged something
	 * useful */
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1, TRUE);
    }
    overhdr_xref = overview_index("Xref", OVextra);
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
    const char *name;
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
#ifdef HAVE_INET6
    char		ListenAddr[INET6_ADDRSTRLEN];
#else
    char		ListenAddr[16];
#endif
    int			lfd, fd;
    socklen_t		clen;
#ifdef HAVE_INET6
    struct sockaddr_storage ssa, csa;
    struct sockaddr_in6	*ssa6 = (struct sockaddr_in6 *) &ssa;
#else
    struct sockaddr_in	ssa, csa;
#endif
    struct sockaddr_in	*ssa4 = (struct sockaddr_in *) &ssa;
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

    int respawn = 0;

    setproctitle_init(argc, argv);

    /* Parse arguments.   Must COPY() optarg if used because setproctitle may
       clobber it! */
    Reject = NULL;
    LLOGenable = FALSE;
    GRPcur = NULL;
    MaxBytesPerSecond = 0;
    strcpy(Username, "unknown");

    /* Set up the pathname, first thing, and teach our error handlers about
       the name of the program. */
    name = argv[0];
    if (name == NULL || *name == '\0')
	name = "nnrpd";
    else {
	const char *p;

	p = strrchr(name, '/');
	if (p != NULL)
	    name = p + 1;
    }
    message_program_name = COPY(name);
    openlog(message_program_name, L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_handlers_die(1, message_log_syslog_crit);
    message_handlers_warn(1, message_log_syslog_warning);
    message_handlers_notice(1, message_log_syslog_notice);

    if (!innconf_read(NULL))
        exit(1);

#ifdef HAVE_SSL
    while ((i = getopt(argc, argv, "c:b:Dfi:I:g:nop:P:Rr:s:tS")) != EOF)
#else
    while ((i = getopt(argc, argv, "c:b:Dfi:I:g:nop:P:Rr:s:t")) != EOF)
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
	    strncpy( ListenAddr, optarg, sizeof(ListenAddr) );
 	    break;
 	case 'D':			/* standalone daemon mode */
 	    DaemonMode = TRUE;
 	    break;
       case 'P':                       /* prespawn count in daemon mode */
	    respawn = atoi(optarg);
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
	case 'I':			/* Instance */
	    NNRPinstance = COPY(optarg);
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

#ifdef HAVE_SSL
        tls_init();
#endif

#ifdef HAVE_INET6
	memset(&ssa, '\0', sizeof(struct sockaddr_in6));
	ssa6->sin6_family = AF_INET6;
	ssa6->sin6_port   = htons(ListenPort);
	if (inet_pton(AF_INET6, ListenAddr, ssa6->sin6_addr.s6_addr) > 0) {
	    if ( (lfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0) {
		syslog(L_FATAL, "can't open socket (%m)");
		exit(1);
	    }
	}
	else {
#endif
	    memset(&ssa, '\0', sizeof(struct sockaddr_in));
	    ssa4->sin_family = AF_INET;
	    ssa4->sin_port   = htons(ListenPort);
	    if (inet_aton(ListenAddr, &ssa4->sin_addr) <= 0 )
		ssa4->sin_addr.s_addr = htonl(INADDR_ANY);
	    if ( (lfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		syslog(L_FATAL, "can't open socket (%m)");
		exit(1);
	    }
#ifdef HAVE_INET6
	}
#endif

	if (setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR,
		       (char *)&one, sizeof(one)) < 0) {
	    syslog(L_FATAL, "can't setsockopt(SO_REUSEADDR) (%m)");
	    exit(1);
	}

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
		syslog(L_FATAL, "nnrpd %s must be owned by %s", innconf->pathrun, NEWSUSER);
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
	    daemonize("/");
	}

	if (ListenPort == NNTP_PORT)
	    strcpy(buff, "nnrpd.pid");
	else
	    snprintf(buff, sizeof(buff), "nnrpd-%d.pid", ListenPort);
        path = concatpath(innconf->pathrun, buff);
        pidfile = fopen(path, "w");
        free(path);
	if (pidfile == NULL) {
	    syslog(L_ERROR, "cannot write %s %m", buff);
            exit(1);
	}
	fprintf(pidfile,"%lu\n", (unsigned long) getpid());
	fclose(pidfile);

	/* Set signal handle to care for dead children */
	if (!respawn)
	    xsignal(SIGCHLD, WaitChild);

	/* Arrange to toggle tracing. */
	(void)xsignal(SIGHUP, ToggleTrace);
 
	setproctitle("accepting connections");
 	
	listen(lfd, 128);	

	if (respawn) {
	    /* pre-forked mode */
	    for (;;) {
		if (respawn > 0) {
		    --respawn;
		    pid = fork();
		    if (pid == 0) {
			do {
			    clen = sizeof(csa);
			    fd = accept(lfd, (struct sockaddr *) &csa, &clen);
			} while (fd < 0);
			break;
		    }
		}
		for (;;) {
		    if (respawn == 0)
			pid = wait(NULL);
		    else
			pid = waitpid(-1, NULL, WNOHANG);
		    if (pid <= 0)
			break;
		    ++respawn;
		}
	    }
	} else {
	    /* fork on demand */
	    do {
		clen = sizeof(csa);
		fd = accept(lfd, (struct sockaddr *) &csa, &clen);
		if (fd < 0)
		    continue;
	    
		for (i = 0; i <= innconf->maxforks && (pid = fork()) < 0; i++) {
		    if (i == innconf->maxforks) {
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
	}

	/* child process starts here */
	setproctitle("connected");
	close(lfd);
	dup2(fd, 0);
	close(fd);
	dup2(0, 1);
	dup2(0, 2);
	SetupDaemon();

	/* if we are a daemon innd didn't make us nice, so be nice kids */
	if (innconf->nicekids) {
	    if (nice(innconf->nicekids) < 0)
		syslog(L_ERROR, "Could not nice child to %ld: %m", innconf->nicekids);
	}

	/* Only automatically reap children in the listening process */
	(void)xsignal(SIGCHLD, SIG_DFL);
 
    } else {
	SetupDaemon();
	/* Arrange to toggle tracing. */
	(void)xsignal(SIGHUP, ToggleTrace);
    }/* DaemonMode */

    /* Setup. */
    TMRinit(TMR_MAX);
    if (GetTimeInfo(&Now) < 0) {
	syslog(L_FATAL, "cant gettimeinfo %m");
	OVclose();
	exit(1);
    }
    STATstart = TIMEINFOasDOUBLE(Now);

#ifdef HAVE_SSL
    ClientSSL = FALSE;
    if (initialSSL) {
        tls_init();
        if (tls_start_servertls(0, 1) == -1) {
            Reply("%d SSL connection failed\r\n", NNTP_STARTTLS_BAD_VAL);
            ExitWithStats(1, FALSE);
        }
        nnrpd_starttls_done = 1;
        ClientSSL = TRUE;
    }
#endif /* HAVE_SSL */

    /* If requested, check the load average. */
    if (innconf->nnrpdloadlimit > 0) {
        double load[1];

        if (getloadavg(load, 1) < 0)
            warn("cannot obtain system load");
        else {
            if ((int)(load[0] + 0.5) > innconf->nnrpdloadlimit) {
                syslog(L_NOTICE, "load %.2f > %ld", load[0], innconf->nnrpdloadlimit);
                Reply("%d load at %.2f, try later\r\n", NNTP_GOODBYE_VAL,
                      load[0]);
                ExitWithStats(1, TRUE);
            }
        }
    }

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
    setproctitle("%s connect", ClientHost);

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
            LocalLogDirName = concatpath(innconf->pathlog, "tracklogs");
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

    line_init(&NNTPline);

    /* Main dispatch loop. */
    for (timeout = innconf->initialtimeout, av = NULL, ac = 0; ;
			timeout = clienttimeout) {
	TMRstart(TMR_NNTPWRITE);
	fflush(stdout);
	TMRstop(TMR_NNTPWRITE);
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
	else {
	    size_t len;
	    const char *p;

	    r = line_read(&NNTPline, timeout, &p, &len);
	    switch (r) {
	    default:
		syslog(L_ERROR, "%s internal %d in main", ClientHost, r);
		/* FALLTHROUGH */
	    case RTtimeout:
		if (timeout < clienttimeout)
		    syslog(L_NOTICE, "%s timeout short", ClientHost);
		else
		    syslog(L_NOTICE, "%s timeout", ClientHost);
		ExitWithStats(1, FALSE);
		break;
	    case RTok:
		if (len < sizeof(buff)) {
		    /* line_read guarantees null termination */
		    memcpy(buff, p, len + 1);
		    /* Do some input processing, check for blank line. */
		    if (Tracing)
			syslog(L_TRACE, "%s < %s", ClientHost, buff);
		    if (buff[0] == '\0')
			continue;
		    ac = Argify(buff, &av);
		    break;
		}
		/* FALLTHROUGH */		
	    case RTlong:
		Reply("%d Line too long\r\n", NNTP_BAD_COMMAND_VAL);
		continue;
	    case RTeof:
		/* Handled below. */
		break;
	    }
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
	setproctitle("%s %s", ClientHost, av[0]);
	(*cp->Function)(ac, av);
	if (PushedBack)
	    break;
	if (PERMaccessconf)
	    clienttimeout = PERMaccessconf->clienttimeout;
	else
	    clienttimeout = innconf->clienttimeout;
    }

    Reply("%s\r\n", NNTP_GOODBYE_ACK);

    ExitWithStats(0, FALSE);
    /* NOTREACHED */
    return 1;
}
