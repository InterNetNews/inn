/*
**  innxbatch.c
**
**  $Id$
**
**  Transmit batches to remote site, using the XBATCH command
**  Modelled after innxmit.c and nntpbatch.c
**
**  Invocation:
**	innxbatch [options] <serverhost> <file> ...
#ifdef FROMSTDIN
**	innxbatch -i <serverhost>
#endif FROMSTDIN
**  will connect to serverhost's nntp port, and transfer the named files,
**  with an xbatch command for every file. Files that have been sent
**  successfully are unlink()ed. In case of any error, innxbatch terminates
**  and leaves any remaining files untouched, for later transmission.
**  Options:
**	-D	increase debug level
**	-v	report statistics to stdout
#ifdef FROMSTDIN
**	-i	read batch file names from stdin instead from command line.
**		For each successfully transmitted batch, an OK is printed on
**		stdout, to indicate that another file name is expected.
#endif
**	-t	Timeout for connection attempt
**	-T	Timeout for batch transfers.
**  We do not use any file locking. At worst, a batch could be transmitted
**  twice in parallel by two independant invocations of innxbatch.
**  To prevent this, innxbatch should e invoked by a shell script that uses
**  shlock(1) to achieve mutual exclusion.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <setjmp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
/*#include <sys/uio.h>*/
#include "nntp.h"
#include <syslog.h> 
#include "libinn.h"
#include "macros.h"

/*
** Syslog formats - collected together so they remain consistent
*/
STATIC char	STAT1[] =
	"%s stats offered %lu accepted %lu refused %lu rejected %lu";
STATIC char	STAT2[] = "%s times user %.3f system %.3f elapsed %.3f";
STATIC char	CANT_CONNECT[] = "%s connect failed %s";
STATIC char	CANT_AUTHENTICATE[] = "%s authenticate failed %s";
STATIC char	XBATCH_FAIL[] = "%s xbatch failed %s";
STATIC char	UNKNOWN_REPLY[] = "Unknown reply after sending batch -- %s";
STATIC char	CANNOT_UNLINK[] = "cannot unlink %s: %m";
/*
**  Global variables.
*/
STATIC BOOL		Debug = 0;
STATIC BOOL		STATprint;
STATIC char		*REMhost;
STATIC double		STATbegin;
STATIC double		STATend;
STATIC char		*XBATCHname;
STATIC int		FromServer;
STATIC int		ToServer;
STATIC SIGVAR		GotAlarm;
STATIC SIGVAR		GotInterrupt;
STATIC SIGVAR		JMPyes;
STATIC jmp_buf		JMPwhere;
STATIC unsigned long	STATaccepted;
STATIC unsigned long	STAToffered;
STATIC unsigned long	STATrefused;
STATIC unsigned long	STATrejected;

/*
**  Send a line to the server. \r\n will be appended
*/
STATIC BOOL
REMwrite(fd, p)
register int	fd;
register char	*p;
{
  register int		i;
  register int		err;
  register char		*dest;
  static char		buff[NNTP_STRLEN];

  for (dest = buff, i = 0; p[i]; ) *dest++ = p[i++];
  *dest++ = '\r';
  *dest++ = '\n';
  *dest++ = '\0';

  for (dest = buff, i+=2; i; dest += err, i -= err) {
    err = write(fd, dest, i);
    if (err < 0) {
      (void)fprintf(stderr, "cant write %s to %s: %s\n",
		    REMhost, dest, strerror(errno));
      return FALSE;
    }
  }
  if (Debug)
    (void)fprintf(stderr, "> %s\n", p);

  return TRUE;
}

/*
**  Print transfer statistics, clean up, and exit.
*/
STATIC NORETURN
ExitWithStats(x)
int			x;
{
  static char		QUIT[] = "quit";
  TIMEINFO		Now;
  double		usertime;
  double		systime;

  (void)REMwrite(ToServer, QUIT);

  (void)GetTimeInfo(&Now);
  STATend = TIMEINFOasDOUBLE(Now);
  if (GetResourceUsage(&usertime, &systime) < 0) {
    usertime = 0;
    systime = 0;
  }

  if (STATprint) {
    (void)printf(STAT1,
	REMhost, STAToffered, STATaccepted, STATrefused, STATrejected);
    (void)printf("\n");
    (void)printf(STAT2, REMhost, usertime, systime, STATend - STATbegin);
    (void)printf("\n");
  }

  syslog(L_NOTICE, STAT1,
	 REMhost, STAToffered, STATaccepted, STATrefused, STATrejected);
  syslog(L_NOTICE, STAT2, REMhost, usertime, systime, STATend - STATbegin);

  exit(x);
  /* NOTREACHED */
}


/*
**  Clean up the NNTP escapes from a line.
*/
STATIC char *
REMclean(buff)
    char	*buff;
{
    char	*p;

    if ((p = strchr(buff, '\r')) != NULL)
	*p = '\0';
    if ((p = strchr(buff, '\n')) != NULL)
	*p = '\0';

    /* The dot-escape is only in text, not command responses. */
    return buff;
}


/*
**  Read a line of input, with timeout. We expect only answer lines, so
**  we ignore \r\n-->\n mapping and the dot escape.
**  Return TRUE if okay, *or we got interrupted.*
*/
STATIC BOOL
REMread(start, size)
char	*start;
int	size;
{
  register char		*p;
  register char		*h;
  struct timeval	t;
  FDSET			rmask;
  int			i;

  for (p = start; size; ) {
    FD_ZERO(&rmask);
    FD_SET(FromServer, &rmask);
    t.tv_sec = 10 * 60;
    t.tv_usec = 0;
    i = select(FromServer + 1, &rmask, (FDSET *)NULL, (FDSET *)NULL, &t);
    if (GotInterrupt) return TRUE;
    if (i < 0) {
      if (errno == EINTR) continue;
      else return FALSE;
    }
    if (i == 0 || !FD_ISSET(FromServer, &rmask)) return FALSE;
    i = read(FromServer, p, size-1);
    if (GotInterrupt) return TRUE;
    if (i <= 0) return FALSE;
    h = p;
    p += i;
    size -= i;
    for ( ; h < p; h++) {
      if (h > start && '\n' == *h && '\r' == h[-1]) {
	*h = h[-1] = '\0';
	size = 0;
      }
    }
  }

  if (Debug)
    (void)fprintf(stderr, "< %s\n", start);

  return TRUE;
}


/*
**  Handle the interrupt.
*/
static void
Interrupted()
{
  (void)fprintf(stderr, "Interrupted\n");
  ExitWithStats(1);
}


/*
**  Send a whole xbatch to the server. Uses the global variables
**  REMbuffer & friends
*/
STATIC BOOL
REMsendxbatch(fd, buf, size)
int fd;
char *buf;
int size;
{
  register char	*p;
  int		i;
  int		err;

  for (i = size, p = buf; i; p += err, i -= err) {
    err = write(fd, p, i);
    if (err < 0) {
      (void)fprintf(stderr, "cant write xbatch to %s: %s\n",
		    REMhost, strerror(errno));
      return FALSE;
    }
  }
  if (GotInterrupt) Interrupted();
  if (Debug)
    (void)fprintf(stderr, "> [%d bytes of xbatch]\n", size);

  /* What did the remote site say? */
  if (!REMread(buf, size)) {
    (void)fprintf(stderr, "No reply after sending xbatch, %s\n",
		  strerror(errno));
    return FALSE;
  }
  if (GotInterrupt) Interrupted();
  
  /* Parse the reply. */
  switch (atoi(buf)) {
  default:
    (void)fprintf(stderr, "Unknown reply after sending batch -- %s", buf);
    syslog(L_ERROR, UNKNOWN_REPLY, buf);
    return FALSE;
    /* NOTREACHED */
    break;
  case NNTP_RESENDIT_VAL:
  case NNTP_GOODBYE_VAL:
    syslog(L_NOTICE, XBATCH_FAIL, REMhost, buf);
    STATrejected++;
    return FALSE;
    /* NOTREACHED */
    break;
  case NNTP_OK_XBATCHED_VAL:
    STATaccepted++;
    if (Debug) (void)fprintf(stderr, "will unlink(%s)\n", XBATCHname);
    if (unlink(XBATCHname)) {
      /* probably another incarantion was faster, so avoid further duplicate
       * work
       */
      (void)fprintf(stderr, "cannot unlink %s: %s\n",
		    XBATCHname, strerror(errno));
      syslog(L_NOTICE, CANNOT_UNLINK, XBATCHname);
      return FALSE;
    }
    break;
  }
  
  /* Article sent */
  return TRUE;
}

/*
**  Mark that we got interrupted.
*/
STATIC SIGHANDLER
CATCHinterrupt(s)
    int		s;
{
    GotInterrupt = TRUE;
    /* Let two interrupts kill us. */
    (void)xsignal(s, SIG_DFL);
}


/*
**  Mark that the alarm went off.
*/
/* ARGSUSED0 */
STATIC SIGHANDLER
CATCHalarm(s)
    int		s;
{
    GotAlarm = TRUE;
    if (JMPyes)
	longjmp(JMPwhere, 1);
}


/*
**  Print a usage message and exit.
*/
STATIC NORETURN
Usage()
{
    (void)fprintf(stderr,
	"Usage: innxbatch [-D] [-v] [-t#] [-T#] host file ...\n");
#ifdef FROMSTDIN
    (void)fprintf(stderr,
        "       innxbatch [-D] [-v] [-t#] [-T#] -i host\n");
#endif
    exit(1);
}


int
main(ac, av)
int ac;
char *av[];
{
  int			i;
  register char		*p;
  TIMEINFO		Now;
  FILE			*From;
  FILE			*To;
  char			buff[NNTP_STRLEN];
  SIGHANDLER		(*old)();
  unsigned int		ConnectTimeout;
  unsigned int		TotalTimeout;
  struct stat		statbuf;
  int			fd;
  int			err;
  char			*XBATCHbuffer = NULL;
  int			XBATCHbuffersize = 0;
  int			XBATCHsize;

  /* Set defaults. */
  if (ReadInnConf() < 0) exit(1);
  ConnectTimeout = 0;
  TotalTimeout = 0;
  (void)umask(NEWSUMASK);

  /* Parse JCL. */
  while ((i = getopt(ac, av, "Dit:T:v")) != EOF)
    switch (i) {
    default:
      Usage();
      /* NOTREACHED */
      break;
    case 'D':
      Debug++;
      break;
#ifdef FROMSTDIN
    case 'i':
      FromStdin = TRUE;
      break;
#endif
    case 't':
      ConnectTimeout = atoi(optarg);
      break;
    case 'T':
      TotalTimeout = atoi(optarg);
      break;
    case 'v':
      STATprint = TRUE;
      break;
    }
  ac -= optind;
  av += optind;

  /* Parse arguments; host and filename. */
  if (ac < 2)
    Usage();
  REMhost = av[0];
  ac--;
  av++;

  (void)openlog("innxmit", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);

  /* Open a connection to the remote server. */
  if (ConnectTimeout) {
    GotAlarm = FALSE;
    old = xsignal(SIGALRM, CATCHalarm);
    JMPyes = TRUE;
    if (setjmp(JMPwhere)) {
      (void)fprintf(stderr, "Can't connect to %s, timed out\n",
		    REMhost);
      exit(1);
    }
    (void)alarm(ConnectTimeout);
  }
  if (NNTPconnect(REMhost, NNTP_PORT, &From, &To, buff) < 0 || GotAlarm) {
    i = errno;
    (void)fprintf(stderr, "Can't connect to %s, %s\n",
		  REMhost, buff[0] ? REMclean(buff) : strerror(errno));
    if (GotAlarm)
      syslog(L_NOTICE, CANT_CONNECT, REMhost, "timeout");
    else
      syslog(L_NOTICE, CANT_CONNECT, REMhost,
	     buff[0] ? REMclean(buff) : strerror(i));
    exit(1);
  }

  if (Debug)
    (void)fprintf(stderr, "< %s\n", REMclean(buff));
  if (NNTPsendpassword(REMhost, From, To) < 0 || GotAlarm) {
    i = errno;
    (void)fprintf(stderr, "Can't authenticate with %s, %s\n",
		  REMhost, strerror(errno));
    syslog(L_ERROR, CANT_AUTHENTICATE,
	   REMhost, GotAlarm ? "timeout" : strerror(i));
    /* Don't send quit; we want the remote to print a message. */
    exit(1);
  }
  if (ConnectTimeout) {
    (void)alarm(0);
    (void)xsignal(SIGALRM, old);
    JMPyes = FALSE;
  }
  
  /* We no longer need standard I/O. */
  FromServer = fileno(From);
  ToServer = fileno(To);

#if	defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF)
  i = 24 * 1024;
  if (setsockopt(ToServer, SOL_SOCKET, SO_SNDBUF, (char *)&i, sizeof i) < 0)
    (void)perror("cant setsockopt(SNDBUF)");
  if (setsockopt(FromServer, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof i) < 0)
    (void)perror("cant setsockopt(RCVBUF)");
#endif	/* defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF) */

  GotInterrupt = FALSE;
  GotAlarm = FALSE;

  /* Set up signal handlers. */
  (void)xsignal(SIGHUP, CATCHinterrupt);
  (void)xsignal(SIGINT, CATCHinterrupt);
  (void)xsignal(SIGTERM, CATCHinterrupt);
  (void)xsignal(SIGPIPE, SIG_IGN);
  if (TotalTimeout) {
    (void)xsignal(SIGALRM, CATCHalarm);
    (void)alarm(TotalTimeout);
  }

  /* Start timing. */
  if (GetTimeInfo(&Now) < 0) {
    (void)fprintf(stderr, "Can't get time, %s\n", strerror(errno));
    exit(1);
  }
  STATbegin = TIMEINFOasDOUBLE(Now);


  /* main loop over all specified files */
  for (XBATCHname = *av; ac && (XBATCHname = *av); av++, ac--) {

    if (Debug) (void)fprintf(stderr, "will work on %s\n", XBATCHname);

    if (GotAlarm) {
      (void)fprintf(stderr, "Timed out\n");
      ExitWithStats(1);
    }
    if (GotInterrupt) Interrupted();

    if ((fd = open(XBATCHname, O_RDONLY, 0)) < 0) {
      (void)fprintf(stderr, "Can't open \"%s\", %s - skipping it\n",
		    XBATCHname, strerror(errno));
      continue;
    }

    if (fstat(fd, &statbuf)) {
      (void)fprintf(stderr, "Can't stat \"%s\", %s - skipping it\n",
		    XBATCHname, strerror(errno));
      (void)close(i);
      continue;
    }

    XBATCHsize = statbuf.st_size;
    if (XBATCHsize == 0) {
      (void)fprintf(stderr, "Batch file \"%s\" is zero length, - skipping it\n",
		    XBATCHname);
      (void)close(i);
      (void)unlink(XBATCHname);
      continue;
    } else if (XBATCHsize > XBATCHbuffersize) {
      XBATCHbuffersize = XBATCHsize;
      if (XBATCHbuffer) DISPOSE(XBATCHbuffer);
      XBATCHbuffer = NEW(char, XBATCHsize);
    }

    for (i = XBATCHsize, p = XBATCHbuffer; i; i -= err, p+= err) {
      err = read(fd, p, i);
      if (err < 0) {
	(void)fprintf(stderr, "error reading %s: %s - skipping it\n",
		      XBATCHname, strerror(errno));
	break;
      } else if (0 == err) {
	(void)fprintf(stderr, "unexpected EOF reading %s: %s - truncated\n",
		      XBATCHname, strerror(errno));
	XBATCHsize = p - XBATCHbuffer;
	break;
      }
    }
    (void)close(fd);
    if (err < 0)
      continue;

    if (GotInterrupt) Interrupted();

    /* Offer the xbatch. */
    (void)sprintf(buff, "xbatch %d", XBATCHsize);
    if (!REMwrite(ToServer, buff)) {
      (void)fprintf(stderr, "Can't offer xbatch to %s, %s\n",
		    REMhost, strerror(errno));
      ExitWithStats(1);
    }
    STAToffered++;
    if (GotInterrupt) Interrupted();

    /* Does he want it? */
    if (!REMread(buff, (int)sizeof buff)) {
      (void)fprintf(stderr, "No reply to XBATCH %d from %s, %s\n",
		    XBATCHsize, REMhost, strerror(errno));
      ExitWithStats(1);
    }
    if (GotInterrupt) Interrupted();

    /* Parse the reply. */
    switch (atoi(buff)) {
    default:
      (void)fprintf(stderr, "Unknown reply to \"%s\" -- %s", XBATCHname, buff);
      ExitWithStats(1);
      /* NOTREACHED */
      break;
    case NNTP_RESENDIT_VAL:
    case NNTP_GOODBYE_VAL:
      /* Most likely out of space -- no point in continuing. */
      syslog(L_NOTICE, XBATCH_FAIL, REMhost, buff);
      ExitWithStats(1);
      /* NOTREACHED */
    case NNTP_CONT_XBATCH_VAL:
      if (!REMsendxbatch(ToServer, XBATCHbuffer, XBATCHsize))
	ExitWithStats(1);
      /* NOTREACHED */
      break;
    case NNTP_SYNTAX_VAL:
    case NNTP_BAD_COMMAND_VAL:
      (void)fprintf(stderr, "Server %s seems not understand XBATCH: %s\n",
		    REMhost, buff);
      syslog(L_FATAL, XBATCH_FAIL, REMhost, buff);
      break;
    }
  }
  ExitWithStats(0);
  /* NOTREACHED */
  return 0;
}
