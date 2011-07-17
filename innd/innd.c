/*  $Id$
**
**  Variable definitions, miscellany, and main().
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/newsuser.h"
#include "innperl.h"

#define DEFINE_DATA
#include "innd.h"
#include "inn/ov.h"


bool		Debug = false;
bool		NNRPTracing = false;
bool		StreamingOff = false ; /* default is we can stream */
bool		Tracing = false;
bool		DoCancels = true;
char		LogName[] = "SERVER";
int		ErrorCount = IO_ERROR_COUNT;
OPERATINGMODE	Mode = OMrunning;
int		RemoteLimit = REMOTELIMIT;
time_t		RemoteTimer = REMOTETIMER;
int		RemoteTotal = REMOTETOTAL;
bool		ThrottledbyIOError = false;

static char	*PID = NULL;

/* Signal handling.  If we receive a signal that should kill the server,
   killer_signal is set to the signal number that we received.  This isn't
   what indicates that we should terminate; that's the separate global
   variable GotTerminate, used in CHANreadloop. */
static volatile sig_atomic_t killer_signal = 0;

/* Whether our self-maintained logs (stdout and stderr) are buffered, used
   to determine whether fflush is needed.  Should be static. */
bool BufferedLogs = true;

/* FILEs for logs and error logs.  Everything should just use stdout and
   stderr. */
FILE *Log = NULL;
FILE *Errlog = NULL;

/* Some very old systems have a completely inadequate BUFSIZ buffer size, at
   least for our logging purposes. */
#if BUFSIZ < 4096
# define LOG_BUFSIZ 4096
#else
# define LOG_BUFSIZ BUFSIZ
#endif

/* Internal prototypes. */
static void             catch_terminate(int sig);
static void             xmalloc_abort(const char *what, size_t size,
                                      const char *file, int line);

/* header table initialization */
#define ARTHEADERINIT(name, type) {name, type, sizeof(name) - 1}
const ARTHEADER ARTheaders[] = {
  /*		 Name			Type */
  ARTHEADERINIT("Approved",		HTstd),
/* #define HDR__APPROVED			0 */
  ARTHEADERINIT("Control",		HTstd),
/* #define HDR__CONTROL				1 */
  ARTHEADERINIT("Date",			HTreq),
/* #define HDR__DATE				2 */
  ARTHEADERINIT("Distribution",		HTstd),
/* #define HDR__DISTRIBUTION			3 */
  ARTHEADERINIT("Expires",		HTstd),
/* #define HDR__EXPIRES				4 */
  ARTHEADERINIT("From",			HTreq),
/* #define HDR__FROM				5 */
  ARTHEADERINIT("Lines",		HTstd),
/* #define HDR__LINES				6 */
  ARTHEADERINIT("Message-ID",		HTreq),
/* #define HDR__MESSAGE_ID			7 */
  ARTHEADERINIT("Newsgroups",		HTreq),
/* #define HDR__NEWSGROUPS			8 */
  ARTHEADERINIT("Path",			HTreq),
/* #define HDR__PATH				9 */
  ARTHEADERINIT("Reply-To",		HTstd),
/* #define HDR__REPLY_TO			10 */
  ARTHEADERINIT("Sender",		HTstd),
/* #define HDR__SENDER				11 */
  ARTHEADERINIT("Subject",		HTreq),
/* #define HDR__SUBJECT				12 */
  ARTHEADERINIT("Supersedes",		HTstd),
/* #define HDR__SUPERSEDES			13 */
  ARTHEADERINIT("Bytes",		HTstd),
/* #define HDR__BYTES				14 */
  ARTHEADERINIT("Also-Control",		HTobs),
/* #define HDR__ALSOCONTROL			15 */
  ARTHEADERINIT("References",		HTstd),
/* #define HDR__REFERENCES			16 */
  ARTHEADERINIT("Xref",			HTsav),
/* #define HDR__XREF				17 */
  ARTHEADERINIT("Keywords",		HTstd),
/* #define HDR__KEYWORDS			18 */
  ARTHEADERINIT("X-Trace",		HTstd),
/* #define HDR__XTRACE				19 */
  ARTHEADERINIT("Date-Received",	HTobs),
/* #define HDR__DATERECEIVED			20 */
  ARTHEADERINIT("Posted",		HTobs),
/* #define HDR__POSTED				21 */
  ARTHEADERINIT("Posting-Version",	HTobs),
/* #define HDR__POSTINGVERSION			22 */
  ARTHEADERINIT("Received",		HTobs),
/* #define HDR__RECEIVED			23 */
  ARTHEADERINIT("Relay-Version",	HTobs),
/* #define HDR__RELAYVERSION			24 */
  ARTHEADERINIT("NNTP-Posting-Host",	HTstd),
/* #define HDR__NNTPPOSTINGHOST			25 */
  ARTHEADERINIT("Followup-To",		HTstd),
/* #define HDR__FOLLOWUPTO			26 */
  ARTHEADERINIT("Organization",		HTstd),
/* #define HDR__ORGANIZATION			27 */
  ARTHEADERINIT("Content-Type",		HTstd),
/* #define HDR__CONTENTTYPE			28 */
  ARTHEADERINIT("Content-Base",		HTstd),
/* #define HDR__CONTENTBASE			29 */
  ARTHEADERINIT("Content-Disposition",	HTstd),
/* #define HDR__CONTENTDISPOSITION		30 */
  ARTHEADERINIT("X-Newsreader",		HTstd),
/* #define HDR__XNEWSREADER			31 */
  ARTHEADERINIT("X-Mailer",		HTstd),
/* #define HDR__XMAILER				32 */
  ARTHEADERINIT("X-Newsposter",		HTstd),
/* #define HDR__XNEWSPOSTER			33 */
  ARTHEADERINIT("X-Cancelled-By",	HTstd),
/* #define HDR__XCANCELLEDBY			34 */
  ARTHEADERINIT("X-Canceled-By",	HTstd),
/* #define HDR__XCANCELEDBY			35 */
  ARTHEADERINIT("Cancel-Key",		HTstd),
/* #define HDR__CANCELKEY			36 */
  ARTHEADERINIT("User-Agent",		HTstd),
/* #define HDR__USER_AGENT			37 */
  ARTHEADERINIT("X-Original-Message-ID",	HTstd),
/* #define HDR__X_ORIGINAL_MESSAGE_ID		38 */
  ARTHEADERINIT("Cancel-Lock",		HTstd),
/* #define HDR__CANCEL_LOCK			39 */
  ARTHEADERINIT("Content-Transfer-Encoding",	HTstd),
/* #define HDR__CONTENT_TRANSFER_ENCODING	40 */
  ARTHEADERINIT("Face",			HTstd),
/* #define HDR__FACE				41 */
  ARTHEADERINIT("Injection-Info",	HTstd),
/* #define HDR__INJECTION_INFO			42 */
  ARTHEADERINIT("List-ID",		HTstd),
/* #define HDR__LIST_ID				43 */
  ARTHEADERINIT("MIME-Version",		HTstd),
/* #define HDR__MIME_VERSION			44 */
  ARTHEADERINIT("Originator",		HTstd),
/* #define HDR__ORIGINATOR			45 */
  ARTHEADERINIT("X-Auth",		HTstd),
/* #define HDR__X_AUTH				46 */
  ARTHEADERINIT("X-Complaints-To",	HTstd),
/* #define HDR__X_COMPLAINTS_TO			47 */
  ARTHEADERINIT("X-Face",		HTstd),
/* #define HDR__X_FACE				48 */
  ARTHEADERINIT("X-HTTP-UserAgent",	HTstd),
/* #define HDR__X_HTTP_USERAGENT		49 */
  ARTHEADERINIT("X-HTTP-Via",		HTstd),
/* #define HDR__X_HTTP_VIA			50 */
  ARTHEADERINIT("X-Modbot",		HTstd),
/* #define HDR__X_MODBOT			51 */
  ARTHEADERINIT("X-Modtrace",		HTstd),
/* #define HDR__X_MODTRACE			52 */
  ARTHEADERINIT("X-No-Archive",		HTstd),
/* #define HDR__X_NO_ARCHIVE			53 */
  ARTHEADERINIT("X-Original-Trace",	HTstd),
/* #define HDR__X_ORIGINAL_TRACE		54 */
  ARTHEADERINIT("X-Originating-IP",	HTstd),
/* #define HDR__X_ORIGINATING_IP		55 */
  ARTHEADERINIT("X-PGP-Key",		HTstd),
/* #define HDR__X_PGP_KEY			56 */
  ARTHEADERINIT("X-PGP-Sig",		HTstd),
/* #define HDR__X_PGP_SIG			57 */
  ARTHEADERINIT("X-Poster-Trace",	HTstd),
/* #define HDR__X_POSTER_TRACE			58 */
  ARTHEADERINIT("X-Postfilter",		HTstd),
/* #define HDR__X_POSTFILTER			59 */
  ARTHEADERINIT("X-Proxy-User",		HTstd),
/* #define HDR__X_PROXY_USER			60 */
  ARTHEADERINIT("X-Submissions-To",	HTstd),
/* #define HDR__X_SUBMISSIONS_TO		61 */
  ARTHEADERINIT("X-Usenet-Provider",	HTstd),
/* #define HDR__X_USENET_PROVIDER		62 */
  ARTHEADERINIT("In-Reply-To",		HTstd),
/* #define HDR__IN_REPLY_TO			63 */
  ARTHEADERINIT("Injection-Date",	HTstd),
/* #define HDR__INJECTION_DATE			64 */
  ARTHEADERINIT("NNTP-Posting-Date",    HTstd),
/* #define HDR__NNTP_POSTING_DATE               65 */
  ARTHEADERINIT("X-User-ID",            HTstd),
/* #define HDR__X_USER_ID                       66 */
  ARTHEADERINIT("X-Auth-Sender",        HTstd),
/* #define HDR__X_AUTH_SENDER                   67 */
  ARTHEADERINIT("X-Original-NNTP-Posting-Host", HTstd),
/* #define HDR__X_ORIGINAL_NNTP_POSTING_HOST    68 */
  ARTHEADERINIT("Original-Sender",      HTstd),
/* #define HDR__ORIGINAL_SENDER                 69 */
  ARTHEADERINIT("NNTP-Posting-Path",    HTstd),
/* #define HDR__NNTP_POSTING_PATH               70 */
  ARTHEADERINIT("Archive",              HTstd),
/* #define HDR__ARCHIVE                         71 */
  ARTHEADERINIT("Archived-At",          HTstd),
/* #define HDR__ARCHIVED_AT                     72 */
  ARTHEADERINIT("Summary",              HTstd),
/* #define HDR__SUMMARY                         73 */
  ARTHEADERINIT("Comments",             HTstd)
/* #define HDR__COMMENTS                        74 */
};
/* #define MAX_ARTHEADER                        75 */


/*
**  Signal handler to catch SIGTERM and similar signals and queue a clean
**  shutdown.
*/
static void
catch_terminate(int sig)
{
    GotTerminate = true;
    killer_signal = sig;

#ifndef HAVE_SIGACTION
    xsignal(sig, catch_terminate);
#endif
}


/*
**  Memory allocation failure handler.  Instead of the default behavior of
**  just exiting, call abort to generate a core dump.
*/
static void
xmalloc_abort(const char *what, size_t size, const char *file, int line)
{
    fprintf(stderr, "SERVER cant %s %lu bytes at %s line %d: %s", what,
            (unsigned long) size, file, line, strerror(errno));
    syslog(LOG_CRIT, "SERVER cant %s %lu bytes at %s line %d: %m", what,
           (unsigned long) size, file, line);
    abort();
}


/*
**  The name is self-explanatory.
*/
void
CleanupAndExit(int status, const char *why)
{
    JustCleanup();
    if (why)
        syslog(LOG_WARNING, "SERVER shutdown %s", why);
    else
        syslog(LOG_WARNING, "SERVER shutdown received signal %lu",
               (unsigned long) killer_signal);
    exit(status);
}


/*
**  Close down all parts of the system (e.g., before calling exit or exec).
*/
void
JustCleanup(void)
{
    SITEflushall(false);
    CCclose();
    LCclose();
    NCclose();
    RCclose();
    ICDclose();
    InndHisClose();
    ARTclose();
    if (innconf->enableoverview) 
        OVclose();
    NGclose();
    SMshutdown();

#if DO_PERL
    PerlFilter(false);
    PerlClose();
#endif

#if DO_PYTHON
    PYclose();
#endif

    CHANshutdown();
    innconf_free(innconf);
    innconf = NULL;

    sleep(1);

    if (unlink(PID) < 0 && errno != ENOENT)
        syslog(LOG_ERR, "SERVER cant unlink %s: %m", PID);
}


/*
**  Flush one log file, re-establishing buffering if necessary.  stdout is
**  block-buffered, stderr is line-buffered.
*/
void
ReopenLog(FILE *F)
{
    char *path, *oldpath;
    int mask;

    if (Debug)
	return;

    path = concatpath(innconf->pathlog,
                      (F == stdout) ? INN_PATH_LOGFILE : INN_PATH_ERRLOG);
    oldpath = concat(path, ".old", (char *) 0);
    if (rename(path, oldpath) < 0)
        syswarn("SERVER cant rename %s to %s", path, oldpath);
    free(oldpath);
    mask = umask(033);
    if (freopen(path, "a", F) != F)
        sysdie("SERVER cant freopen %s", path);
    free(path);
    umask(mask);
    if (BufferedLogs)
        setvbuf(F, NULL, (F == stdout) ? _IOFBF : _IOLBF, LOG_BUFSIZ);
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
main(int ac, char *av[])
{
    const char *name, *p;
    char *path;
    bool flag;
    static char		WHEN[] = "PID file";
    int			i;
    char		buff[SMBUF];
    FILE		*F;
    bool		ShouldFork;
    bool		ShouldRenumber;
    bool		ShouldSyntaxCheck;
    bool		filter = true;
    pid_t		pid;
#if	defined(_DEBUG_MALLOC_INC)
    union malloptarg	m;
#endif	/* defined(_DEBUG_MALLOC_INC) */
#if DO_PERL
    char                *path1, *path2;
#endif

    /* Set up the pathname, first thing, and teach our error handlers about
       the name of the program. */
    name = av[0];
    if (name == NULL || *name == '\0')
	name = "innd";
    else {
        p = strrchr(name, '/');
        if (p != NULL)
            name = p + 1;
    }
    message_program_name = name;
    openlog(name, LOG_CONS | LOG_NDELAY, LOG_INN_SERVER);
    message_handlers_die(2, message_log_stderr, message_log_syslog_crit);
    message_handlers_warn(2, message_log_stderr, message_log_syslog_err);
    message_handlers_notice(1, message_log_syslog_notice);

    /* Make sure innd is running as the correct user.  If it is started as
       root, switch to running as the news user. */
    ensure_news_user_grp(true, true);

    /* Handle malloc debugging. */
#if	defined(_DEBUG_MALLOC_INC)
    m.i = M_HANDLE_ABORT;
    dbmallopt(MALLOC_WARN, &m);
    dbmallopt(MALLOC_FATAL, &m);
    m.i = 3;
    dbmallopt(MALLOC_FILLAREA, &m);
    m.i = 0;
    dbmallopt(MALLOC_CKCHAIN, &m);
    dbmallopt(MALLOC_CKDATA, &m);
#endif	/* defined(_DEBUG_MALLOC_INC) */

    /* Set defaults. */
    TimeOut.tv_sec = DEFAULT_TIMEOUT;
    TimeOut.tv_usec = 0;
    ShouldFork = true;
    ShouldRenumber = false;
    ShouldSyntaxCheck = false;

    /* Set some options from inn.conf that can be overridden with
       command-line options if they exist, so read inn.conf first. */
    if (!innconf_read(NULL))
        exit(1);

    /* Parse JCL. */
    CCcopyargv(av);
    while ((i = getopt(ac, av, "4:6:ac:CdfH:i:l:m:n:No:P:rsSt:T:uX:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
        case '4':
            if (innconf->bindaddress)
                free(innconf->bindaddress);
            innconf->bindaddress = xstrdup(optarg);
            break;
        case '6':
            if (innconf->bindaddress6)
                free(innconf->bindaddress6);
            innconf->bindaddress6 = xstrdup(optarg);
            break;
	case 'a':
	    AnyIncoming = true;
	    break;
	case 'c':
	    innconf->artcutoff = strtoul(optarg, NULL, 10);
	    break;
 	case 'C':
 	    DoCancels = false;
  	    break;
	case 'd':
	    Debug = true;
	    break;
	case 'f':
	    ShouldFork = false;
	    break;
	case 'H':
	    RemoteLimit = atoi(optarg);
	    break;
	case 'i':
	    innconf->maxconnections = strtoul(optarg, NULL, 10);
	    break;
	case 'l':
	    innconf->maxartsize = strtoul(optarg, NULL, 10);
	    break;
	case 'm':
	    if (ModeReason)
		free(ModeReason);
	    switch (*optarg) {
	    default:
		Usage();
		/* NOTREACHED */
	    case 'g':	Mode = OMrunning;	break;
	    case 'p':	Mode = OMpaused;	break;
	    case 't':	Mode = OMthrottled;	break;
	    }
	    if (Mode != OMrunning)
                ModeReason = concat(OMpaused ? "Paus" : "Throttl",
                                    "ed from the command line", (char *) 0);
	    break;
	case 'N':
	    filter = false;
	    break;
	case 'n':
	    switch (*optarg) {
	    default:
		Usage();
		/* NOTREACHED */
	    case 'n':	innconf->readerswhenstopped = false;	break;
	    case 'y':	innconf->readerswhenstopped = true;	break;
	    }
	    break;
	case 'o':
	    MaxOutgoing = atoi(optarg);
	    break;
	case 'P':
	    innconf->port = strtoul(optarg, NULL, 10);
	    break;
	case 'r':
	    ShouldRenumber = true;
	    break;
	case 's':
	    ShouldSyntaxCheck = true;
	    break;
        case 'S':
            RCreadlist();
            exit(0);
            break;
	case 't':
	    TimeOut.tv_sec = atol(optarg);
	    break;
	case 'T':
	    RemoteTotal = atoi(optarg);
	    break;
	case 'u':
	    BufferedLogs = false;
	    break;
	case 'X':
	    RemoteTimer = atoi(optarg);
	    break;
        case 'Z':
            StreamingOff = true;
            break;
	}
    ac -= optind;
    if (ac != 0)
	Usage();
    if (ModeReason != NULL && !innconf->readerswhenstopped)
	NNRPReason = xstrdup(ModeReason);

    if (ShouldSyntaxCheck) {
	if ((p = CCcheckfile((char **)NULL)) == NULL)
	    exit(0);
	fprintf(stderr, "%s\n", p + 2);
	exit(1);
    }

    /* Get the Path entry. */
    if (innconf->pathhost == NULL) {
	syslog(L_FATAL, "%s No pathhost set", LogName);
	exit(1);
    }
    Path.used = strlen(innconf->pathhost) + 1;
    Path.size = Path.used + 1;
    Path.data = xmalloc(Path.size);
    snprintf(Path.data, Path.size, "%s!", innconf->pathhost);
    if (innconf->pathalias == NULL) {
	Pathalias.used = 0;
	Pathalias.data = NULL;
    } else {
	Pathalias.used = strlen(innconf->pathalias) + 1;
	Pathalias.size = Pathalias.used + 1;
	Pathalias.data = xmalloc(Pathalias.size);
	snprintf(Pathalias.data, Pathalias.size, "%s!", innconf->pathalias);
    }
    if (innconf->pathcluster == NULL) {
        Pathcluster.used = 0;
        Pathcluster.data = NULL;
    } else {
        Pathcluster.used = strlen(innconf->pathcluster) + 1;
        Pathcluster.size = Pathcluster.used + 1;
        Pathcluster.data = xmalloc(Pathcluster.size);
        snprintf(Pathcluster.data, Pathcluster.size, "%s!", innconf->pathcluster);
    }
    /* Trace history ? */
    if (innconf->stathist != NULL) {
        syslog(L_NOTICE, "logging hist stats to %s", innconf->stathist);
        HISlogto(innconf->stathist);
    }

    i = dbzneedfilecount();
    if (!fdreserve(3 + i)) { /* TEMPORARYOPEN, history stats, INND_HISTORY and i */
	syslog(L_FATAL, "%s cant reserve file descriptors %m", LogName);
	exit(1);
    }

    /* Set up our permissions. */
    umask(NEWSUMASK);

    /* Become a daemon and initialize our log files. */
    if (Debug) {
	xsignal(SIGINT, catch_terminate);
        if (chdir(innconf->patharticles) < 0)
            sysdie("SERVER cant chdir to %s", innconf->patharticles);
    } else {
	if (ShouldFork)
            daemonize(innconf->patharticles);

	/* Open the logs.  stdout is used to log information about incoming
           articles and stderr is used to log serious error conditions (as
           well as to capture stderr from embedded filters).  Both are
           normally fully buffered. */
        path = concatpath(innconf->pathlog, INN_PATH_LOGFILE);
        if (freopen(path, "a", stdout) == NULL)
            sysdie("SERVER cant freopen stdout to %s", path);
        setvbuf(stdout, NULL, _IOFBF, LOG_BUFSIZ);
        free(path);
        path = concatpath(innconf->pathlog, INN_PATH_ERRLOG);
        if (freopen(path, "a", stderr) == NULL)
            sysdie("SERVER cant freopen stderr to %s", path);
        setvbuf(stderr, NULL, _IOLBF, BUFSIZ);
        free(path);
    }
    Log = stdout;
    Errlog = stderr;

    /* Initialize overview if necessary. */
    if (innconf->enableoverview && !OVopen(OV_WRITE))
        die("SERVER cant open overview method");

    /* Attempt to increase the number of open file descriptors. */
    if (innconf->rlimitnofile > 0) {
        if (setfdlimit(innconf->rlimitnofile) < 0)
            syswarn("SERVER cant set file descriptor limit");
    }

    /* Get number of open channels. */
    i = getfdlimit();
    if (i < 0)
        sysdie("SERVER cant get file descriptor limit");

#ifdef FD_SETSIZE
    if (FD_SETSIZE > 0 && (unsigned) i >= FD_SETSIZE) {
        syslog(LOG_WARNING, "%s number of descriptors (%d) exceeding or equaling FD_SETSIZE (%d)",
               LogName, i, FD_SETSIZE);
        i = FD_SETSIZE-1;
    }
#endif

    /* There is no file descriptor limit on some hosts; for those, cap at
       MaxOutgoing plus maxconnections plus 20, or 5000, whichever is larger. 
       Otherwise, we use insane amounts of memory for the channel table.
       FIXME:  Get rid of this hard-coded constant.
       (TODO:  Consider implementing libevent.) */
    if (i > 5000) {
        unsigned long max;

        max = innconf->maxconnections + MaxOutgoing + 20;
        if (max < 5000)
            max = 5000;
        i = max;
    }
    syslog(L_NOTICE, "%s descriptors %d", LogName, i);
    if (MaxOutgoing == 0) {
	/* getfdlimit() - (stdio + dbz + cc + lc + rc + art + fudge) */
	MaxOutgoing = i - (  3   +  3  +  2 +  1 +  1 +  1  +  2  );
	syslog(L_NOTICE, "%s outgoing %d", LogName, MaxOutgoing);
    }

    /* See if another instance is alive. */
    if (PID == NULL)
	PID = concatpath(innconf->pathrun, INN_PATH_SERVERPID);
    if ((F = fopen(PID, "r")) != NULL) {
	if (fgets(buff, sizeof buff, F) != NULL
	 && ((pid = (pid_t) atol(buff)) > 0)
	 && (kill(pid, 0) > 0 || errno != ESRCH)) {
	    syslog(L_FATAL, "%s already_running pid %ld", LogName,
	    (long) pid);
	    exit(1);
	}
	fclose(F);
    }

    if (gettimeofday(&Now, NULL) < 0)
	syslog(L_ERROR, "%s cant gettimeofday %m", LogName);

    /* Set up signal and error handlers. */
    xmalloc_error_handler = xmalloc_abort;
    xsignal(SIGHUP, catch_terminate);
    xsignal(SIGTERM, catch_terminate);

    /* Set up the various parts of the system.  Channel feeds start processes
       so call PROCsetup before ICDsetup.  NNTP needs to know if it's a slave,
       so call RCsetup before NCsetup.  RCsetup calls innbind and waits for
       it, so call PROCsetup after RCsetup to not interpose a signal
       handler. */
    CHANsetup(i);
    if (Mode == OMrunning)
        InndHisOpen();
    CCsetup();
    LCsetup();
    RCsetup();
    PROCsetup(10);
    WIPsetup();
    NCsetup();
    ARTsetup();
    ICDsetup(true);
    if (innconf->timer != 0)
        TMRinit(TMR_MAX);

    /* Initialize the storage subsystem. */
    flag = true;
    if (!SMsetup(SM_RDWR, &flag) || !SMsetup(SM_PREOPEN, &flag))
        die("SERVER cant set up storage manager");
    if (!SMinit())
        die("SERVER cant initalize storage manager: %s", SMerrorstr);

#if	defined(_DEBUG_MALLOC_INC)
    m.i = 1;
    dbmallopt(MALLOC_CKCHAIN, &m);
    dbmallopt(MALLOC_CKDATA, &m);
#endif	/* defined(_DEBUG_MALLOC_INC) */

    /* Record our PID. */
    pid = getpid();
    if ((F = fopen(PID, "w")) == NULL) {
	i = errno;
	syslog(L_ERROR, "%s cant fopen %s %m", LogName, PID);
	IOError(WHEN, i);
    }
    else {
	if (fprintf(F, "%ld\n", (long)pid) == EOF || ferror(F)) {
	    i = errno;
	    syslog(L_ERROR, "%s cant fprintf %s %m", LogName, PID);
	    IOError(WHEN, i);
	}
	if (fclose(F) == EOF) {
	    i = errno;
	    syslog(L_ERROR, "%s cant fclose %s %m", LogName, PID);
	    IOError(WHEN, i);
	}
	if (chmod(PID, 0664) < 0) {
	    i = errno;
	    syslog(L_ERROR, "%s cant chmod %s %m", LogName, PID);
	    IOError(WHEN, i);
	}
    }

#if DO_PERL
    /* Load the Perl code */
    path1 = concatpath(innconf->pathfilter, INN_PATH_PERL_STARTUP_INND);
    path2 = concatpath(innconf->pathfilter, INN_PATH_PERL_FILTER_INND);
    PERLsetup(path1, path2, "filter_art");
    free(path1);
    free(path2);
    PLxsinit();
    if (filter)
	PerlFilter(true);
#endif /* DO_PERL */

#if DO_PYTHON
    PYsetup();
    if (!filter)
	PYfilter(false);
#endif /* DO_PYTHON */
 
    /* And away we go... */
    if (ShouldRenumber) {
        syslog(LOG_NOTICE, "SERVER renumbering");
        if (!ICDrenumberactive())
            die("SERVER cant renumber");
    }
    syslog(LOG_NOTICE, "SERVER starting");
    CHANreadloop();

    /* CHANreadloop should never return. */
    CleanupAndExit(1, "CHANreadloop returned");
    return 1;
}
