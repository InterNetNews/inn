/*  $Id$
**
**  NNTP server for readers (NNRP) for InterNetNews.
**
**  This server doesn't do any real load-limiting, except for what has
**  proven empirically necessary (i.e. look at GRPscandir).
*/

#include "config.h"
#include "portable/alloca.h"
#include "clibrary.h"
#include "portable/setproctitle.h"
#include "portable/socket.h"
#include "portable/wait.h"
#include <netdb.h>
#include <signal.h>
#include <netinet/tcp.h>

#if HAVE_GETSPNAM
# include <shadow.h>
#endif

#include "inn/innconf.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/network.h"
#include "inn/newsuser.h"
#include "inn/ov.h"
#include "inn/version.h"
#define MAINLINE
#include "nnrpd.h"

#include "tls.h"

#ifdef HAVE_SSL
extern SSL *tls_conn;
bool nnrpd_starttls_done = false;
#endif 


/*
**  If we have getloadavg, include the appropriate header file.  Otherwise,
**  just assume that we always have a load of 0.
*/
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


char	*ACTIVE = NULL;
char	*ACTIVETIMES = NULL;
char	*HISTORY = NULL;
char	*NEWSGROUPS = NULL;
char	*NNRPACCESS = NULL;

static char 	*LocalLogFileName = NULL;
static char 	*LocalLogDirName;

struct history *History;
static double	STATstart;
static double	STATfinish;
static char	*PushedBack;
static sig_atomic_t	ChangeTrace;
bool	DaemonMode = false;
bool	ForeGroundMode = false;
static const char 	*HostErrorStr;
bool GetHostByAddr = true;      /* Formerly DO_NNRP_GETHOSTBYADDR. */
const char *NNRPinstance = "";

#ifdef DO_PERL
bool   PerlLoaded = false;
#endif

#ifdef DO_PYTHON
bool PY_use_dynamic = false;
#endif

static char	CMDfetchhelp[] = "[message-ID|number]";


/*
**  { command base name, function to call, need authentication,
**    min args, max args, help string }
*/
static CMDENT	CMDtable[] = {
    {	"ARTICLE",	CMDfetch,	true,	1,	2,
	CMDfetchhelp },
    {   "AUTHINFO",     CMDauthinfo,    false,  3,      CMDany,
        "USER name|PASS password"
#ifdef HAVE_SASL
        "|SASL mechanism [initial-response]"
#endif
        "|GENERIC program [argument ...]" },
    {	"BODY",		CMDfetch,	true,	1,	2,
	CMDfetchhelp },
    {   "CAPABILITIES", CMDcapabilities,false,  1,      2,
        "[keyword]" },
    {	"DATE",		CMDdate,	false,	1,	1,
	NULL },
    {	"GROUP",	CMDgroup,	true,	2,	2,
	"newsgroup" },
    {   "HDR",          CMDpat  ,       true,   2,      3,
        "header [message-ID|range]" },
    {	"HEAD",		CMDfetch,	true,	1,	2,
	CMDfetchhelp },
    {	"HELP",		CMDhelp,	false,	1,	1,
	NULL },
    {	"IHAVE",	CMDpost,	true,	2,	2,
	"message-ID" },
    {	"LAST",		CMDnextlast,	true,	1,	1,
	NULL },
    {	"LIST",		CMDlist,	true,	1,	3,
	"[ACTIVE [wildmat]|ACTIVE.TIMES [wildmat]|DISTRIB.PATS|DISTRIBUTIONS"
        "|HEADERS [MSGID|RANGE]|MODERATORS|MOTD|NEWSGROUPS [wildmat]"
        "|OVERVIEW.FMT|SUBSCRIPTIONS]" },
    {	"LISTGROUP",	CMDgroup,	true,	1,	3,
	"[newsgroup [range]]" },
    {	"MODE",		CMDmode,	false,	2,	2,
	"READER" },
    {	"NEWGROUPS",	CMDnewgroups,	true,	3,	4,
	"[yy]yymmdd hhmmss [GMT]" },
    {	"NEWNEWS",	CMDnewnews,	true,	4,	5,
	"wildmat [yy]yymmdd hhmmss [GMT]" },
    {	"NEXT",		CMDnextlast,	true,	1,	1,
	NULL },
    {   "OVER",         CMDover,        true,   1,      2,
        "[range]" },
    {	"POST",		CMDpost,	true,	1,	1,
	NULL },
    {   "QUIT",         CMDquit,        false,  1,      1,
        NULL },
    /* SLAVE (which was ill-defined in RFC 977) was removed from the NNTP
     * protocol in RFC 3977. */
    {	"SLAVE",	CMD_unimp,	false,	1,	1,
	NULL },
#ifdef HAVE_SSL
    {   "STARTTLS",     CMDstarttls,    false,  1,      1,
        NULL },
#endif
    {	"STAT",		CMDfetch,	true,	1,	2,
	CMDfetchhelp },
    {	"XGTITLE",	CMDxgtitle,	true,	1,	2,
	"[wildmat]" },
    {	"XHDR",		CMDpat,		true,	2,	3,
	"header [message-ID|range]" },
    {	"XOVER",	CMDover,	true,	1,	2,
	"[range]" },
    {	"XPAT",		CMDpat,		true,	4,	CMDany,
	"header message-ID|range pattern [pattern ...]" },
    {	NULL,           CMD_unimp,      false,  0,      0,
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
    double		usertime;
    double		systime;

    line_free(&NNTPline);
    fflush(stdout);
    STATfinish = TMRnow_double();
    if (GetResourceUsage(&usertime, &systime) < 0) {
	usertime = 0;
	systime = 0;
    }

    GRPreport();
    if (ARTcount)
        syslog(L_NOTICE, "%s exit articles %ld groups %ld", 
    	    Client.host, ARTcount, GRPcount);
    if (POSTreceived || POSTrejected)
	syslog(L_NOTICE, "%s posts received %ld rejected %ld",
	   Client.host, POSTreceived, POSTrejected);
    syslog(L_NOTICE, "%s times user %.3f system %.3f idle %.3f elapsed %.3f",
	Client.host, usertime, systime, IDLEtime, STATfinish - STATstart);
    /* Tracking code - Make entries in the logfile(s) to show that we have
     * finished with this session. */
    if (!readconf && PERMaccessconf && PERMaccessconf->readertrack) {
	syslog(L_NOTICE, "%s Tracking Disabled (%s)", Client.host, Username);
	if (LLOGenable) {
		fprintf(locallog, "%s Tracking Disabled (%s)\n", Client.host, Username);
		fclose(locallog);
		syslog(L_NOTICE,"%s Local Logging ends (%s) %s", Client.host, Username, LocalLogFileName);
	}
    }
    if (ARTget)
        syslog(L_NOTICE, "%s artstats get %ld time %ld size %ld", Client.host,
            ARTget, ARTgettime, ARTgetsize);
    if (!readconf && PERMaccessconf && PERMaccessconf->nnrpdoverstats && OVERcount)
        syslog(L_NOTICE, "%s overstats count %ld hit %ld miss %ld time %ld size %ld dbz %ld seek %ld get %ld artcheck %ld", Client.host,
            OVERcount, OVERhit, OVERmiss, OVERtime, OVERsize, OVERdbz, OVERseek, OVERget, OVERartcheck);

#ifdef HAVE_SSL
     if (tls_conn) {
        SSL_shutdown(tls_conn);
        SSL_free(tls_conn);
        tls_conn = NULL;
     } 
#endif

#ifdef HAVE_SASL
    if (sasl_conn) {
	sasl_dispose(&sasl_conn);
	sasl_conn = NULL;
	sasl_ssf = 0;
	sasl_maxout = NNTP_MAXLEN_COMMAND;
    }
    sasl_done();
#endif /* HAVE_SASL */

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
    PY_close_python();
#endif /* DO_PYTHON */

    if (History)
	HISclose(History);

    if (innconf->timer != 0) {
        TMRsummary(Client.host, timer_name);
        TMRfree();
    }

    if (LocalLogFileName != NULL)
	free(LocalLogFileName);
    closelog();
    exit(x);
}


/*
**  The HELP command.
*/
void
CMDhelp(int ac UNUSED, char *av[] UNUSED)
{
    CMDENT	*cp;
    char	*p, *q;
    static const char *newsmaster = NEWSMASTER;

    Reply("%d Legal commands\r\n", NNTP_INFO_HELP);
    for (cp = CMDtable; cp->Name; cp++) {
        if (cp->Function == CMD_unimp)
            continue;
	if (cp->Help == NULL)
	    Printf("  %s\r\n", cp->Name);
	else
	    Printf("  %s %s\r\n", cp->Name, cp->Help);
    }
    if (PERMaccessconf && (VirtualPathlen > 0)) {
	if (PERMaccessconf->newsmaster) {
	    if (strchr(PERMaccessconf->newsmaster, '@') == NULL) {
		Printf("Report problems to <%s@%s>.\r\n",
		    PERMaccessconf->newsmaster, PERMaccessconf->domain);
	    } else {
		Printf("Report problems to <%s>.\r\n",
		    PERMaccessconf->newsmaster);
	    }
	} else {
	    /* Sigh, pickup from newsmaster anyway. */
	    if ((p = strchr(newsmaster, '@')) == NULL)
		Printf("Report problems to <%s@%s>.\r\n",
		    newsmaster, PERMaccessconf->domain);
	    else {
                q = xstrndup(newsmaster, p - newsmaster);
		Printf("Report problems to <%s@%s>.\r\n",
		    q, PERMaccessconf->domain);
		free(q);
	    }
	}
    } else {
	if (strchr(newsmaster, '@') == NULL)
	    Printf("Report problems to <%s@%s>.\r\n",
		newsmaster, innconf->fromhost);
	else
	    Printf("Report problems to <%s>.\r\n",
		newsmaster);
    }
    Printf(".\r\n");
}


/*
**  The CAPABILITIES command.
**
**  nnrpd does not advertise the MODE-READER capability; only innd may
**  advertise it.
*/
void
CMDcapabilities(int ac, char *av[])
{
    if (ac == 2 && !IsValidKeyword(av[1])) {
        Reply("%d Syntax error\r\n", NNTP_ERR_SYNTAX);
        return;
    }

    Reply("%d Capability list:\r\n", NNTP_INFO_CAPABILITIES);

    Printf("VERSION 2\r\n");

    Printf("IMPLEMENTATION %s\r\n", INN_VERSION_STRING);

#ifdef HAVE_SASL
    const char *mechlist = NULL;

    /* Check for available SASL mechanisms.
     * Start the string with a space for the strstr() calls afterwards. */
    sasl_listmech(sasl_conn, NULL, " ", " ", "", &mechlist, NULL, NULL);
#endif

    /* The client is not already authenticated. */
    if ((!PERMauthorized || PERMneedauth || PERMcanauthenticate)) {
        Printf("AUTHINFO");

        /* No arguments if the server does not permit any authentication commands
         * in its current state. */
        if (PERMcanauthenticate) {
#ifdef HAVE_SSL
            if (PERMcanauthenticatewithoutSSL || nnrpd_starttls_done) {
#endif
                /* AUTHINFO USER is advertised only if a TLS layer is active,
                 * if compiled with TLS support. */
                Printf(" USER");
#ifdef HAVE_SSL
            } else {
#ifdef HAVE_SASL
                /* Remove unsecure PLAIN, LOGIN and EXTERNAL SASL mechanisms,
                 * if compiled with TLS support and a TLS layer is not active. */
                if (mechlist != NULL) {
                    char *p;

                    if ((p = strstr(mechlist, " PLAIN")) != NULL
                        && (p[6] == '\0' || p[6] == ' ')) {
                        memmove(p, p+6, strlen(p)-5);
                    }
                    if ((p = strstr(mechlist, " LOGIN")) != NULL
                        && (p[6] == '\0' || p[6] == ' ')) {
                        memmove(p, p+6, strlen(p)-5);
                    }
                    if ((p = strstr(mechlist, " EXTERNAL")) != NULL
                        && (p[9] == '\0' || p[9] == ' ')) {
                        memmove(p, p+9, strlen(p)-8);
                    }
                }
#endif /* HAVE_SASL */
            }
#endif /* HAVE_SSL */
#ifdef HAVE_SASL
            /* Check whether at least one SASL mechanism is available. */
            if (mechlist != NULL && strlen(mechlist) > 2) {
                Printf(" SASL");
            }
#endif
        }
        Printf("\r\n");
    }

    if (PERMcanread) {
        Printf("HDR\r\n");
    }
    
    if (PERMaccessconf->allowihave && PERMcanpost) {
        Printf("IHAVE\r\n");
    }

    Printf("LIST ACTIVE ACTIVE.TIMES DISTRIB.PATS HEADERS NEWSGROUPS OVERVIEW.FMT\r\n");
    
    if (PERMaccessconf->allownewnews && PERMcanread) {
        Printf("NEWNEWS\r\n");
    }

    if (PERMcanread) {
        Printf("OVER\r\n");
    }

    if (PERMcanpost) {
        Printf("POST\r\n");
    }

    Printf("READER\r\n");

#ifdef HAVE_SASL
    /* Check whether at least one SASL mechanism is available. */
    if (mechlist != NULL && strlen(mechlist) > 2) {
        Printf("SASL%s\r\n", mechlist);
    }
#endif

#ifdef HAVE_SSL
    /* A TLS layer is not active and the client is not already authenticated. */
    if (!nnrpd_starttls_done
        && (!PERMauthorized || PERMneedauth || PERMcanauthenticate)) {
        Printf("STARTTLS\r\n");
    }
#endif

    Printf(".\r\n");
}


/*
**  Catch-all for unimplemented functions.
*/
void
CMD_unimp(int ac UNUSED, char *av[])
{
    Reply("%d \"%s\" not implemented; try \"HELP\"\r\n",
        NNTP_ERR_COMMAND, av[0]);
}


/*
**  The QUIT command.
*/
void
CMDquit(int ac UNUSED, char *av[] UNUSED)
{
    Reply("%d Bye!\r\n", NNTP_OK_QUIT);
    ExitWithStats(0, false);
}


/*
**  Convert an address to a hostname.  Don't trust the reverse lookup
**  since anyone can fake reverse DNS entries.
*/
static bool
Address2Name(struct sockaddr *sa, char *hostname, size_t size)
{
    static const char MISMATCH[] = "reverse lookup validation failed";
    struct addrinfo hints, *ai, *host;
    char *p;
    int ret;
    bool valid = false;

    /* Get the official hostname, store it away. */
    ret = getnameinfo(sa, SA_LEN(sa), hostname, size, NULL, 0, NI_NAMEREQD);
    if (ret != 0) {
	HostErrorStr = gai_strerror(ret);
	return false;
    }

    /* Get addresses for this host. */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = NETWORK_AF_HINT;
    hints.ai_socktype = SOCK_STREAM;
    ret = getaddrinfo(hostname, NULL, &hints, &ai);
    if (ret != 0) {
	HostErrorStr = gai_strerror(ret);
	return false;
    }

    /* Make sure one of those addresses is the address we got. */
    for (host = ai; host != NULL; host = host->ai_next)
        if (network_sockaddr_equal(sa, host->ai_addr)) {
            valid = true;
            break;
        }
    freeaddrinfo(ai);

    /* Lowercase the returned name since wildmats are case-sensitive. */
    if (valid) {
        for (p = hostname; *p != '\0'; p++)
            if (CTYPE(isupper, *p))
                *p = tolower((int) *p);
        return true;
    } else {
	HostErrorStr = MISMATCH;
	return false;
    }
}


/*
**  Determine access rights of the client.
*/
static void
StartConnection(void)
{
    static const char *default_host_error = "unknown error";
    struct sockaddr_storage ssc, sss;
    struct sockaddr *sac = (struct sockaddr *) &ssc;
    struct sockaddr *sas = (struct sockaddr *) &sss;
    socklen_t length;
    size_t size;
    int nodelay = 1;

    memset(&Client, 0, sizeof(Client));
    strlcpy(Client.host, "?", sizeof(Client.host));

    /* Get the peer's name. */
    length = sizeof(ssc);
    if (getpeername(STDIN_FILENO, sac, &length) < 0) {
        if (!isatty(STDIN_FILENO)) {
	    sysnotice("? can't getpeername");
	    Reply("%d Can't get your name.  Goodbye!\r\n", NNTP_ERR_ACCESS);
	    ExitWithStats(1, true);
	}
        strlcpy(Client.host, "stdin", sizeof(Client.host));
    } else {
	/* Figure out client's IP address/hostname. */
	HostErrorStr = default_host_error;
        if (!network_sockaddr_sprint(Client.ip, sizeof(Client.ip), sac)) {
            notice("? can't get client numeric address: %s", HostErrorStr);
            Reply("%d Can't get your numeric address.  Goodbye!\r\n",
                   NNTP_ERR_ACCESS);
	    ExitWithStats(1, true);
	}
	if (GetHostByAddr) {
            HostErrorStr = default_host_error;
            if (!Address2Name(sac, Client.host, sizeof(Client.host))) {
                notice("? reverse lookup for %s failed: %s -- using IP"
                       " address for access", Client.ip, HostErrorStr);
	        strlcpy(Client.host, Client.ip, sizeof(Client.host));
	    }
	} else {
            strlcpy(Client.host, Client.ip, sizeof(Client.host));
        }

	/* Figure out server's IP address/hostname. */
        length = sizeof(sss);
	if (getsockname(STDIN_FILENO, sas, &length) < 0) {
	    sysnotice("%s can't getsockname", Client.host);
	    Reply("%d Can't figure out where you connected to.  Goodbye!\r\n",
                   NNTP_ERR_ACCESS);
	    ExitWithStats(1, true);
	}
	HostErrorStr = default_host_error;
        size = sizeof(Client.serverip);
        if (!network_sockaddr_sprint(Client.serverip, size, sas)) {
            notice("? can't get server numeric address: %s", HostErrorStr);
            Reply("%d Can't get my own numeric address.  Goodbye!\r\n",
                   NNTP_ERR_ACCESS);
	    ExitWithStats(1, true);
	}
	if (GetHostByAddr) {
	    HostErrorStr = default_host_error;
            size = sizeof(Client.serverhost);
            if (!Address2Name(sas, Client.serverhost, size)) {
                notice("? reverse lookup for %s failed: %s -- using IP"
                       " address for access", Client.serverip, HostErrorStr);
	        strlcpy(Client.serverhost, Client.serverip,
                        sizeof(Client.serverhost));
	    }
	} else {
            strlcpy(Client.serverhost, Client.serverip,
                    sizeof(Client.serverhost));
        }

	/* Get port numbers. */
        Client.port = network_sockaddr_port(sac);
        Client.serverport = network_sockaddr_port(sas);
    }

    /* Setting TCP_NODELAY to nnrpd fixes a problem of slow downloading
     * of overviews and slow answers on some architectures (like BSD/OS). */
    setsockopt(STDIN_FILENO, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    notice("%s (%s) connect", Client.host, Client.ip);

    PERMgetaccess(NNRPACCESS);
    PERMgetpermissions();
}


/*
**  Write a buffer, via SASL security layer and/or TLS if necessary.
*/
void
write_buffer(const char *buff, ssize_t len)
{
    const char *p;
    ssize_t n;

    TMRstart(TMR_NNTPWRITE);

    p = buff;
    while (len > 0) {
	const char *out;
	unsigned outlen;

#ifdef HAVE_SASL
	if (sasl_conn && sasl_ssf) {
            int r;

	    /* Can only encode as much as the client can handle at one time. */
	    n = (len > sasl_maxout) ? sasl_maxout : len;
	    if ((r = sasl_encode(sasl_conn, p, n, &out, &outlen)) != SASL_OK) {
		sysnotice("sasl_encode() failed: %s",
			  sasl_errstring(r, NULL, NULL));
		return;
	    }
	} else
#endif /* HAVE_SASL */
	{
	    /* Output the entire unencoded string. */
	    n = len;
	    out = buff;
	    outlen = len;
	}

	len -= n;
	p += n;

#ifdef HAVE_SSL
	if (tls_conn) {
            int r;

Again:
	    r = SSL_write(tls_conn, out, outlen);
	    switch (SSL_get_error(tls_conn, r)) {
	    case SSL_ERROR_NONE:
	    case SSL_ERROR_SYSCALL:
		break;
	    case SSL_ERROR_WANT_WRITE:
		goto Again;
		break;
	    case SSL_ERROR_SSL:
		SSL_shutdown(tls_conn);
		tls_conn = NULL;
		errno = ECONNRESET;
		break;
	    case SSL_ERROR_ZERO_RETURN:
		break;
	    }
	} else
#endif /* HAVE_SSL */
	    do {
		n = write(STDIN_FILENO, out, outlen);
	    } while (n == -1 && errno == EINTR);
    }

    TMRstop(TMR_NNTPWRITE);
}


/*
**  Send formatted output, possibly with debugging output.
*/
static void
VPrintf(const char *fmt, va_list args, int dotrace)
{
    char buff[2048], *p;
    ssize_t len;

    len = vsnprintf(buff, sizeof(buff), fmt, args);
    write_buffer(buff, len);

    if (dotrace && Tracing) {
	int oerrno = errno;

        /* Copy output, but strip trailing CR-LF.  Note we're assuming here
         * that no output line can ever be longer than 2045 characters. */
        p = buff + strlen(buff) - 1;
        while (p >= buff && (*p == '\n' || *p == '\r'))
            *p-- = '\0';
        syslog(L_TRACE, "%s > %s", Client.host, buff);

        errno = oerrno;
    }
}


/*
**  Send a reply, possibly with debugging output.
**  Reply() is used for answers which can possibly be traced (response codes).
**  Printf() is used in other cases.
*/
void
Reply(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    VPrintf(fmt, args, 1);
    va_end(args);
}

void
Printf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    VPrintf(fmt, args, 0);
    va_end(args);
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
    ChangeTrace = true;
#ifndef HAVE_SIGACTION
    xsignal(s, ToggleTrace);
#endif
}

/*
**  Got a SIGPIPE; exit cleanly.
*/
static RETSIGTYPE
CatchPipe(int s UNUSED)
{
    ExitWithStats(0, false);
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


static void
SetupDaemon(void)
{
    bool                val;

    val = true;
    if (SMsetup(SM_PREOPEN, (void *)&val) && !SMinit()) {
	syslog(L_NOTICE, "can't initialize storage method, %s", SMerrorstr);
	Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
	ExitWithStats(1, true);
    }
    OVextra = overview_extra_fields();
    if (OVextra == NULL) {
	/* Overview_extra_fields should already have logged something
	 * useful. */
	Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
	ExitWithStats(1, true);
    }
    overhdr_xref = overview_index("Xref", OVextra);
    if (!OVopen(OV_READ)) {
	/* This shouldn't really happen. */
	syslog(L_NOTICE, "can't open overview %m");
	Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
	ExitWithStats(1, true);
    }
    if (!OVctl(OVCACHEKEEP, &val)) {
	syslog(L_NOTICE, "can't enable overview cache %m");
	Reply("%d NNTP server unavailable.  Try later!\r\n", NNTP_FAIL_TERMINATING);
	ExitWithStats(1, true);
    }
}


/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    fprintf(stderr, "Usage error.\n");
    exit(1);
}


int
main(int argc, char *argv[])
{
    const char *name;
    CMDENT		*cp;
    char		buff[NNTP_MAXLEN_COMMAND];
    char		**av;
    int			ac;
    READTYPE		r;
    int			i;
    char                **v;
    char		*Reject;
    int			timeout;
    unsigned int	vid=0; 
    int 		count=123456789;
    struct		timeval tv;
    unsigned short	ListenPort = NNTP_PORT;
    char                *ListenAddr = NULL;
    char                *ListenAddr6 = NULL;
    int			lfd, fd;
    pid_t		pid = -1;
    FILE                *pidfile;
    int			clienttimeout;
    char		*ConfFile = NULL;
    char                *path;
    bool                validcommandtoolong;

    int respawn = 0;

    setproctitle_init(argc, argv);

    /* Parse arguments.  Must xstrdup() optarg if used because setproctitle
     * may clobber it! */
    Reject = NULL;
    LLOGenable = false;
    GRPcur = NULL;
    MaxBytesPerSecond = 0;
    strlcpy(Username, "unknown", sizeof(Username));

    /* Set up the pathname, first thing, and teach our error handlers about
     * the name of the program. */
    name = argv[0];
    if (name == NULL || *name == '\0')
	name = "nnrpd";
    else {
	const char *p;

        /* We take the last "/" in the path. */
	p = strrchr(name, '/');
	if (p != NULL)
	    name = p + 1;
    }
    message_program_name = xstrdup(name);
    openlog(message_program_name, L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
    message_handlers_die(1, message_log_syslog_crit);
    message_handlers_warn(1, message_log_syslog_warning);
    message_handlers_notice(1, message_log_syslog_notice);

    if (!innconf_read(NULL))
        exit(1);

#ifdef HAVE_SASL
    if (sasl_server_init(sasl_callbacks, "INN") != SASL_OK) {
	syslog(L_FATAL, "sasl_server_init() failed");
	exit(1);
    }
#endif /* HAVE_SASL */

#ifdef HAVE_SSL
    while ((i = getopt(argc, argv, "6:c:b:Dfi:I:nop:P:r:s:tS")) != EOF)
#else
    while ((i = getopt(argc, argv, "6:c:b:Dfi:I:nop:P:r:s:t")) != EOF)
#endif /* HAVE_SSL */
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
        case '6':                       /* Bind to a certain IPv6 address. */
            ListenAddr6 = xstrdup(optarg);
            break;
 	case 'b':			/* Bind to a certain IPv4 address. */
            ListenAddr = xstrdup(optarg);
 	    break;
	case 'c':                       /* Use alternate readers.conf. */
	    ConfFile = concatpath(innconf->pathetc, optarg);
	    break;
 	case 'D':			/* Standalone daemon mode. */
 	    DaemonMode = true;
 	    break;
        case 'P':                       /* Prespawn count in daemon mode. */
	    respawn = atoi(optarg);
	    break;
 	case 'f':			/* Don't fork on daemon mode. */
 	    ForeGroundMode = true;
 	    break;
	case 'i':			/* Initial command. */
	    PushedBack = xstrdup(optarg);
	    break;
	case 'I':			/* Instance. */
	    NNRPinstance = xstrdup(optarg);
	    break;
	case 'n':			/* No DNS lookups. */
	    GetHostByAddr = false;
	    break;
	case 'o':
	    Offlinepost = true;		/* Offline posting only. */
	    break;
 	case 'p':			/* TCP port for daemon mode. */
 	    ListenPort = atoi(optarg);
 	    break;
	case 'r':			/* Reject connection message. */
	    Reject = xstrdup(optarg);
	    break;
	case 's':			/* Unused title string. */
	    break;
	case 't':			/* Tracing. */
	    Tracing = true;
	    break;
#ifdef HAVE_SSL
	case 'S':			/* Force SSL negotiation. */
	    initialSSL = true;
	    break;
#endif /* HAVE_SSL */
	}
    argc -= optind;
    if (argc)
	Usage();
    if (ListenAddr != NULL && ListenAddr6 != NULL)
        die("-6 and -b may not both be given");

    /* Make other processes happier if someone is reading.  This allows other
     * processes like overchan to keep up when there are lots of readers.
     * Note that this is cumulative with nicekids. */
    if (innconf->nicennrpd > 0)
	nice(innconf->nicennrpd);

    HISTORY = concatpath(innconf->pathdb, INN_PATH_HISTORY);
    ACTIVE = concatpath(innconf->pathdb, INN_PATH_ACTIVE);
    ACTIVETIMES = concatpath(innconf->pathdb, INN_PATH_ACTIVETIMES);
    NEWSGROUPS = concatpath(innconf->pathdb, INN_PATH_NEWSGROUPS);
    if(ConfFile)
        NNRPACCESS = ConfFile;
    else
        NNRPACCESS = concatpath(innconf->pathetc,INN_PATH_NNRPACCESS);


    if (DaemonMode) {
        if (ListenAddr6 != NULL)
            lfd = network_bind_ipv6(ListenAddr6, ListenPort);
        else if (ListenAddr != NULL)
            lfd = network_bind_ipv4(ListenAddr, ListenPort);
        else
            lfd = network_bind_ipv4("0.0.0.0", ListenPort);
        if (lfd < 0)
            die("can't bind to any addresses");

        /* If started as root, switch to news uid.  Unlike other parts of INN, we
         * don't die if we can't become the news user.  As long as we're not
         * running as root, everything's fine; it's okay to write the things we
         * write as a member of the news group. */
        if (getuid() == 0) {
            ensure_news_user_grp(true, true);
        }

	/* Detach. */
	if (!ForeGroundMode) {
	    daemonize("/");
	}

	if (ListenPort == NNTP_PORT)
	    strlcpy(buff, "nnrpd.pid", sizeof(buff));
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

	/* Set signal handle to care for dead children. */
	if (!respawn)
	    xsignal(SIGCHLD, WaitChild);

	/* Arrange to toggle tracing. */
	xsignal(SIGHUP, ToggleTrace);
 
	setproctitle("accepting connections");
 	
	listen(lfd, 128);	

	if (respawn) {
	    /* Pre-forked mode. */
	    for (;;) {
		if (respawn > 0) {
		    --respawn;
		    pid = fork();
		    if (pid == 0) {
			do {
			    fd = accept(lfd, NULL, NULL);
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
	    /* Fork on demand. */
	    do {
		fd = accept(lfd, NULL, NULL);
		if (fd < 0)
		    continue;
	    
		for (i = 0; i <= innconf->maxforks && (pid = fork()) < 0; i++) {
		    if (i == innconf->maxforks) {
			syslog(L_FATAL, "can't fork (dropping connection): %m");
			continue;
		    }
		    syslog(L_NOTICE, "can't fork (waiting): %m");
		    sleep(1);
		}
		if (ChangeTrace) {
		    Tracing = Tracing ? false : true;
		    syslog(L_TRACE, "trace %sabled", Tracing ? "en" : "dis");
		    ChangeTrace = false;
		}
		if (pid != 0)
		    close(fd);
	    } while (pid != 0);
	}

	/* Child process starts here. */
	setproctitle("connected");
	close(lfd);
	dup2(fd, 0);
	close(fd);
	dup2(0, 1);
	dup2(0, 2);
        if (innconf->timer != 0)
            TMRinit(TMR_MAX);
        STATstart = TMRnow_double();
	SetupDaemon();

	/* If we are a daemon, innd didn't make us nice, so be nice kids. */
	if (innconf->nicekids) {
	    if (nice(innconf->nicekids) < 0)
		syslog(L_ERROR, "Could not nice child to %ld: %m", innconf->nicekids);
	}

	/* Only automatically reap children in the listening process. */
	xsignal(SIGCHLD, SIG_DFL);
 
    } else {
        if (innconf->timer)
            TMRinit(TMR_MAX);
        STATstart = TMRnow_double();
	SetupDaemon();
	/* Arrange to toggle tracing. */
	xsignal(SIGHUP, ToggleTrace);
    } /* DaemonMode */

#ifdef HAVE_SSL
    if (initialSSL) {
        tls_init();
        if (tls_start_servertls(0, 1) == -1) {
            Reply("%d SSL connection failed\r\n", NNTP_FAIL_TERMINATING);
            ExitWithStats(1, false);
        }
        nnrpd_starttls_done = true;
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
                Reply("%d load at %.2f, try later\r\n", NNTP_FAIL_TERMINATING,
                      load[0]);
                ExitWithStats(1, true);
            }
        }
    }

    /* Catch SIGPIPE so that we can exit out of long write loops. */
    xsignal(SIGPIPE, CatchPipe);

    /* Get permissions and see if we can talk to this client. */
    StartConnection();
    if (!PERMcanread && !PERMcanpost && !PERMneedauth) {
	syslog(L_NOTICE, "%s no_permission", Client.host);
	Reply("%d You have no permission to talk.  Goodbye!\r\n",
	       NNTP_ERR_ACCESS);
	ExitWithStats(1, false);
    }

    /* Proceed with initialization. */
    setproctitle("%s connect", Client.host);

    /* Were we told to reject connections? */
    if (Reject) {
	syslog(L_NOTICE, "%s rejected %s", Client.host, Reject);
	Reply("%d %s\r\n", NNTP_FAIL_TERMINATING, Reject);
	ExitWithStats(0, false);
    }

    if (PERMaccessconf) {
	if (PERMaccessconf->readertrack)
	    PERMaccessconf->readertrack =
                TrackClient(Client.host, Username, sizeof(Username));
    } else {
	if (innconf->readertrack)
	    innconf->readertrack =
                TrackClient(Client.host, Username, sizeof(Username));
    }

    if ((PERMaccessconf && PERMaccessconf->readertrack)
        || (!PERMaccessconf && innconf->readertrack)) {
	int len;
	syslog(L_NOTICE, "%s Tracking Enabled (%s)", Client.host, Username);
	pid=getpid();
	gettimeofday(&tv,NULL);
	count += pid;
	vid = tv.tv_sec ^ tv.tv_usec ^ pid ^ count;
	len = strlen("innconf->pathlog") + strlen("/tracklogs/log-") + BUFSIZ;
	LocalLogFileName = xmalloc(len);
	sprintf(LocalLogFileName, "%s/tracklogs/log-%d", innconf->pathlog, vid);
	if ((locallog = fopen(LocalLogFileName, "w")) == NULL) {
            LocalLogDirName = concatpath(innconf->pathlog, "tracklogs");
	    MakeDirectory(LocalLogDirName, false);
	    free(LocalLogDirName);
	}
	if (locallog == NULL && (locallog = fopen(LocalLogFileName, "w")) == NULL) {
	    syslog(L_ERROR, "%s Local Logging failed (%s) %s: %m", Client.host, Username, LocalLogFileName);
	} else {
	    syslog(L_NOTICE, "%s Local Logging begins (%s) %s",Client.host, Username, LocalLogFileName);
	    fprintf(locallog, "%s Tracking Enabled (%s)\n", Client.host, Username);
	    fflush(locallog);
	    LLOGenable = true;
	}
    }

#ifdef HAVE_SASL
    SASLnewserver();
#endif /* HAVE_SASL */

    if (PERMaccessconf) {
        Reply("%d %s InterNetNews NNRP server %s ready (%s)\r\n",
	   (PERMcanpost || (PERMcanauthenticate && PERMcanpostgreeting)) ?
               NNTP_OK_BANNER_POST : NNTP_OK_BANNER_NOPOST,
           PERMaccessconf->pathhost, INN_VERSION_STRING,
	   (!PERMneedauth && PERMcanpost) ? "posting ok" : "no posting");
	clienttimeout = PERMaccessconf->clienttimeout;
    } else {
        Reply("%d %s InterNetNews NNRP server %s ready (%s)\r\n",
	   (PERMcanpost || (PERMcanauthenticate && PERMcanpostgreeting)) ?
               NNTP_OK_BANNER_POST : NNTP_OK_BANNER_NOPOST,
           innconf->pathhost, INN_VERSION_STRING,
	   (!PERMneedauth && PERMcanpost) ? "posting ok" : "no posting");
	clienttimeout = innconf->clienttimeout;
    }

    line_init(&NNTPline);

    /* Main dispatch loop. */
    for (timeout = innconf->initialtimeout, av = NULL, ac = 0; ;
			timeout = clienttimeout) {
        alloca_free();
	TMRstart(TMR_NNTPWRITE);
	fflush(stdout);
	TMRstop(TMR_NNTPWRITE);
	if (ChangeTrace) {
	    Tracing = Tracing ? false : true;
	    syslog(L_TRACE, "trace %sabled", Tracing ? "en" : "dis");
	    ChangeTrace = false;
	}
	if (PushedBack) {
	    if (PushedBack[0] == '\0')
		continue;
	    if (Tracing)
		syslog(L_TRACE, "%s < %s", Client.host, PushedBack);
	    ac = Argify(PushedBack, &av);
	    r = RTok;
	}
	else {
	    size_t len;
            size_t lenstripped = 0;
	    const char *p;

	    r = line_read(&NNTPline, timeout, &p, &len, &lenstripped);
	    switch (r) {
	    default:
		syslog(L_ERROR, "%s internal %d in main", Client.host, r);
		/* FALLTHROUGH */
	    case RTtimeout:
		if (timeout < clienttimeout)
		    syslog(L_NOTICE, "%s timeout short", Client.host);
		else
		    syslog(L_NOTICE, "%s timeout", Client.host);
		ExitWithStats(1, false);
		break;
	    case RTok:
                /* len does not count CRLF. */
		if (len + lenstripped <= sizeof(buff)) {
		    /* line_read guarantees null termination. */
		    memcpy(buff, p, len + 1);
		    /* Do some input processing, check for blank line. */
                    if (buff[0] != '\0')
                        ac = Argify(buff, &av);
		    if (Tracing) {
                        /* Do not log passwords if AUTHINFO PASS,
                         * AUTHINFO SASL PLAIN or AUTHINFO SASL EXTERNAL
                         * are used. 
                         * AUTHINFO SASL LOGIN does not use an initial response;
                         * therefore, there is nothing to hide here. */
                        if (ac > 2 && strcasecmp(av[0], "AUTHINFO") == 0
                            && (strcasecmp(av[1], "PASS") == 0
                                || (ac > 3 && strcasecmp(av[1], "SASL") == 0
                                    && (strcasecmp(av[2], "PLAIN") == 0
                                        || strcasecmp(av[2], "EXTERNAL") == 0))))
                            syslog(L_TRACE, "%s < %s %s %s ********", Client.host,
                                   av[0], av[1],
                                   strcasecmp(av[1], "SASL") == 0 ? av[2] : "");
                        else
                            syslog(L_TRACE, "%s < %s", Client.host, buff);
                    }
		    if (buff[0] == '\0')
			continue;
		    break;
		}
		/* FALLTHROUGH */		
	    case RTlong:
                /* The line is too long but we have to make sure that
                 * no recognized command has been sent. */
                validcommandtoolong = false;
                for (cp = CMDtable; cp->Name; cp++) {
                    if (strncasecmp(cp->Name, p, strlen(cp->Name)) == 0) {
                        if (p[strlen(cp->Name)] == ' ') {
                            validcommandtoolong = true;
                            break;
                        }
                    }
                }
                Reply("%d Line too long\r\n",
                      validcommandtoolong ? NNTP_ERR_SYNTAX : NNTP_ERR_COMMAND);
		continue;
	    case RTeof:
		/* Handled below. */
		break;
	    }
	}
	/* Client gone? */
	if (r == RTeof)
	    break;
	if (ac == 0)
	    break;

	/* Find command. */
	for (cp = CMDtable; cp->Name; cp++)
	    if (strcasecmp(cp->Name, av[0]) == 0)
		break;

        /* If no command has been recognized. */
	if (cp->Name == NULL) {
	    if ((int)strlen(buff) > 40)
		syslog(L_NOTICE, "%s unrecognized %.40s...", Client.host, buff);
	    else
		syslog(L_NOTICE, "%s unrecognized %s", Client.host, buff);
	    Reply("%d What?\r\n", NNTP_ERR_COMMAND);
	    continue;
	}

        /* Check whether all arguments do not exceed their allowed size. */
        if (ac > 1) {
            validcommandtoolong = false;
            for (v = av; *v; v++)
                if (strlen(*v) > NNTP_MAXLEN_ARG) {
                    validcommandtoolong = true;
                    Reply("%d Argument too long\r\n", NNTP_ERR_SYNTAX);
                    break;
                }
            if (validcommandtoolong)
                continue;
        }

	/* Check usage. */
	if ((cp->Minac != CMDany && ac < cp->Minac)
	 || (cp->Maxac != CMDany && ac > cp->Maxac)) {
	    Reply("%d Syntax is:  %s %s\r\n",
		NNTP_ERR_SYNTAX, cp->Name, cp->Help ? cp->Help : "(no argument allowed)");
	    continue;
	}

	/* Check permissions and dispatch. */
	if (cp->Needauth && PERMneedauth) {
	    Reply("%d Authentication required for command\r\n",
		NNTP_FAIL_AUTH_NEEDED);
	    continue;
	}
	setproctitle("%s %s", Client.host, av[0]);

        (*cp->Function)(ac, av);

        if (PushedBack)
	    break;
	if (PERMaccessconf)
	    clienttimeout = PERMaccessconf->clienttimeout;
	else
	    clienttimeout = innconf->clienttimeout;
    }

    CMDquit(ac, av);

    /* NOTREACHED */
    return 1;
}
