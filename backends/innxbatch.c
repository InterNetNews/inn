/*  $Id$
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
**  To prevent this, innxbatch should be invoked by a shell script that uses
**  shlock(1) to achieve mutual exclusion.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/time.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/timer.h"
#include "libinn.h"
#include "nntp.h"

/*
** Syslog formats - collected together so they remain consistent
*/
static char	STAT1[] =
	"%s stats offered %lu accepted %lu refused %lu rejected %lu";
static char	STAT2[] = "%s times user %.3f system %.3f elapsed %.3f";
static char	CANT_CONNECT[] = "%s connect failed %s";
static char	CANT_AUTHENTICATE[] = "%s authenticate failed %s";
static char	XBATCH_FAIL[] = "%s xbatch failed %s";
static char	UNKNOWN_REPLY[] = "Unknown reply after sending batch -- %s";
static char	CANNOT_UNLINK[] = "cannot unlink %s: %m";
/*
**  Global variables.
*/
static bool		Debug = 0;
static bool		STATprint;
static char		*REMhost;
static double		STATbegin;
static double		STATend;
static char		*XBATCHname;
static int		FromServer;
static int		ToServer;
static sig_atomic_t	GotAlarm;
static sig_atomic_t	GotInterrupt;
static sig_atomic_t	JMPyes;
static jmp_buf		JMPwhere;
static unsigned long	STATaccepted;
static unsigned long	STAToffered;
static unsigned long	STATrefused;
static unsigned long	STATrejected;

/*
**  Send a line to the server. \r\n will be appended
*/
static bool
REMwrite(int fd, char *p)
{
  int		i;
  int		err;
  char		*dest;
  static char		buff[NNTP_STRLEN];

  for (dest = buff, i = 0; p[i]; ) *dest++ = p[i++];
  *dest++ = '\r';
  *dest++ = '\n';
  *dest++ = '\0';

  for (dest = buff, i+=2; i; dest += err, i -= err) {
    err = write(fd, dest, i);
    if (err < 0) {
      syswarn("cannot write %s to %s", dest, REMhost);
      return false;
    }
  }
  if (Debug)
    fprintf(stderr, "> %s\n", p);

  return true;
}

/*
**  Print transfer statistics, clean up, and exit.
*/
static void
ExitWithStats(int x)
{
  static char		QUIT[] = "quit";
  double		usertime;
  double		systime;

  REMwrite(ToServer, QUIT);

  STATend = TMRnow_double();
  if (GetResourceUsage(&usertime, &systime) < 0) {
    usertime = 0;
    systime = 0;
  }

  if (STATprint) {
    printf(STAT1,
	REMhost, STAToffered, STATaccepted, STATrefused, STATrejected);
    printf("\n");
    printf(STAT2, REMhost, usertime, systime, STATend - STATbegin);
    printf("\n");
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
static char *
REMclean(char *buff)
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
**  Return true if okay, *or we got interrupted.*
*/
static bool
REMread(char *start, int size)
{
  char *p, *h;
  struct timeval t;
  fd_set rmask;
  int i;

  for (p = start; size; ) {
    FD_ZERO(&rmask);
    FD_SET(FromServer, &rmask);
    t.tv_sec = 10 * 60;
    t.tv_usec = 0;
    i = select(FromServer + 1, &rmask, NULL, NULL, &t);
    if (GotInterrupt) return true;
    if (i < 0) {
      if (errno == EINTR) continue;
      else return false;
    }
    if (i == 0 || !FD_ISSET(FromServer, &rmask)) return false;
    i = read(FromServer, p, size-1);
    if (GotInterrupt) return true;
    if (i <= 0) return false;
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
    fprintf(stderr, "< %s\n", start);

  return true;
}


/*
**  Handle the interrupt.
*/
static void
Interrupted(void)
{
  warn("interrupted");
  ExitWithStats(1);
}


/*
**  Send a whole xbatch to the server. Uses the global variables
**  REMbuffer & friends
*/
static bool
REMsendxbatch(int fd, char *buf, int size)
{
  char	*p;
  int		i;
  int		err;

  for (i = size, p = buf; i; p += err, i -= err) {
    err = write(fd, p, i);
    if (err < 0) {
      syswarn("cannot write xbatch to %s", REMhost);
      return false;
    }
  }
  if (GotInterrupt) Interrupted();
  if (Debug)
    fprintf(stderr, "> [%d bytes of xbatch]\n", size);

  /* What did the remote site say? */
  if (!REMread(buf, size)) {
    syswarn("no reply after sending xbatch");
    return false;
  }
  if (GotInterrupt) Interrupted();
  
  /* Parse the reply. */
  switch (atoi(buf)) {
  default:
    warn("unknown reply after sending batch -- %s", buf);
    syslog(L_ERROR, UNKNOWN_REPLY, buf);
    return false;
    /* NOTREACHED */
    break;
  case NNTP_RESENDIT_VAL:
  case NNTP_GOODBYE_VAL:
    syslog(L_NOTICE, XBATCH_FAIL, REMhost, buf);
    STATrejected++;
    return false;
    /* NOTREACHED */
    break;
  case NNTP_OK_XBATCHED_VAL:
    STATaccepted++;
    if (Debug) fprintf(stderr, "will unlink(%s)\n", XBATCHname);
    if (unlink(XBATCHname)) {
      /* probably another incarantion was faster, so avoid further duplicate
       * work
       */
      syswarn("cannot unlink %s", XBATCHname);
      syslog(L_NOTICE, CANNOT_UNLINK, XBATCHname);
      return false;
    }
    break;
  }
  
  /* Article sent */
  return true;
}

/*
**  Mark that we got interrupted.
*/
static RETSIGTYPE
CATCHinterrupt(int s)
{
    GotInterrupt = true;

    /* Let two interrupts kill us. */
    xsignal(s, SIG_DFL);
}


/*
**  Mark that the alarm went off.
*/
/* ARGSUSED0 */
static RETSIGTYPE
CATCHalarm(int s UNUSED)
{
    GotAlarm = true;
    if (JMPyes)
	longjmp(JMPwhere, 1);
}


/*
**  Print a usage message and exit.
*/
static void
Usage(void)
{
    warn("Usage: innxbatch [-Dv] [-t#] [-T#] host file ...");
#ifdef FROMSTDIN
    warn("       innxbatch [-Dv] [-t#] [-T#] -i host");
#endif
    exit(1);
}


int
main(int ac, char *av[])
{
  int			i;
  char                  *p;
  FILE			*From;
  FILE			*To;
  char			buff[NNTP_STRLEN];
  RETSIGTYPE		(*volatile old)(int) = NULL;
  struct stat		statbuf;
  int			fd;
  int			err;
  char *volatile        XBATCHbuffer = NULL;
  char **volatile       argv;
  volatile int		XBATCHbuffersize = 0;
  volatile int		XBATCHsize, argc;
  volatile unsigned int	ConnectTimeout;
  volatile unsigned int	TotalTimeout;

  openlog("innxbatch", L_OPENLOG_FLAGS | LOG_PID, LOG_INN_PROG);
  message_program_name = "innxbatch";

  /* Set defaults. */
  if (!innconf_read(NULL))
      exit(1);
  ConnectTimeout = 0;
  TotalTimeout = 0;
  umask(NEWSUMASK);

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
      FromStdin = true;
      break;
#endif
    case 't':
      ConnectTimeout = atoi(optarg);
      break;
    case 'T':
      TotalTimeout = atoi(optarg);
      break;
    case 'v':
      STATprint = true;
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
  argc = ac;
  argv = av;

  /* Open a connection to the remote server. */
  if (ConnectTimeout) {
    GotAlarm = false;
    old = xsignal(SIGALRM, CATCHalarm);
    JMPyes = true;
    if (setjmp(JMPwhere))
      die("cannot connect to %s: timed out", REMhost);
    alarm(ConnectTimeout);
  }
  if (NNTPconnect(REMhost, NNTP_PORT, &From, &To, buff, sizeof(buff)) < 0
      || GotAlarm) {
    i = errno;
    warn("cannot connect to %s: %s", REMhost,
         buff[0] ? REMclean(buff): strerror(errno));
    if (GotAlarm)
      syslog(L_NOTICE, CANT_CONNECT, REMhost, "timeout");
    else
      syslog(L_NOTICE, CANT_CONNECT, REMhost,
	     buff[0] ? REMclean(buff) : strerror(i));
    exit(1);
  }

  if (Debug)
    fprintf(stderr, "< %s\n", REMclean(buff));
  if (NNTPsendpassword(REMhost, From, To) < 0 || GotAlarm) {
    i = errno;
    syswarn("cannot authenticate with %s", REMhost);
    syslog(L_ERROR, CANT_AUTHENTICATE,
	   REMhost, GotAlarm ? "timeout" : strerror(i));
    /* Don't send quit; we want the remote to print a message. */
    exit(1);
  }
  if (ConnectTimeout) {
    alarm(0);
    xsignal(SIGALRM, old);
    JMPyes = false;
  }
  
  /* We no longer need standard I/O. */
  FromServer = fileno(From);
  ToServer = fileno(To);

#if	defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF)
  i = 24 * 1024;
  if (setsockopt(ToServer, SOL_SOCKET, SO_SNDBUF, (char *)&i, sizeof i) < 0)
    perror("cant setsockopt(SNDBUF)");
  if (setsockopt(FromServer, SOL_SOCKET, SO_RCVBUF, (char *)&i, sizeof i) < 0)
    perror("cant setsockopt(RCVBUF)");
#endif	/* defined(SOL_SOCKET) && defined(SO_SNDBUF) && defined(SO_RCVBUF) */

  GotInterrupt = false;
  GotAlarm = false;

  /* Set up signal handlers. */
  xsignal(SIGHUP, CATCHinterrupt);
  xsignal(SIGINT, CATCHinterrupt);
  xsignal(SIGTERM, CATCHinterrupt);
  xsignal(SIGPIPE, SIG_IGN);
  if (TotalTimeout) {
    xsignal(SIGALRM, CATCHalarm);
    alarm(TotalTimeout);
  }

  /* Start timing. */
  STATbegin = TMRnow_double();

  /* main loop over all specified files */
  for (XBATCHname = *argv; argc && (XBATCHname = *argv); argv++, argc--) {

    if (Debug) fprintf(stderr, "will work on %s\n", XBATCHname);

    if (GotAlarm) {
      warn("timed out");
      ExitWithStats(1);
    }
    if (GotInterrupt) Interrupted();

    if ((fd = open(XBATCHname, O_RDONLY, 0)) < 0) {
      syswarn("cannot open %s, skipping", XBATCHname);
      continue;
    }

    if (fstat(fd, &statbuf)) {
      syswarn("cannot stat %s, skipping", XBATCHname);
      close(i);
      continue;
    }

    XBATCHsize = statbuf.st_size;
    if (XBATCHsize == 0) {
      warn("batch file %s is zero length, skipping", XBATCHname);
      close(i);
      unlink(XBATCHname);
      continue;
    } else if (XBATCHsize > XBATCHbuffersize) {
      XBATCHbuffersize = XBATCHsize;
      if (XBATCHbuffer) free(XBATCHbuffer);
      XBATCHbuffer = xmalloc(XBATCHsize);
    }

    err = 0; /* stupid compiler */
    for (i = XBATCHsize, p = XBATCHbuffer; i; i -= err, p+= err) {
      err = read(fd, p, i);
      if (err < 0) {
        syswarn("error reading %s, skipping", XBATCHname);
	break;
      } else if (0 == err) {
        syswarn("unexpected EOF reading %s, truncated", XBATCHname);
	XBATCHsize = p - XBATCHbuffer;
	break;
      }
    }
    close(fd);
    if (err < 0)
      continue;

    if (GotInterrupt) Interrupted();

    /* Offer the xbatch. */
    snprintf(buff, sizeof(buff), "xbatch %d", XBATCHsize);
    if (!REMwrite(ToServer, buff)) {
      syswarn("cannot offer xbatch to %s", REMhost);
      ExitWithStats(1);
    }
    STAToffered++;
    if (GotInterrupt) Interrupted();

    /* Does he want it? */
    if (!REMread(buff, (int)sizeof buff)) {
      syswarn("no reply to XBATCH %d from %s", XBATCHsize, REMhost);
      ExitWithStats(1);
    }
    if (GotInterrupt) Interrupted();

    /* Parse the reply. */
    switch (atoi(buff)) {
    default:
      warn("unknown reply to %s -- %s", XBATCHname, buff);
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
      warn("server %s seems not to understand XBATCH: %s", REMhost, buff);
      syslog(L_FATAL, XBATCH_FAIL, REMhost, buff);
      break;
    }
  }
  ExitWithStats(0);
  /* NOTREACHED */
  return 0;
}
