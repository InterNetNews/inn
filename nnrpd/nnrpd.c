/*  $Revision$
**
**  NNTP server for readers (NNRP) for InterNetNews.
**  This server doesn't do any real load-limiting, except for what has
**  proven empirically necesary (i.e., look at GRPscandir).
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include "configdata.h"
#include "clibrary.h"
#define MAINLINE
#include "nnrpd.h"
#include <netdb.h>
#include <pwd.h>
#if	defined(HPUX)
#include <sys/pstat.h>
#endif	/* defined(HPUX) */


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
    /* Default permission -- change with adb. */
BOOL	PERMdefault = FALSE;
BOOL	ForceReadOnly = FALSE;
char 	LocalLogFileName[256];

STATIC double	STATstart;
STATIC double	STATfinish;
STATIC char	*PushedBack;
#if	!defined(HPUX)
STATIC char	*TITLEstart;
STATIC char	*TITLEend;
#endif	/* !defined(HPUX) */
STATIC SIGVAR	ChangeTrace;
BOOL	DaemonMode = FALSE;


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

extern int RARTenable;
extern int LLOGenable;
extern int TrackClient();

STATIC char	CMDfetchhelp[] = "[MessageID|Number]";

STATIC CMDENT	CMDtable[] = {
    {	"authinfo",	CMDauthinfo,	FALSE,	3,	CMDany,
	"user Name|pass Password|generic <prog> <args>" },
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
	"yymmdd hhmmss [\"GMT\"] [<distributions>]" },
    {	"newnews",	CMDnewnews,	TRUE,	4,	6,
	"newsgroups yymmdd hhmmss [\"GMT\"] [<distributions>]" },
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
ExitWithStats(x)
    int			x;
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
    if (RARTenable) {
	syslog(L_NOTICE, "%s Tracking Disabled (%s)", ClientHost, Username);
	if (LLOGenable) {
		fprintf(locallog, "%s Tracking Disabled (%s)\n", ClientHost, Username);
		fclose(locallog);
		syslog(L_NOTICE,"%s Local Logging ends (%s) %s",ClientHost, Username, LocalLogFileName);
	}
    }
    if (ARTget)
        syslog(L_NOTICE, "%s artstats get %d time %d size %d", ClientHost,
            ARTget, ARTgettime, ARTgetsize);
    if (OVERcount)
        syslog(L_NOTICE, "%s overstats count %d hit %d miss %d time %d size %d read %d dbz %d seek %d get %d", ClientHost,
            OVERcount, OVERhit, OVERmiss, OVERtime, OVERsize, OVERread, OVERdbz, OVERseek, OVERget);

     if (DaemonMode) {
     	shutdown(STDIN, 2);
     	shutdown(STDOUT, 2);
     	shutdown(STDERR, 2);
 	close(STDIN);    	
 	close(STDOUT);
 	close(STDERR);    	
     }
            
    exit(x);
}


/*
**  The "help" command.
*/
/* ARGSUSED0 */
STATIC FUNCTYPE
CMDhelp(ac, av)
    int		ac;
    char	*av[];
{
    CMDENT	*cp;

    Reply("%s\r\n", NNTP_HELP_FOLLOWS);
    for (cp = CMDtable; cp->Name; cp++)
	if (cp->Help == NULL)
	    Printf("  %s\r\n", cp->Name);
	else
	    Printf("  %s %s\r\n", cp->Name, cp->Help);
    if (strchr(NEWSMASTER, '@') == NULL)
	Printf("Report problems to <%s@%s>\r\n",
	    NEWSMASTER, innconf->fromhost);
    else
	Printf("Report problems to <%s>\r\n",
	    NEWSMASTER);
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
#if	!defined(HPUX)
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
    (void)pstat(PSTAT_SETCMD, un, strlen(buff), 0, 0);
#endif	/* defined(HPUX) */
#endif	/* defined(HAVE_SETPROCTITLE) */
}


#if	defined(DO_NNRP_GETHOSTBYADDR)
/*
**  Convert an IP address to a hostname.  Don't trust the reverse lookup,
**  since anyone can fake .in-addr.arpa entries.
*/
STATIC BOOL
Address2Name(ap, hostname, i)
    register INADDR		*ap;
    register char		*hostname;
    register int		i;
{
    register char		*p;
    register struct hostent	*hp;
#if	defined(h_addr)
    register char		**pp;
#endif

    /* Get the official hostname, store it away. */
    if ((hp = gethostbyaddr((char *)ap, sizeof *ap, AF_INET)) == NULL)
	return FALSE;
    (void)strncpy(hostname, hp->h_name, i);
    hostname[i - 1] = '\0';

    /* Get addresses for this host. */
    if ((hp = gethostbyname(hostname)) == NULL)
	return FALSE;

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
    if (strchr(hostname, '.') == NULL
     && (p = innconf->domain) != NULL) {
	(void)strcat(hostname, ".");
	(void)strcat(hostname, p);
    }

    /* Make all lowercase, for wildmat. */
    for (p = hostname; *p; p++)
	if (CTYPE(isupper, *p))
	    *p = tolower(*p);
    return TRUE;
}
#endif	/* defined(DO_NNRP_GETHOSTBYADDR) */


BOOL
PERMinfile(hp, ip, user, pass, accesslist, accessfile)
    char		*hp;
    char		*ip;
    char		*user;
    char		*pass;
    char		*accesslist;
    char		*accessfile;
{
    register FILE	*F;
    register char	*p;
    register BOOL	found;
    register int	i;
    struct passwd	*pwd;
    char		buff[BIG_BUFFER];
    char		filename[SMBUF];
    char		*fields[5];

    if ((F = fopen(accessfile, "r")) == NULL) {
	syslog(L_ERROR, "%s cant fopen %s %m", ClientHost, accessfile);
	return FALSE;
    }

    PERMcanread = FALSE;
    PERMcanpost = FALSE;
    PERMlocpost = FALSE;
    found = FALSE;
    accesslist[0] = '\0';
    filename[0] = '\0';
    while (fgets(buff, sizeof buff, F) != NULL) {
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, COMMENT_CHAR)) != NULL)
	    *p = '\0';
	if (buff[0] == '\0')
	    continue;

	/* Split "host:permissions:user:pass:groups" into fields. */
	for (fields[0] = buff, i = 0, p = buff; *p; p++)
	    if (*p == ':') {
		*p = '\0';
		fields[++i] = p + 1;
	    }
	if ((i != 4) && !((i == 1) && (*fields[1] == '/'))) {
	    /* Malformed line. */
	    syslog(L_ERROR, "Malformed line in access file");
	    continue;
	}

	if (hp) {
	    /* Got an address; try to match either the IP address or as
	     * a text hostname. */
	    int ipmatch = FALSE;
	    int hostmatch = FALSE;
	    int netmatch = FALSE;

	    /* compare IP number and first field */
	    if (ip && wildmat(ip, fields[0]))
		ipmatch = TRUE;

	    /* compare hostname and first field */
	    if (wildmat(hp, fields[0]))
		hostmatch = TRUE;

	    /* try a netmask match */
	    if (ip && (p = strchr(fields[0], '/')) != (char *)NULL) {
		int bits, c;
		struct in_addr ia, net;
		unsigned int mask;

		*p = '\0';
		ia.s_addr = inet_addr(ip);
		net.s_addr = inet_addr(fields[0]);
		if (ia.s_addr != (unsigned int)INADDR_NONE &&
		    net.s_addr != (unsigned int)INADDR_NONE) {
		    mask = inet_addr(p+1);
		    if (strchr(p+1, '.') == (char *)NULL) {
		    /* if (mask == (unsigned int)INADDR_NONE) { */
			for (bits = c = 0; c < mask && c < 32; c++)
			    bits |= (1 << (31 - c));
			mask = htonl(bits);
		    }
		    if ((ia.s_addr & mask) == (net.s_addr & mask))
			netmatch = TRUE;
		}
	    }
	    if (!ipmatch && !hostmatch && !netmatch)
		continue;
	}

	/* If the PERM field starts with '/', then we go to another file */
	if (*fields[1] == '/') {
	    strncpy(filename, fields[1], sizeof(filename));
	    continue;
	} else
	    filename[0] = '\0';

#ifdef DONT_HAVE_SHADOW
	/* See if we should lookup a specific user in the passwd file */
	if (user && pass && EQ(fields[2], "+")) {
	    if ((pwd = getpwnam(user)) == NULL)
		continue;
	    if (!EQ(pwd->pw_passwd, crypt(pass, pwd->pw_passwd)))
		continue;
	} else
#endif
	/* Matching for a specific user or just the host? */
	if (user && (!EQ(user, fields[2]) || !EQ(pass, fields[3])))
	    continue;

	PERMcanread = strchr(fields[1], 'R') != NULL;
	PERMcanpost = strchr(fields[1], 'P') != NULL;
	PERMlocpost = strchr(fields[1], 'L') != NULL;
	if (strchr(fields[1], 'N') != NULL) PERMnewnews = TRUE;
	if (ForceReadOnly) PERMcanpost=FALSE;
	(void)strcpy(PERMuser, user ? user : fields[2]);
	(void)strcpy(PERMpass, pass ? pass : fields[3]);
	(void)strcpy(accesslist, fields[4]);
	found = TRUE;
    }
    (void)fclose(F);
    if (found && (strlen(filename) > 0))
	return(PERMinfile(hp, ip, user, pass, accesslist, filename));
    return found;
}


/*
**  Determine access rights of the client.
*/
STATIC void
StartConnection(accesslist)
    char		*accesslist;
{
    struct sockaddr_in	sin;
    int			length;
    char		buff[SMBUF];
    char		*ClientAddr;

    /* Get the peer's name. */
    length = sizeof sin;
    ClientAddr = NULL;
    if (getpeername(STDIN, (struct sockaddr *)&sin, &length) < 0) {
	if (!isatty(STDIN)) {
	    syslog(L_TRACE, "%s cant getpeername %m", "?");
            (void)strcpy(ClientHost, "?"); /* so stats generation looks correct. */
	    Printf("%d I can't get your name.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1);
	}
	(void)strcpy(ClientHost, "stdin");
        ClientIP = 0L;
    }
#if defined(AF_DECnet)
    else if (sin.sin_family == AF_DECnet) {
	char *p, *MyDECnetDomain;
	(void) strcpy(ClientHost, getenv("REMNODE"));
	for (p = ClientHost; *p; p++)
	    if (CTYPE(isupper, *p))
		*p = tolower(*p);
#define _CONF_DECNETDOMAIN "decnetdomain"
	if ((MyDECnetDomain = GetConfigValue(_CONF_DECNETDOMAIN)) != NULL) {
	    (void)strcat(ClientHost, ".");
	    (void)strcat(ClientHost, MyDECnetDomain);
	}
    }
#endif
    else {
	if (sin.sin_family != AF_INET) {
	    syslog(L_ERROR, "%s bad_address_family %ld",
		"?", (long)sin.sin_family);
	    Printf("%d Bad address family.  Goodbye.\r\n", NNTP_ACCESS_VAL);
	    ExitWithStats(1);
	}

	/* Get client's name. */
#if	defined(DO_NNRP_GETHOSTBYADDR)
	if (!Address2Name(&sin.sin_addr, ClientHost, (int)sizeof ClientHost)) {
	    (void)strcpy(ClientHost, inet_ntoa(sin.sin_addr));
	    syslog(L_NOTICE,
		"? cant gethostbyaddr %s %m -- using IP address for access",
		ClientHost);
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
    }

    strncpy (LogName,ClientHost,sizeof(LogName) - 1) ;
    LogName[sizeof(LogName) - 1] = '\0';

    syslog(L_NOTICE, "%s connect", ClientHost);
    if (!PERMinfile(ClientHost, ClientAddr, (char *)NULL, (char *)NULL,
	    accesslist, NNRPACCESS)) {
	syslog(L_NOTICE, "%s no_access", ClientHost);
	Printf("%d You are not in my access file.  Goodbye.\r\n",
	    NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }
}


#if	!defined(VAR_NONE)

#if	defined(VAR_VARARGS)
#if	defined(lint)
#define START_VARARG(fmt, vp, type)	va_start(vp); fmt = NULL
#else
#define START_VARARG(fmt, vp, type)	va_start(vp); fmt = va_arg(vp, type)
#endif	/* defined(lint) */
#endif	/* defined(VAR_VARARGS) */
#if	defined(VAR_STDARGS)
#define START_VARARG(fmt, vp, type)	va_start(vp, fmt)
#endif	/* defined(VAR_STDARGS) */

/*
**  Send a reply, possibly with debugging output.
*/
/*VARARGS*/
void
#if	defined(VAR_VARARGS)
Reply(va_alist)
    va_dcl
#endif	/* defined(VAR_VARARGS) */
#if	defined(VAR_STDARGS)
Reply(char *fmt, ...)
#endif	/* defined(VAR_STDARGS) */
{
    register int	oerrno;
    register char	*p;
    va_list		vp;
    char		buff[2048];
#if	defined(VAR_VARARGS)
    register char	*fmt;
#endif	/* defined(VAR_VARARGS) */

    START_VARARG(fmt, vp, char*);
    (void)vprintf(fmt, vp);
    va_end(vp);

    if (Tracing) {
	oerrno = errno;
	START_VARARG(fmt, vp, char*);

	/* Copy output, but strip trailing CR-LF. */
	(void)vsprintf(buff, fmt, vp);
	p = buff + strlen(buff) - 1;
	while (p >= buff && (*p == '\n' || *p == '\r'))
	    *p-- = '\0';
	syslog(LOG_DEBUG, "%s > %s", ClientHost, buff);

	va_end(vp);
	errno = oerrno;
    }
}
#endif	/* !defined(VAR_NONE) */


/*
**  Got a signal; toggle tracing.
*/
STATIC SIGHANDLER
ToggleTrace(s)
    int		s;
{
    ChangeTrace = TRUE;
    (void)signal(s, ToggleTrace);
}

/*
** Got a SIGPIPE; exit cleanly
*/
STATIC SIGHANDLER
CatchPipe(s)
    int		s;
{
    ExitWithStats(0);
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
    (void)signal(s, WaitChild);
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
main(argc, argv, env)
    int			argc;
    char		*argv[];
    char		*env[];
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
    register int	i;
    char		*Reject;
    char		accesslist[BIG_BUFFER];
    int			timeout;
    BOOL		val;
    char		*p;
    int			vid=0; 
    int 		count=123456789;
    struct		timeval tv;
    unsigned short	ListenPort = NNTP_PORT;
    unsigned long	ListenAddr = htonl(INADDR_ANY);
    int			lfd, fd, clen;
    struct sockaddr_in	ssa, csa;
    PID_T		pid = -1;
   

#if	!defined(HPUX)
    /* Save start and extent of argv for TITLEset. */
    TITLEstart = argv[0];
    TITLEend = argv[argc - 1] + strlen(argv[argc - 1]) - 1;
#endif	/* !defined(HPUX) */

    /* Parse arguments.   Must COPY() optarg if used because the
     * TITLEset() routine would clobber it! */
    Reject = NULL;
    RARTenable=FALSE;
    LLOGenable=FALSE;

    openlog("nnrpd", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

    if (ReadInnConf() < 0) exit(1);

    while ((i = getopt(argc, argv, "b:Di:lop:Rr:s:t")) != EOF)
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
	case 'i':			/* Initial command */
	    PushedBack = COPY(optarg);
	    break;
	case 'l':			/* Tracking */
	    RARTenable=TRUE;
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
	}
    argc -= optind;
    if (argc)
	Usage();

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
	    
	ssa.sin_family = AF_INET;
	ssa.sin_addr.s_addr = ListenAddr;
	ssa.sin_port = htons(ListenPort);
	
	if (bind(lfd, (struct sockaddr *) &ssa, sizeof(ssa)) < 0) {
	    fprintf(stderr, "%s: can't bind (%s)\n", argv[0], strerror(errno));
	    syslog(L_FATAL, "can't bind local address (%m)");
	    exit(1);
	}

	/* Detach */
	if ((pid = FORK()) < 0) {
	    fprintf(stderr, "%s: can't ford (%s)\n", argv[0], strerror(errno));
	    syslog(L_FATAL, "can't fork (%m)");
	    exit(1);
	} else if (pid != 0) 
	    exit(0);

	setsid();
 
	/* Set signal handle to care for dead children */
	(void)signal(SIGCHLD, WaitChild);
 
	TITLEset("nnrpd: accepting connections");
 	
	listen(lfd, 5);	

listen_loop:
	clen = sizeof(csa);
	fd = accept(lfd, (struct sockaddr *) &csa, &clen);
	if (fd < 0)
		goto listen_loop;
    
	for (i = 0; (pid = FORK()) < 0; i++) {
	    if (i == MAX_FORKS) {
		syslog(L_FATAL, "cant fork %m -- giving up");
		exit(1);
	    }
	    syslog(L_NOTICE, "cant fork %m -- waiting");
	    (void)sleep(1);
	}

	if (pid != 0)
		goto listen_loop;

	/* child process starts here */
	TITLEset("nnrpd: connected");
	close(lfd);
	dup2(fd, 0);
	close(fd);
	dup2(0, 1);
	dup2(0, 2);
 
    }  /* DaemonMode */

    /* Setup. */
    if (GetTimeInfo(&Now) < 0) {
	syslog(L_FATAL, "cant gettimeinfo %m");
	exit(1);
    }
    STATstart = TIMEINFOasDOUBLE(Now);

    PERMnewnews = innconf->allownewnews;

    if (innconf->overviewmmap)
	val = TRUE;
    else
	val = FALSE;
    if (!OVERsetup(OVER_MMAP, &val) || !OVERsetup(OVER_MODE, "r")) {
	syslog(L_FATAL, "cant setup unified overview %m");
	ExitWithStats(1);
    }

#if	NNRP_LOADLIMIT > 0
    if ((load = GetLoadAverage()) > NNRP_LOADLIMIT) {
	syslog(L_NOTICE, "load %d > %d", load, NNRP_LOADLIMIT);
	Reply("%d load at %d, try later\r\n", NNTP_GOODBYE_VAL, load);
	ExitWithStats(1);
    }
#endif	/* NNRP_LOADLIMIT > 0 */

    strcpy (LogName, "?");

    OVERindex = NULL;
    OVERicount = 0;

    /* Catch SIGPIPE so that we can exit out of long write loops */
    (void)signal(SIGPIPE, CatchPipe);

    /* Arrange to toggle tracing. */
    (void)signal(SIGHUP, ToggleTrace);

    /* Get permissions and see if we can talk to this client */
    StartConnection(accesslist);
    if (!PERMcanread && !PERMcanpost) {
	syslog(L_NOTICE, "%s no_permission", ClientHost);
	Printf("%d You have no permission to talk.  Goodbye.\r\n",
	       NNTP_ACCESS_VAL);
	ExitWithStats(1);
    }

    /* Proceed with initialization. */
    PERMneedauth = PERMuser[0] != '\0' && PERMpass[0] != '\0';
    PERMspecified = NGgetlist(&PERMlist, accesslist);
    TITLEset("connect");

    /* Were we told to reject connections? */
    if (Reject) {
	syslog(L_NOTICE, "%s rejected %s", ClientHost, Reject);
	Reply("%s %s\r\n", NNTP_GOODBYE, Reject);
	ExitWithStats(0);
    }

    if (RARTenable)
	RARTenable=TrackClient(ClientHost,Username);

    if (RARTenable) {
	syslog(L_NOTICE, "%s Tracking Enabled (%s)", ClientHost, Username);
	pid=getpid();
	gettimeofday(&tv,NULL);
	count += pid;
	vid = tv.tv_sec ^ tv.tv_usec ^ pid ^ count;
	sprintf(LocalLogFileName, "%s/tracklogs/log-%ld", innconf->pathlog,vid);
	if ((locallog=fopen(LocalLogFileName, "w")) != NULL) {
		syslog(L_NOTICE, "%s Local Logging begins (%s) %s",ClientHost, Username, LocalLogFileName);
		fprintf(locallog, "%s Tracking Enabled (%s)\n", ClientHost, Username);
		fflush(locallog);
		LLOGenable=TRUE;
	} else {
		syslog(L_NOTICE, "%s Local Logging failed (%s) %s", ClientHost, Username, LocalLogFileName);
	}
    }

    ARTreadschema();
    if (!GetGroupList()) {
	/* This shouldn't really happen. */
	syslog(L_NOTICE, "%s cant getgrouplist %m", ClientHost);
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1);
    }

    if (innconf->storageapi && !SMinit()) {
	syslog(L_NOTICE, "%s cant initialize storage method, %s", ClientHost, SMerrorstr);
	Reply("%d NNTP server unavailable. Try later.\r\n", NNTP_TEMPERR_VAL);
	ExitWithStats(1);
    }

#if defined(DO_PERL)
    /* Load the Perl code */
    PERLsetup(NULL, cpcatpath(innconf->pathfilter, _PATH_PERL_FILTER_NNRPD), "filter_post");
    PerlFilter (TRUE) ;
#endif /* defined(DO_PERL) */

    Reply("%d %s InterNetNews NNRP server %s ready (%s).\r\n",
	   PERMcanpost ? NNTP_POSTOK_VAL : NNTP_NOPOSTOK_VAL,
	   innconf->pathhost, INNVersion(),
	   PERMcanpost ? "posting ok" : "no posting");

    /* Exponential posting backoff */
    (void)InitBackoffConstants();

    /* Main dispatch loop. */
    for (timeout = INITIAL_TIMEOUT, av = NULL; ;
			timeout = innconf->clienttimeout) {
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
	}
	else
	    switch (r = READline(buff, (int)sizeof buff, timeout)) {
	    default:
		syslog(L_ERROR, "%s internal %d in main", ClientHost, r);
		/* FALLTHROUGH */
	    case RTtimeout:
		if (timeout < innconf->clienttimeout)
		    syslog(L_NOTICE, "%s timeout short", ClientHost);
		else
		    syslog(L_NOTICE, "%s timeout", ClientHost);
		Printf("%d Timeout after %d seconds, closing connection.\r\n",
		       NNTP_TEMPERR_VAL, timeout);
		ExitWithStats(1);
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
    }

    Reply("%s\r\n", NNTP_GOODBYE_ACK);

    ExitWithStats(0);
    /* NOTREACHED */
}
