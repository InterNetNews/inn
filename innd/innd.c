/*  $Id$
**
**  Variable definitions, miscellany, and main().
*/
#include "config.h"
#include "clibrary.h"
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/uio.h>

#ifdef DO_FAST_RESOLV
# include <arpa/nameser.h>
# include <resolv.h>
#endif

#define DEFINE_DATA
#include "innd.h"
#include "ov.h"

/* Some systems, such as FreeBSD 3.4 RELEASE, require sys/time.h, included
   by innd.h, to be included before sys/resource.h. */
#include <sys/resource.h>


#if defined(HAVE_SETBUFFER)
# define SETBUFFER(F, buff, size)	setbuffer((F), (buff), (size))
STATIC int	LogBufferSize = 4096;
#else
# define SETBUFFER(F, buff, size)	setbuf((F), (buff))
STATIC int	LogBufferSize = BUFSIZ;
#endif	/* defined(HAVE_SETBUFFER) */


BOOL		AmRoot = TRUE;
BOOL		BufferedLogs = TRUE;
BOOL		NNRPTracing = FALSE;
BOOL		StreamingOff = FALSE ; /* default is we can stream */
BOOL		Tracing = FALSE;
BOOL		DoCancels = TRUE;
char		LogName[] = "SERVER";
int		ErrorCount = IO_ERROR_COUNT;
int		SPOOLlen;
OPERATINGMODE	Mode = OMrunning;
int		RemoteLimit = REMOTELIMIT;
time_t		RemoteTimer = REMOTETIMER;
int		RemoteTotal = REMOTETOTAL;
BOOL		ThrottledbyIOError = FALSE;

#if	defined(__CENTERLINE__)
BOOL		Debug = TRUE;
#else
BOOL		Debug = FALSE;
#endif	/* defined(__CENTERLINE__) */

#if	defined(lint) || defined(__CENTERLINE__)
int		KeepLintQuiet = 0;
#endif	/* defined(lint) || defined(__CENTERLINE__) */


STATIC char	*ErrlogBuffer;
STATIC char	*LogBuffer;
STATIC char	*ERRLOG = NULL;
STATIC char	*LOG = NULL;
STATIC char	*PID = NULL;
STATIC UID_T	NewsUID;
STATIC GID_T	NewsGID;



/*
**  Sprintf a long into a buffer with enough leading zero's so that it
**  takes up width characters.  Don't add trailing NUL.  Return TRUE
**  if it fit.  Used for updating high-water marks in the active file
**  in-place.
*/
BOOL
FormatLong(p, value, width)
    register char	*p;
    register u_long	value;
    register int	width;
{
    for (p += width - 1; width-- > 0; ) {
	*p-- = (int)(value % 10) + '0';
	value /= 10;
    }
    return value == 0;
}


/*
**  Glue a string, a char, and a string together.  Useful for making
**  filenames.
*/
void
FileGlue(p, n1, c, n2)
    register char	*p;
    register char	*n1;
    char		c;
    register char	*n2;
{
    p += strlen(strcpy(p, n1));
    *p++ = c;
    (void)strcpy(p, n2);
}


/*
**  Turn any \r or \n in text into spaces.  Used to splice back multi-line
**  headers into a single line.
*/
STATIC char *
Join(text)
    register char	*text;
{
    register char	*p;

    for (p = text; *p; p++)
	if (*p == '\n' || *p == '\r')
	    *p = ' ';
    return text;
}


/*
**  Return a short name that won't overrun our bufer or syslog's buffer.
**  q should either be p, or point into p where the "interesting" part is.
*/
char *
MaxLength(p, q)
    char		*p;
    char		*q;
{
    static char		buff[80];
    register int	i;

    /* Already short enough? */
    i = strlen(p);
    if (i < sizeof buff - 1)
	return Join(p);

    /* Simple case of just want the begining? */
    if (q - p < sizeof buff - 4) {
	(void)strncpy(buff, p, sizeof buff - 4);
	(void)strcpy(&buff[sizeof buff - 4], "...");
    }
    /* Is getting last 10 characters good enough? */
    else if ((p + i) - q < 10) {
	(void)strncpy(buff, p, sizeof buff - 14);
	(void)strcpy(&buff[sizeof buff - 14], "...");
	(void)strcpy(&buff[sizeof buff - 11], &p[i - 10]);
    }
    else {
	/* Not in last 10 bytes, so use double elipses. */
	(void)strncpy(buff, p, sizeof buff - 17);
	(void)strcpy(&buff[sizeof buff - 17], "...");
	(void)strncpy(&buff[sizeof buff - 14], &q[-5], 10);
	(void)strcpy(&buff[sizeof buff - 4], "...");
    }
    return Join(buff);
}


/*
**  Split text into comma-separated fields.  Return an allocated
**  NULL-terminated array of the fields within the modified argument that
**  the caller is expected to save or free.  We don't use strchr() since
**  the text is expected to be either relatively short or "comma-dense."
*/
char **
CommaSplit(text)
    char		*text;
{
    register int	i;
    register char	*p;
    register char	**av;
    char		**save;

    /* How much space do we need? */
    for (i = 2, p = text; *p; p++)
	if (*p == ',')
	    i++;

    for (av = save = NEW(char*, i), *av++ = p = text; *p; )
	if (*p == ',') {
	    *p++ = '\0';
	    *av++ = p;
	}
	else
	    p++;
    *av = NULL;
    return save;
}


/*
**  Do we need a shell for the command?  If not, av is filled in with
**  the individual words of the command and the command is modified to
**  have NUL's inserted.
*/
BOOL
NeedShell(p, av, end)
    register char	*p;
    register char	**av;
    register char	**end;
{
    static char		Metachars[] = ";<>|*?[]{}()#$&=`'\"\\~\n";
    register char	*q;

    /* We don't use execvp(); works for users, fails out of /etc/rc. */
    if (*p != '/')
	return TRUE;
    for (q = p; *q; q++)
	if (strchr(Metachars, *q) != NULL)
	    return TRUE;

    for (end--; av < end; ) {
	/* Mark this word, check for shell meta-characters. */
	for (*av++ = p; *p && !ISWHITE(*p); p++)
	    continue;

	/* If end of list, we're done. */
	if (*p == '\0') {
	    *av = NULL;
	    return FALSE;
	}

	/* Skip whitespace, find next word. */
	for (*p++ = '\0'; ISWHITE(*p); p++)
	    continue;
	if (*p == '\0') {
	    *av = NULL;
	    return FALSE;
	}
    }

    /* Didn't fit. */
    return TRUE;
}


/*
**  Spawn a process, with I/O redirected as needed.  Return the PID or -1
**  (and a syslog'd message) on error.
*/
PID_T
Spawn(niceval, fd0, fd1, fd2, av)
    int		niceval;
    int		fd0;
    int		fd1;
    int		fd2;
    char	*av[];
{
    static char	NOCLOSE[] = "%s cant close %d in %s %m";
    static char	NODUP2[] = "%s cant dup2 %d to %d in %s %m";
    PID_T	i;

    /* Fork; on error, give up.  If not using the patched dbz, make
     * this call fork! */
    i = FORK();
    if (i == -1) {
	syslog(L_ERROR, "%s cant fork %s %m", LogName, av[0]);
	return -1;
    }

    /* If parent, do nothing. */
    if (i > 0)
	return i;

    /* Child -- do any I/O redirection. */
    if (fd0 != 0) {
	if (dup2(fd0, 0) < 0) {
	    syslog(L_FATAL, NODUP2, LogName, fd0, 0, av[0]);
	    _exit(1);
	}
	if (fd0 != fd1 && fd0 != fd2 && close(fd0) < 0)
	    syslog(L_ERROR, NOCLOSE, LogName, fd0, av[0]);
    }
    if (fd1 != 1) {
	if (dup2(fd1, 1) < 0) {
	    syslog(L_FATAL, NODUP2, LogName, fd1, 1, av[0]);
	    _exit(1);
	}
	if (fd1 != fd2 && close(fd1) < 0)
	    syslog(L_ERROR, NOCLOSE, LogName, fd1, av[0]);
    }
    if (fd2 != 2) {
	if (dup2(fd2, 2) < 0) {
	    syslog(L_FATAL, NODUP2, LogName, fd2, 2, av[0]);
	    _exit(1);
	}
	if (close(fd2) < 0)
	    syslog(L_ERROR, NOCLOSE, LogName, fd2, av[0]);
    }
    CloseOnExec(0, FALSE);
    CloseOnExec(1, FALSE);
    CloseOnExec(2, FALSE);

    /* Try to set our permissions. */
    if (niceval != 0)
	if (nice(niceval) == -1) 
		syslog(L_ERROR, "Could not nice child to %d: %m", niceval);
    if (setgid(NewsGID) == -1)
	syslog(L_ERROR, "%s cant setgid in %s %m", LogName, av[0]);
    if (setuid(NewsUID) == -1)
	syslog(L_ERROR, "%s cant setuid in %s %m", LogName, av[0]);

    /* Start the desired process (finally!). */
    (void)execv(av[0], av);
    syslog(L_FATAL, "%s cant exec in %s %m", LogName, av[0]);
    _exit(1);
    /* NOTREACHED */
}


/*
**  Stat our control directory and see who should own things.
*/
STATIC BOOL
GetNewsOwnerships()
{
    struct stat	Sb;

    /* Make sure item exists and is of the right type. */
    if (stat(innconf->pathrun, &Sb) < 0)
	return FALSE;
    if (!S_ISDIR(Sb.st_mode))
	return FALSE;
    NewsUID = Sb.st_uid;
    NewsGID = Sb.st_gid;
    return TRUE;
}


/*
**  Change the onwership of a file.
*/
void
xchown(p)
    char	*p;
{
    if (chown(p, NewsUID, NewsGID) < 0)
	syslog(L_ERROR, "%s cant chown %s %m", LogName, p);
}


/*
**  Flush one log file, with pessimistic size of working filename buffer.
*/
void
ReopenLog(F)
    FILE	*F;
{
    char	buff[SMBUF];
    char	*Name;
    char	*Buffer;
    int		mask;

    if (Debug)
	return;
    if (F == Log) {
	Name = LOG;
	Buffer = LogBuffer;
    }
    else {
	Name = ERRLOG;
	Buffer = ErrlogBuffer;
    }

    FileGlue(buff, Name, '.', "old");
    if (rename(Name, buff) < 0)
	syslog(L_ERROR, "%s cant rename %s to %s %m", LogName, Name, buff);
    mask = umask(033);
    if (freopen(Name, "a", F) != F) {
	syslog(L_FATAL, "%s cant freopen %s %m", LogName, Name);
	exit(1);
    }
    (void)umask(mask);
    if (AmRoot)
	xchown(Name);
    if (BufferedLogs)
	SETBUFFER(F, Buffer, LogBufferSize);
}


/*
**  Function called when memory allocation fails.
*/
static int
AllocationFailure(const char *what, size_t size, const char *file, int line)
{
    syslog(L_FATAL, "%s cant %s %lu bytes at line %d of %s: %m", LogName,
           what, size, line, file);
    abort();
}


/*
**  We ran out of space or other I/O error, throttle ourselves.
*/
void
ThrottleIOError(char *when)
{
    char	buff[SMBUF];
    STRING	p;
    int		oerrno;

    if (Mode == OMrunning) {
	oerrno = errno;
	if (Reservation) {
	    DISPOSE(Reservation);
	    Reservation = NULL;
	}
	(void)sprintf(buff, "%s writing %s file -- throttling",
	    strerror(oerrno), when);
	if ((p = CCblock(OMthrottled, buff)) != NULL)
	    syslog(L_ERROR, "%s cant throttle %s", LogName, p);
	syslog(L_FATAL, "%s throttle %s", LogName, buff);
	errno = oerrno;
	ThrottledbyIOError = TRUE;
    }
}

/*
**  No matching storage.conf, throttle ourselves.
*/
void
ThrottleNoMatchError(void)
{
    char	buff[SMBUF];
    STRING	p;
    int		oerrno;

    if (Mode == OMrunning) {
	if (Reservation) {
	    DISPOSE(Reservation);
	    Reservation = NULL;
	}
	(void)sprintf(buff, "%s storing article -- throttling",
	    SMerrorstr);
	if ((p = CCblock(OMthrottled, buff)) != NULL)
	    syslog(L_ERROR, "%s cant throttle %s", LogName, p);
	syslog(L_FATAL, "%s throttle %s", LogName, buff);
	ThrottledbyIOError = TRUE;
    }
}


/*
**  Close down all parts of the system (e.g., before calling exit or exec).
*/
void
JustCleanup()
{
    SITEflushall(FALSE);
    /* PROCclose(FALSE); */
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

#if defined(DO_TCL)
    TCLclose();
#endif /* defined(DO_TCL) */
#if defined(DO_PERL)
    PerlFilter (FALSE) ;
    PerlClose();
#endif /* defined(DO_PERL) */
#if defined(DO_PYTHON)
    PYclose();
#endif /* defined(DO_TCL) */

    CHANshutdown();
    ClearInnConf();

    (void)sleep(1);
    /* PROCclose(TRUE); */
    if (unlink(PID) < 0 && errno != ENOENT)
	syslog(L_ERROR, "%s cant unlink %s %m", LogName, PID);
}


/*
**  The name is self-explanatory.
*/
NORETURN
CleanupAndExit(x, why)
    int		x;
    char	*why;
{
    JustCleanup();
    if (why)
	syslog(L_FATAL, "%s shutdown %s", LogName, why);
    else
	syslog(L_FATAL, "%s shutdown received signal %d",
	    LogName, KillerSignal);
    exit(x);
}


#if defined(HAVE_RLIMIT) && defined(RLIMIT_NOFILE)
/*
**  Set the limit on the number of open files we can have.  I don't
**  like having to do this.
*/
STATIC void
SetDescriptorLimit(i)
    int			i;
{
    struct rlimit	rl;

    if (getrlimit(RLIMIT_NOFILE, &rl) < 0) {
	syslog(L_ERROR, "%s cant getrlimit(NOFILE) %m", LogName);
	return;
    }
    rl.rlim_cur = i;
    if (setrlimit(RLIMIT_NOFILE, &rl) < 0) {
	syslog(L_ERROR, "%s cant setrlimit(NOFILE) %d %m", LogName, i);
	return;
    }
}
#endif /* HAVE_RLIMIT && RLIMIT_NOFILE */


/*
**  Signal handler to catch SIGTERM and queue a clean shutdown.
*/
STATIC SIGHANDLER
CatchTerminate(s)
    int		s;
{
    GotTerminate = TRUE;
    KillerSignal = s;
#ifndef HAVE_SIGACTION
    xsignal(s, CatchTerminate);
#endif
}


/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage()
{
    (void)fprintf(stderr, "Usage error.\n");
    exit(1);
}


int main(int ac, char *av[])
{
    static char		WHEN[] = "PID file";
    int			i;
    int			fd;
    int			logflags;
    char		buff[SMBUF];
    char		*p;
    FILE		*F;
    BOOL		ShouldFork;
    BOOL		ShouldRenumber;
    BOOL		ShouldSyntaxCheck;
    BOOL		val;
    PID_T		pid;
#if	defined(_DEBUG_MALLOC_INC)
    union malloptarg	m;
#endif	/* defined(_DEBUG_MALLOC_INC) */

    /* Set up the pathname, first thing. */
    path = av[0];
    if (path == NULL || *path == '\0')
	path = "innd";
    else if ((p = strrchr(path, '/')) != NULL)
	path = p + 1;
    ONALLOCFAIL(AllocationFailure);

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
    logflags = L_OPENLOG_FLAGS | LOG_NOWAIT;
    fd = -1;

#if	defined(DO_FAST_RESOLV)
    /* We only use FQDN's in the hosts.nntp file. */
    _res.options &= ~(RES_DEFNAMES | RES_DNSRCH);
#endif	/* defined(DO_FAST_RESOLV) */

    openlog(path, logflags, LOG_INN_SERVER);
  /* Set some options from inn.conf(5) that can be overridden with
     command-line options if they exist */
    if (ReadInnConf() < 0) exit(1);
    LOG = COPY(cpcatpath(innconf->pathlog, _PATH_LOGFILE));
    ERRLOG = COPY(cpcatpath(innconf->pathlog, _PATH_ERRLOG));

    /* Parse JCL. */
    CCcopyargv(av);
    while ((i = getopt(ac, av, "ac:Cdfi:l:m:o:n:p:P:rst:uH:T:X:")) != EOF)
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
	case 'd':
	    Debug = TRUE;
#if	defined(LOG_PERROR)
	    logflags = LOG_PERROR | (logflags & ~LOG_CONS);
#endif	/* defined(LOG_PERROR) */
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
	     * called inndstart. */
	    if (fd == -1) {
		fd = atoi(optarg);
		AmRoot = FALSE;
	    }
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
 	case 'C':
 	    DoCancels = FALSE;
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
	if ((p = (char *) CCcheckfile((char **)NULL)) == NULL)
	    exit(0);
	(void)fprintf(stderr, "%s\n", p + 2);
	exit(1);
    }

    SPOOLlen = strlen(innconf->patharticles);
    /* Go to where the data is. */
    if (chdir(innconf->patharticles) < 0) {
	syslog(L_FATAL, "%s cant chdir %s %m", LogName, innconf->patharticles);
	exit(1);
    }

    val = TRUE;

    /* Get the Path entry. */
    if (innconf->pathhost == NULL) {
	syslog(L_FATAL, "%s No pathhost set", LogName);
	exit(1);
    }
    Path.Used = strlen(innconf->pathhost) + 1;
    Path.Data = NEW(char, Path.Used + 1);
    (void)sprintf(Path.Data, "%s!", innconf->pathhost);
    if (innconf->pathalias == NULL) {
	Pathalias.Used = 0;
	Pathalias.Data = NULL;
    } else {
	Pathalias.Used = strlen(innconf->pathalias) + 1;
	Pathalias.Data = NEW(char, Pathalias.Used + 1);
	(void)sprintf(Pathalias.Data, "%s!", innconf->pathalias);
    }

#if	!defined(__CENTERLINE__)
    /* Set standard input to /dev/null. */
    if ((i = open("/dev/null", O_RDWR)) < 0) {
	syslog(L_FATAL, "%s cant open /dev/null %m", LogName);
	exit(1);
    }
    if (dup2(i, 0) != 0)
	syslog(L_NOTICE, "%s cant dup2 %d to 0 %m", LogName, i);
    (void)close(i);
#endif	/* !defined(__CENTERLINE__) */
    i = dbzneedfilecount();
    if (!fdreserve(2 + i)) { /* TEMPORARYOPEN, INND_HISTORY and i */
	syslog(L_FATAL, "%s cant reserve file descriptors %m", LogName);
	exit(1);
    }

    /* Set up our permissions. */
    (void)umask(NEWSUMASK);
    if (!GetNewsOwnerships()) {
	syslog(L_FATAL, "%s internal cant stat control directory %m", LogName);
	exit(1);
    }
    if (fd != -1 && setgid(NewsGID) < 0)
	syslog(L_ERROR, "%s cant setgid running as %d not %d %m",
	    LogName, (int)getgid(), (int)NewsGID);

    if (Debug) {
	Log = stdout;
	Errlog = stderr;
	(void)xsignal(SIGINT, CatchTerminate);
    }
    else {
	if (ShouldFork) {
	    /* Become a server. */
	    i = fork();
	    if (i < 0) {
		syslog(L_FATAL, "%s cant fork %m", LogName);
		exit(1);
	    }
	    if (i > 0)
		_exit(0);

#if	defined(TIOCNOTTY)
	    /* Disassociate from terminal. */
	    if ((i = open("/dev/tty", O_RDWR)) >= 0) {
		if (ioctl(i, TIOCNOTTY, (char *)NULL) < 0)
		    syslog(L_ERROR, "%s cant ioctl(TIOCNOTTY) %m", LogName);
		if (close(i) < 0)
		    syslog(L_ERROR, "%s cant close /dev/tty %m", LogName);
	    }
#endif	/* defined(TIOCNOTTY) */
#if	defined(HAVE_SETSID)
	    (void)setsid();
#endif	/* defined(HAVE_SETSID) */
	}

	/* Open the Log. */
	(void)fclose(stdout);
	if ((Log = fopen(LOG, "a")) == NULL) {
	    syslog(L_FATAL, "%s cant fopen %s %m", LogName, LOG);
	    exit(1);
	}
	if (AmRoot)
	    xchown(LOG);
	if (BufferedLogs && (LogBuffer = NEW(char, LogBufferSize)) != NULL)
	    SETBUFFER(Log, LogBuffer, LogBufferSize);

	/* Open the Errlog. */
	(void)fclose(stderr);
	if ((Errlog = fopen(ERRLOG, "a")) == NULL) {
	    syslog(L_FATAL, "%s cant fopen %s %m", LogName, ERRLOG);
	    exit(1);
	}
	if (AmRoot)
	    xchown(ERRLOG);
	if (BufferedLogs && (ErrlogBuffer = NEW(char, LogBufferSize)) != NULL)
	    SETBUFFER(Errlog, ErrlogBuffer, LogBufferSize);
    }

    if (innconf->enableoverview) {
	if (!OVopen(OV_WRITE)) {
	    syslog(L_FATAL, "%s cant open overview method", LogName);
	    exit(1);
	}
    }

    /* Set number of open channels. */
#if defined(HAVE_RLIMIT) && defined(RLIMIT_NOFILE)
    if (AmRoot && innconf->rlimitnofile >= 0)
	SetDescriptorLimit(innconf->rlimitnofile);
#endif /* HAVE_RLIMIT && RLIMIT_NOFILE */
    /* Get number of open channels. */
    if ((i = getfdcount()) < 0) {
	syslog(L_FATAL, "%s cant getfdcount %m", LogName);
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
	/* getfdcount() - (stdio + dbz + cc + lc + rc + art + Overfdcount + fudge) */
	MaxOutgoing = i - (  3   +  3  +  2 +  1 +  1 +  1  + Overfdcount +  2  );
	syslog(L_NOTICE, "%s outgoing %d", LogName, MaxOutgoing);
    }

    /* See if another instance is alive. */
    if (PID == NULL)
	PID = COPY(cpcatpath(innconf->pathrun, _PATH_SERVERPID));
    if ((F = fopen(PID, "r")) != NULL) {
	if (fgets(buff, sizeof buff, F) != NULL
	 && ((pid = (PID_T) atol(buff)) > 0)
	 && (kill(pid, 0) > 0 || errno != ESRCH)) {
	    (void)syslog(L_FATAL, "%s already_running pid %ld", LogName,
	    (long) pid);
	    exit(1);
	}
	(void)fclose(F);
    }

    if (GetTimeInfo(&Now) < 0)
	syslog(L_ERROR, "%s cant gettimeinfo %m", LogName);

    /* Set up the various parts of the system.  Channel feeds start
     * processes so call PROCsetup before ICDsetup.  NNTP needs to know
     * if it's a slave, so call RCsetup before NCsetup. */
    (void)xsignal(SIGTERM, CatchTerminate);
#if	defined(SIGDANGER)
    (void)xsignal(SIGDANGER, CatchTerminate);
#endif	/* defined(SIGDANGER) */
    CHANsetup(i);
    PROCsetup(10);
    HISsetup();
    CCsetup();
    LCsetup();
    RCsetup(fd);
    WIPsetup();
    NCsetup(i);
    ARTsetup();
    ICDsetup(TRUE);
    
    val = TRUE;
    if (!SMsetup(SM_RDWR, (void *)&val) || !SMsetup(SM_PREOPEN, (void *)&val)) {
	syslog(L_FATAL, "%s cant setup the storage subsystem", LogName);
	exit(1);
    }
    if (!SMinit()) {
	syslog(L_FATAL, "%s cant initialize the storage subsystem %s", LogName, SMerrorstr);
	exit(1);
    }

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

#if defined(DO_TCL)
    TCLsetup();
#endif /* defined(DO_TCL) */

#if defined(DO_PERL)
    /* Load the Perl code */
    /* Make a temp copy because the path is a static var */
    p = COPY(cpcatpath(innconf->pathfilter, _PATH_PERL_STARTUP_INND));
    PERLsetup(p, cpcatpath(innconf->pathfilter, _PATH_PERL_FILTER_INND),
				"filter_art");
    PLxsinit();
    PerlFilter (TRUE) ;
    DISPOSE(p);
#endif /* defined(DO_PERL) */

#if defined(DO_PYTHON)
    PYsetup();
#endif /* (DO_PYTHON) */
 
    /* And away we go... */
    if (ShouldRenumber) {
	syslog(L_NOTICE, "%s renumbering", LogName);
	if (!ICDrenumberactive()) {
	    syslog(L_FATAL, "%s cant renumber", LogName);
	    exit(1);
	}
    }
    syslog(L_NOTICE, "%s starting", LogName);
    CHANreadloop();
    CleanupAndExit(1, "CHANreadloop returned");
    /* NOTREACHED */
    return 1;
}
