/*  $Id$
**
**  Variable definitions, miscellany, and main().
*/

#include "config.h"
#include "clibrary.h"

#define DEFINE_DATA
#include "innd.h"
#include "ov.h"

/* From lib/perl.c. */
#if DO_PERL
extern void PerlFilter(bool value);
extern void PerlClose(void);
extern void PERLsetup(char *startupfile, char *filterfile, char *function);
#endif


bool		Debug = FALSE;
bool		NNRPTracing = FALSE;
bool		StreamingOff = FALSE ; /* default is we can stream */
bool		Tracing = FALSE;
bool		DoCancels = TRUE;
char		LogName[] = "SERVER";
int		ErrorCount = IO_ERROR_COUNT;
OPERATINGMODE	Mode = OMrunning;
int		RemoteLimit = REMOTELIMIT;
time_t		RemoteTimer = REMOTETIMER;
int		RemoteTotal = REMOTETOTAL;
bool		ThrottledbyIOError = FALSE;

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
static RETSIGTYPE       catch_terminate(int signal);
static void             xmalloc_abort(const char *what, size_t size,
                                      const char *file, int line);

/* header table initialization */
#define ARTHEADERINIT(name, type) {name, type, sizeof(name) - 1}
ARTHEADER ARTheaders[] = {
  /*		 Name			Type */
  ARTHEADERINIT("Approved",		HTstd),
/* #define _approved				0 */
  ARTHEADERINIT("Control",		HTstd),
/* #define _control				1 */
  ARTHEADERINIT("Date",			HTreq),
/* #define _date				2 */
  ARTHEADERINIT("Distribution",		HTstd),
/* #define _distribution			3 */
  ARTHEADERINIT("Expires",		HTstd),
/* #define _expires				4 */
  ARTHEADERINIT("From",			HTreq),
/* #define _from				5 */
  ARTHEADERINIT("Lines",		HTstd),
/* #define _lines				6 */
  ARTHEADERINIT("Message-ID",		HTreq),
/* #define _message_id				7 */
  ARTHEADERINIT("Newsgroups",		HTreq),
/* #define _newsgroups				8 */
  ARTHEADERINIT("Path",			HTreq),
/* #define _path				9 */
  ARTHEADERINIT("Reply-To",		HTstd),
/* #define _reply_to				10 */
  ARTHEADERINIT("Sender",		HTstd),
/* #define _sender				11 */
  ARTHEADERINIT("Subject",		HTreq),
/* #define _subject				12 */
  ARTHEADERINIT("Supersedes",		HTstd),
/* #define _supersedes				13 */
  ARTHEADERINIT("Bytes",		HTstd),
/* #define _bytes				14 */
  ARTHEADERINIT("Also-Control",		HTstd),
/* #define _alsocontrol				15 */
  ARTHEADERINIT("References",		HTstd),
/* #define _references				16 */
  ARTHEADERINIT("Xref",			HTsav),
/* #define _xref				17 */
  ARTHEADERINIT("Keywords",		HTstd),
/* #define _keywords				18 */
  ARTHEADERINIT("X-Trace",		HTstd),
/* #define _xtrace				19 */
  ARTHEADERINIT("Date-Received",	HTobs),
/* #define _datereceived			20 */
  ARTHEADERINIT("Posted",		HTobs),
/* #define _posted				21 */
  ARTHEADERINIT("Posting-Version",	HTobs),
/* #define _postintversion			22 */
  ARTHEADERINIT("Received",		HTobs),
/* #define _received				23 */
  ARTHEADERINIT("Relay-Version",	HTobs),
/* #define _relayversion			24 */
  ARTHEADERINIT("NNTP-Posting-Host",	HTstd),
/* #define _nntppostinghost			25 */
  ARTHEADERINIT("Followup-To",		HTstd),
/* #define _followupto				26 */
  ARTHEADERINIT("Organization",		HTstd),
/* #define _organization			27 */
  ARTHEADERINIT("Content-Type",		HTstd),
/* #define _contenttype				28 */
  ARTHEADERINIT("Content-Base",		HTstd),
/* #define _contentbase				29 */
  ARTHEADERINIT("Content-Disposition",	HTstd),
/* #define _contentdisposition			30 */
  ARTHEADERINIT("X-Newsreader",		HTstd),
/* #define _xnewsreader				31 */
  ARTHEADERINIT("X-Mailer",		HTstd),
/* #define _xmailer				32 */
  ARTHEADERINIT("X-Newsposter",		HTstd),
/* #define _xnewsposter				33 */
  ARTHEADERINIT("X-Cancelled-By",	HTstd),
/* #define _xcancelledby			34 */
  ARTHEADERINIT("X-Canceled-By",	HTstd),
/* #define _xcanceledby				35 */
  ARTHEADERINIT("Cancel-Key",		HTstd)
/* #define _cancelkey				36 */
};
/* #define MAX_ARTHEADER			37 */


/*
**  Signal handler to catch SIGTERM and similar signals and queue a clean
**  shutdown.
*/
static RETSIGTYPE
catch_terminate(int signal)
{
    GotTerminate = TRUE;
    killer_signal = signal;

#ifndef HAVE_SIGACTION
    xsignal(signal, catch_terminate);
#endif
}


/*
**  Memory allocation failure handler.  Instead of the default behavior of
**  just exiting, call abort to generate a core dump.
*/
static void
xmalloc_abort(const char *what, size_t size, const char *file, int line)
{
    fprintf(stderr, "SERVER cant %s %lu bytes at %s line %d: %m", what,
            (unsigned long) size, line, file);
    syslog(LOG_CRIT, "SERVER cant %s %lu bytes at %s line %d: %m", what,
           (unsigned long) size, line, file);
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
    SITEflushall(FALSE);
    CCclose();
    LCclose();
    NCclose();
    RCclose();
    ICDclose();
    HISclose();
    ARTclose();
    if (innconf->enableoverview) 
        OVclose();
    NGclose();
    SMshutdown();

#if DO_TCL
    TCLclose();
#endif

#if DO_PERL
    PerlFilter(FALSE);
    PerlClose();
#endif

#if DO_PYTHON
    PYclose();
#endif

    CHANshutdown();
    ClearInnConf();

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
    bool flag;
    static char		WHEN[] = "PID file";
    int			i;
    int			fd;
    char		buff[SMBUF], *q;
    FILE		*F;
    bool		ShouldFork;
    bool		ShouldRenumber;
    bool		ShouldSyntaxCheck;
    bool		val;
    bool		filter = TRUE;
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
    error_program_name = name;
    openlog(name, LOG_CONS | LOG_NDELAY, LOG_INN_SERVER);
    die_set_handlers(2, error_log_stderr, error_log_syslog_crit);
    warn_set_handlers(2, error_log_stderr, error_log_syslog_err);

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
    ShouldFork = TRUE;
    ShouldRenumber = FALSE;
    ShouldSyntaxCheck = FALSE;
    fd = -1;

    /* Set some options from inn.conf that can be overridden with
       command-line options if they exist, so read inn.conf first. */
    if (ReadInnConf() < 0)
        exit(1);

    /* Parse JCL. */
    CCcopyargv(av);
    while ((i = getopt(ac, av, "ac:Cdfi:l:m:o:Nn:p:P:rst:uH:T:X:")) != EOF)
	switch (i) {
	default:
	    Usage();
	    /* NOTREACHED */
	case 'a':
	    AnyIncoming = TRUE;
	    break;
	case 'c':
	    innconf->artcutoff = atoi(optarg) * 24 * 60 * 60;
	    break;
 	case 'C':
 	    DoCancels = FALSE;
  	    break;
	case 'd':
	    Debug = TRUE;
	    break;
	case 'f':
	    ShouldFork = FALSE;
	    break;
	case 'H':
	    RemoteLimit = atoi(optarg);
	    break;
	case 'i':
	    innconf->maxconnections = atoi(optarg);
	    break;
	case 'I':
	    if (innconf->bindaddress) DISPOSE(innconf->bindaddress);
	    innconf->bindaddress = COPY(optarg);
	    break;
	case 'l':
	    innconf->maxartsize = atol(optarg);
	    break;
	case 'm':
	    if (ModeReason)
		DISPOSE(ModeReason);
	    switch (*optarg) {
	    default:
		Usage();
		/* NOTREACHED */
	    case 'g':	Mode = OMrunning;	break;
	    case 'p':	Mode = OMpaused;	break;
	    case 't':	Mode = OMthrottled;	break;
	    }
	    if (Mode != OMrunning) {
		(void)sprintf(buff, "%sed from command line",
			Mode == OMpaused ? "Paus" : "Throttl");
		ModeReason = COPY(buff);
	    }
	    break;
	case 'N':
	    filter = FALSE;
	    break;
	case 'n':
	    switch (*optarg) {
	    default:
		Usage();
		/* NOTREACHED */
	    case 'n':	innconf->readerswhenstopped = FALSE;	break;
	    case 'y':	innconf->readerswhenstopped = TRUE;	break;
	    }
	    break;
	case 'o':
	    MaxOutgoing = atoi(optarg);
	    break;
	case 'p':
	    /* Silently ignore multiple -p flags, in case ctlinnd xexec
	       called inndstart. */
	    if (fd == -1)
		fd = atoi(optarg);
	    break;
	case 'P':
	    innconf->port = atoi(optarg);
	    break;
	case 'r':
	    ShouldRenumber = TRUE;
	    break;
	case 's':
	    ShouldSyntaxCheck = TRUE;
	    break;
	case 't':
	    TimeOut.tv_sec = atol(optarg);
	    break;
	case 'T':
	    RemoteTotal = atoi(optarg);
	    break;
	case 'u':
	    BufferedLogs = FALSE;
	    break;
	case 'X':
	    RemoteTimer = atoi(optarg);
	    break;
        case 'Z':
            StreamingOff = TRUE;
            break;
	}
    ac -= optind;
    if (ac != 0)
	Usage();
    if (ModeReason && innconf->readerswhenstopped)
	NNRPReason = COPY(ModeReason);

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
    Path.Used = strlen(innconf->pathhost) + 1;
    Path.Data = NEW(char, Path.Used + 1);
    sprintf(Path.Data, "%s!", innconf->pathhost);
    if (innconf->pathalias == NULL) {
	Pathalias.Used = 0;
	Pathalias.Data = NULL;
    } else {
	Pathalias.Used = strlen(innconf->pathalias) + 1;
	Pathalias.Data = NEW(char, Pathalias.Used + 1);
	sprintf(Pathalias.Data, "%s!", innconf->pathalias);
    }

    i = dbzneedfilecount();
    if (!fdreserve(2 + i)) { /* TEMPORARYOPEN, INND_HISTORY and i */
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
    syslog(L_NOTICE, "%s descriptors %d", LogName, i);
    if (MaxOutgoing == 0) {
	/* getfdlimit() - (stdio + dbz + cc + lc + rc + art + Overfdcount + fudge) */
	MaxOutgoing = i - (  3   +  3  +  2 +  1 +  1 +  1  + Overfdcount +  2  );
	syslog(L_NOTICE, "%s outgoing %d", LogName, MaxOutgoing);
    }

    /* See if another instance is alive. */
    if (PID == NULL)
	PID = COPY(cpcatpath(innconf->pathrun, _PATH_SERVERPID));
    if ((F = fopen(PID, "r")) != NULL) {
	if (fgets(buff, sizeof buff, F) != NULL
	 && ((pid = (pid_t) atol(buff)) > 0)
	 && (kill(pid, 0) > 0 || errno != ESRCH)) {
	    (void)syslog(L_FATAL, "%s already_running pid %ld", LogName,
	    (long) pid);
	    exit(1);
	}
	(void)fclose(F);
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
    HISsetup();
    CCsetup();
    LCsetup();
    RCsetup(fd);
    WIPsetup();
    NCsetup();
    ARTsetup();
    ICDsetup(TRUE);
    
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
	TCLfilter(FALSE);
#endif /* DO_TCL */

#if DO_PERL
    /* Load the Perl code */
    /* Make a temp copy because the path is a static var */
    q = COPY(cpcatpath(innconf->pathfilter, _PATH_PERL_STARTUP_INND));
    PERLsetup(q, (char *)cpcatpath(innconf->pathfilter,
                                   _PATH_PERL_FILTER_INND), "filter_art");
    PLxsinit();
    if (filter)
	PerlFilter(TRUE);
    DISPOSE(q);
#endif /* DO_PERL */

#if DO_PYTHON
    PYsetup();
    if (!filter)
	PYfilter(FALSE);
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
