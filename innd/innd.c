/*  $Id$
**
**  Variable definitions, miscellany, and main().
*/

#include "config.h"
#include "clibrary.h"

#include "inn/innconf.h"
#include "inn/messages.h"
#include "innperl.h"

#define DEFINE_DATA
#include "innd.h"
#include "ov.h"


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
static RETSIGTYPE       catch_terminate(int sig);
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
  ARTHEADERINIT("X-Original-Message-ID",	HTstd)
/* #define HDR__X_ORIGINAL_MESSAGE_ID		38 */
};
/* #define MAX_ARTHEADER			39 */


/*
**  Signal handler to catch SIGTERM and similar signals and queue a clean
**  shutdown.
*/
static RETSIGTYPE
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
        syslog(LOG_WARNING, "SERVER shutdown received signal %d",
               killer_signal);
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

#if DO_TCL
    TCLclose();
#endif

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
                      (F == stdout) ? _PATH_LOGFILE : _PATH_ERRLOG);
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
    char *t;
    bool flag;
    static char		WHEN[] = "PID file";
    int			i, j, fd[MAX_SOCKETS + 1];
    char		buff[SMBUF], *path1, *path2;
    FILE		*F;
    bool		ShouldFork;
    bool		ShouldRenumber;
    bool		ShouldSyntaxCheck;
    bool		filter = true;
    pid_t		pid;
#if	defined(_DEBUG_MALLOC_INC)
    union malloptarg	m;
#endif	/* defined(_DEBUG_MALLOC_INC) */

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

    /* Make sure innd is not running as root.  innd must be either started
       via inndstart or use a non-privileged port. */
    if (getuid() == 0 || geteuid() == 0)
        die("SERVER must be run as user news, not root (use inndstart)");

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
    fd[0] = fd[1] = -1;

    /* Set some options from inn.conf that can be overridden with
       command-line options if they exist, so read inn.conf first. */
    if (!innconf_read(NULL))
        exit(1);

    /* Parse JCL. */
    CCcopyargv(av);
    while ((i = getopt(ac, av, "ac:Cdfi:l:m:o:Nn:p:P:rst:uH:T:X:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'a':
	    AnyIncoming = true;
	    break;
	case 'c':
	    innconf->artcutoff = atoi(optarg);
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
	    innconf->maxconnections = atoi(optarg);
	    break;
	case 'I':
	    if (innconf->bindaddress) free(innconf->bindaddress);
	    innconf->bindaddress = xstrdup(optarg);
	    break;
	case 'l':
	    innconf->maxartsize = atol(optarg);
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
	case 'p':
	    /* Silently ignore multiple -p flags, in case ctlinnd xexec
	       called inndstart. */
	    if (fd[0] != -1)
		break;
	    t = xstrdup(optarg);
	    p = strtok(t, ",");
	    j = 0;
	    do {
		fd[j++] = atoi(p);
		if (j == MAX_SOCKETS)
		    break;
	    } while ((p = strtok(NULL, ",")) != NULL);
	    fd[j] = -1;
	    free(t);
	    break;
	case 'P':
	    innconf->port = atoi(optarg);
	    break;
	case 'r':
	    ShouldRenumber = true;
	    break;
	case 's':
	    ShouldSyntaxCheck = true;
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
    if (ModeReason && !innconf->readerswhenstopped)
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
        path = concatpath(innconf->pathlog, _PATH_LOGFILE);
        if (freopen(path, "a", stdout) == NULL)
            sysdie("SERVER cant freopen stdout to %s", path);
        setvbuf(stdout, NULL, _IOFBF, LOG_BUFSIZ);
        free(path);
        path = concatpath(innconf->pathlog, _PATH_ERRLOG);
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

    /* Always attempt to increase the number of open file descriptors.  If
       we're not root, this may just fail quietly. */
    if (innconf->rlimitnofile > 0)
        setfdlimit(innconf->rlimitnofile);

    /* Get number of open channels. */
    i = getfdlimit();
    if (i < 0) {
	syslog(L_FATAL, "%s cant get file descriptor limit: %m", LogName);
	exit(1);
    }

    /* There is no file descriptor limit on some hosts; for those, cap at
       MaxOutgoing plus maxconnections plus 20, or 5000, whichever is larger. 
       Otherwise, we use insane amounts of memory for the channel table.
       FIXME: Get rid of this hard-coded constant. */
    if (i > 5000) {
        int max;

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
	PID = concatpath(innconf->pathrun, _PATH_SERVERPID);
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

    if (GetTimeInfo(&Now) < 0)
	syslog(L_ERROR, "%s cant gettimeinfo %m", LogName);

    /* Set up signal and error handlers. */
    xmalloc_error_handler = xmalloc_abort;
    xsignal(SIGHUP, catch_terminate);
    xsignal(SIGTERM, catch_terminate);

    /* Set up the various parts of the system.  Channel feeds start
       processes so call PROCsetup before ICDsetup.  NNTP needs to know if
       it's a slave, so call RCsetup before NCsetup. */
    CHANsetup(i);
    PROCsetup(10);
    if (Mode == OMrunning)
        InndHisOpen();
    CCsetup();
    LCsetup();
    RCsetup(fd[0]);
    for (i = 1; fd[i] != -1; i++)
	RCsetup(fd[i]);
    WIPsetup();
    NCsetup();
    ARTsetup();
    ICDsetup(true);
    if (innconf->timer)
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

#if DO_TCL
    TCLsetup();
    if (!filter)
	TCLfilter(false);
#endif /* DO_TCL */

#if DO_PERL
    /* Load the Perl code */
    path1 = concatpath(innconf->pathfilter, _PATH_PERL_STARTUP_INND);
    path2 = concatpath(innconf->pathfilter, _PATH_PERL_FILTER_INND);
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
