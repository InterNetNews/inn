/* -*- c -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Mon, 15 Jan 1996 17:31:58 +1100
 * Project:     INN -- innfeed
 * File:        main.c
 * RCSId:       $Id$
 *
 * Copyright:   Copyright (c) 1996 by Internet Software Consortium
 *
 *              Permission to use, copy, modify, and distribute this
 *              software for any purpose with or without fee is hereby
 *              granted, provided that the above copyright notice and this
 *              permission notice appear in all copies.
 *
 *              THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE
 *              CONSORTIUM DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS
 *              SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *              MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET
 *              SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 *              INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *              WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 *              WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 *              TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 *              USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Description: Main routines for the innfeed program.
 * 
 */

#if ! defined (lint)
static const char *rcsid = "$Id$" ;
static void use_rcsid (const char *rid) {   /* Never called */
  use_rcsid (rcsid) ; use_rcsid (rid) ;
}
#endif

#include "config.h"             /* system specific configuration */

#include <stdlib.h>
#include <syslog.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <math.h>

#if defined (DO_HAVE_UNISTD)
#include <unistd.h>
#endif

#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>


#include "misc.h"
#include "tape.h"
#include "article.h"
#include "msgs.h"
#include "buffer.h"
#include "connection.h"
#include "configfile.h"

#if defined(DO_HAVE_UNIX_DOMAIN)
#include <sys/un.h>
#endif  /* defined(DO_HAVE_UNIX_DOMAIN) */

#include "endpoint.h"
#include "host.h"
#include "innlistener.h"

#define INHERIT 1
#define NO_INHERIT 0

/* exports */
bool talkToSelf ;
extern int debugWrites ;
bool sigFlag = false ;
const char *InputFile ;
const char *configFile = NULL ;
bool RollInputFile = false ;
const char *pidFile = NULL ;
bool useMMap = false ;
void (*gPrintInfo) (void) ;

/* imports */
extern char *versionInfo ;
#if defined (sun)
extern char *optarg ;           /* needed for Solaris */
extern int optind;
#endif

extern void openInputFile (void);

/* privates */
static char *logFile ;
static char *newsspool ;

static void sigiot (int sig) ;
static void sigalrm (int sig) ;
static void sigchld (int sig) ;
static void sigint (int sig) ;
static void sigquit (int sig) ;
static void sighup (int sig) ;
static void sigterm (int sig) ;
static void sigusr (int sig) ;
static void usage (int) ;
static void gprintinfo (void) ;
static void openLogFile (void) ;
static void writePidFile (void) ;
static int mainOptionsProcess (void *data) ;
static int mainConfigLoadCbk (void *data) ;
static void mainCleanup (void) ;

static char *bopt = NULL ;
static char *aopt = NULL ;
static char *popt = NULL ;
static bool Mopt = false ;
static bool Zopt = false ;
static bool Dopt = false ;
static int debugLevel = 0 ;
static u_int initialSleep = 2 ;
static char *sopt = NULL ;
static char *lopt = NULL ;
static bool eopt = false ;
static int elimit = 0 ;

int main (int argc, char **argv)
{
  EndPoint ep ;
  InnListener listener ;
  int optVal, fd, rval ;
  const char *subProgram = NULL ;
  bool seenV = false ;
  bool dynamicPeers = false ;
  time_t now = theTime() ;
  char dateString [30] ;
  char *copt = NULL ;
  bool checkConfig = false ;
  struct rlimit rl;


  strcpy (dateString,ctime(&now)) ;
  dateString [24] = '\0' ;

  if ((program = strrchr (argv [0],'/')) == NULL)
    program = argv [0] ;
  else
    program++ ;

  gPrintInfo = gprintinfo ;

#if defined (HAVE_MMAP)
  useMMap = true ;
#else
  useMMap = false ;
#endif

  /* always turn on copying into memory unless mmapping and the article
     data being in NNTP format. */
  artBitFiddleContents (true) ;

#define OPT_STRING "a:b:c:Cd:e:hl:mMo:p:S:s:vxyz"

  while ((optVal = getopt (argc,argv,OPT_STRING)) != EOF)
    {
      switch (optVal) 
        {
          case 'a':
            aopt = optarg ;
            break ;

          case 'b':
            if ( !isDirectory (optarg) )
              logAndExit (1,"Not a directory: %s\n",optarg) ;
            bopt = optarg ;
            break ;

          case 'C':
            checkConfig = true ;
            break ;

          case 'c':
            copt = optarg ;
            break ;

          case 'd':
            loggingLevel = atoi (optarg) ;
            debugLevel = loggingLevel ;
            Dopt = true ;
            break ;

          case 'e':
            eopt = true ;
            elimit = atoi (optarg) ;
            if (elimit <= 0)
              {
                fprintf (stderr,"Illegal value for -e option\n") ;
                usage (1) ;
              }
            break ;

          case 'h':
            usage (0) ;

          case 'l':
            lopt = optarg ;
            break ;

          case 'M':
            Mopt = true ;
            useMMap = false ;
            break ;

          case 'm':
            artLogMissingArticles (true) ;
            break ;

          case 'o':
            artSetMaxBytesInUse (atoi (optarg)) ;
            break ;

          case 'p':
            popt = optarg ;
            break ;

          case 's':
            subProgram = optarg ;
            break ;

          case 'S':
            sopt = optarg ;
            break ;

          case 'v':
            seenV = true ;
            break ;

          case 'x':
            talkToSelf = true ;
            break ;

          case 'y':
            dynamicPeers = true ;
            break ;

          case 'z':
            Zopt = true ;
            break ;

          default:
            usage (1) ;	
        }
    }

  argc -= optind;
  argv += optind;

  if (argc > 1)
    usage (1) ;
  else if (argc == 1)
    InputFile = *argv;

  if (seenV)
    {
      warn ("%s version: %s\n",program, versionInfo) ;
      exit (0) ;
    }

  /* make sure we have valid fds 0, 1 & 2 so it is not taken by
    something else, probably openlog().  fd 0 will be freopen()ed on the
    inputFile, the subProgram, or ourself.  fd 1 and fd 2 will
    be freopen()ed on the log file (or will stay pointed at /dev/null).
    
    without doing this, if the descriptors were closed then the
    freopen calls on some systems (like BSDI 2.1) will really close
    whatever has aquired the stdio descriptors, such as the socket
    to syslogd.
    
    XXX possible problems:  what if fd 0 is closed but no inputFile,
    XXX subProgram or talkToSelf is true?  it will not be freopen()ed, so
    XXX innfeed won't have any fresh data (besides, fd 0 is only writable
    XXX here).  perhaps a warning should be issued.
    */
  do
    {
      fd = open("/dev/null", O_WRONLY);
      switch (fd)
        {
          case -1:
            logAndExit (1,"open(\"/dev/null\", O_WRONLY): %s",
                        strerror (errno));
            break;
          case 0:
          case 1:
          case 2:
            /* good, we saved an fd from being trounced */
            break;
          default:
            close(fd);
        }
    } while (fd < 2);

  if ( !checkConfig ) 
    {
      openlog (program,(int)(L_OPENLOG_FLAGS|LOG_PID),LOG_NEWS) ;
      syslog (LOG_NOTICE,STARTING_PROGRAM,versionInfo,dateString) ;
    }

  if (subProgram == NULL && talkToSelf == false)
    {
      struct stat buf ;

      if (fstat (0,&buf) < 0)
        logAndExit (1,FSTAT_FAILURE,"stdin") ;
      else if (S_ISREG (buf.st_mode))
        InputFile = "";
    }

  if (subProgram != NULL && (talkToSelf == true || InputFile))
    {
      dprintf (0,"Cannot specify '-s' with '-x' or an input file\n") ;
      syslog (LOG_ERR,"Incorrect arguments: '-s' with '-x' or an input file\n");
      usage (1) ;
    }

  /*
   * set up the config file name and then read the file in. Order is important.
   */
  configAddLoadCallback (mainOptionsProcess,(checkConfig ? stderr : NULL)) ;
  configAddLoadCallback (tapeConfigLoadCbk,(checkConfig ? stderr : NULL)) ;

  configAddLoadCallback (endpointConfigLoadCbk,(checkConfig ? stderr : NULL));
  configAddLoadCallback (hostConfigLoadCbk,(checkConfig ? stderr : NULL)) ;
  configAddLoadCallback (cxnConfigLoadCbk,(checkConfig ? stderr : NULL)) ;
  configAddLoadCallback (mainConfigLoadCbk,(checkConfig ? stderr : NULL)) ;
  configAddLoadCallback (listenerConfigLoadCbk,(checkConfig ? stderr : NULL));

  if (copt != NULL && *copt == '\0')
    {
      logOrPrint (LOG_CRIT,(checkConfig ? stderr : NULL),
                  "Empty pathname for ``-c'' option") ;
      exit (1) ;
    }
  else if (copt != NULL && bopt != NULL)
    configFile = buildFilename (bopt,copt) ;
  else if (bopt != NULL)
    configFile = buildFilename (bopt,CONFIG_FILE) ;
  else if (copt != NULL)
    configFile = buildFilename (TAPE_DIRECTORY,copt) ;
  else
    configFile = buildFilename (TAPE_DIRECTORY,CONFIG_FILE) ;

  rval = readConfig (configFile,(checkConfig ? stderr : NULL),
                     checkConfig,loggingLevel > 0);
  if (checkConfig)
    {
      if (!rval)
        fprintf (stderr,"config loading failed.\n") ;
      else
        fprintf (stderr,"config loading succeeded.\n") ;
      exit (1) ;
    }
  else if (!rval)
    exit (1) ;

  if (loggingLevel == 0 && fileExistsP (DEBUG_FILE))
    loggingLevel = 1 ;

  if (logFile == NULL && ! isatty (fileno (stderr)))
    logFile = strdup (LOG_FILE) ;

  if (logFile)
    openLogFile () ;

  openfds = 4 ;                 /* stdin, stdout, stderr and syslog */

  writePidFile ();

  if (subProgram != NULL)
    {
      int fds [2] ;
      int pid ;

      if (pipe (fds) < 0)
        {
          syslog (LOG_CRIT,PIPE_FAILURE) ;
          exit (1) ;
        }

      if ((pid = fork ()) < 0)
        {
          syslog (LOG_CRIT,FORK_FAILURE) ;
          exit (1) ;
        }
      else if (pid == 0)
        {                       /* child */
          close (fds[0]) ;
          close (0) ;
          close (1) ;
          close (2) ;
          dup2 (fds[1],1) ;
          dup2 (fds[1],2) ;
          execlp ("sh","sh", "-c", subProgram, NULL) ;
          perror ("execlp") ;
          exit (1) ;
        }
      else
        {                       /* parent */
          close (0) ;
          dup2 (fds[0],0) ;
          close (fds[1]) ;
          signal(SIGCHLD,sigchld) ;
          openfds++ ;
        }
    }
  else  if (talkToSelf)
    {
        /* We're not really getting information from innd or a subprogram,
           but are just processing backlog files. We set up a pipe to ourself
           that we never write to, to simulate an idle innd. */
      int pipefds [2] ;

      if (pipe (pipefds) != 0)
        {
          syslog (LOG_ERR,PIPE_FAILURE) ;
          exit (1) ;
        }

      close (0) ;
      dup2 (pipefds [0], 0) ;

      openfds++ ;
      openfds++ ;
    }

  if (chdir (newsspool) != 0)
    {
      syslog (LOG_ERR,CD_FAILED,newsspool) ;
      exit (1) ;
    }

    /* hook up the endpoint to the source of new article information (usually
       innd). */
  ep = newEndPoint (0) ;        /* fd 0, i.e. stdin */

    /* now arrange for this endpoint to always be the first one checked for
       possible activity. */
  setMainEndPoint (ep) ;

  listener = newListener (ep, talkToSelf,dynamicPeers) ;
  mainListener = listener ;

  sleep (initialSleep) ;


  /* now lower maximum open file limit to match what select(2) can handle. */
  if (getrlimit(RLIMIT_NOFILE,&rl) != 0)
    syslog (LOG_ERR,GETRLIM_FAILED) ;
  else
    {
#if defined (FD_SETSIZE)
      u_int fd_max = FD_SETSIZE ;
#else
      u_int fd_max = sizeof (fd_set) * CHAR_BIT ;
#endif
      
      if (rl.rlim_max > fd_max)
        {
          rl.rlim_max = rl.rlim_cur = fd_max ;
          if (setrlimit (RLIMIT_NOFILE,&rl) != 0)
            syslog (LOG_ERR,SETRLIM_FAILED,(long)fd_max);
        }
    }
  
      
  
  configHosts (talkToSelf) ;

  if (InputFile && *InputFile) {
    openInputFile () ;
  }

  /* handle signal to shutdown */
  setSigHandler (SIGTERM,sigterm) ;
  setSigHandler (SIGQUIT,sigquit) ;

  /* handle signal to reload config */
  setSigHandler (SIGHUP,sighup) ;

  /* handle signal to print snapshot. */
  setSigHandler (SIGINT,sigint) ;

  /* handle signal to roll input file */
  setSigHandler (SIGALRM,sigalrm) ;

  /* handle signal to flush all the backlog files */
  setSigHandler (SIGIOT,sigiot) ;

  /* we can increment and decrement logging levels by sending SIGUSR{1,2} */
  setSigHandler (SIGUSR1,sigusr) ;
  setSigHandler (SIGUSR2,sigusr) ;

  atexit (mainCleanup) ;
  
  Run () ;

  exit (0) ;
}

static void usage (int val)
{
  fprintf (stderr,"usage: %s [ options ] [ file ]\n\n",
           program) ;
  fprintf (stderr,"Version: %s\n\n",versionInfo) ;
  fprintf (stderr,"Config file: %s\n",CONFIG_FILE) ;
  fprintf (stderr,"Backlog directory: %s\n",TAPE_DIRECTORY) ;
  fprintf (stderr,"\nLegal options are:\n") ;
  fprintf (stderr,"\t-a dir      Use the given directory as the top of the article spool\n") ;

  fprintf (stderr,"\t-b dir      Use the given directory as the the storage\n");
  fprintf (stderr,"\t            place for backlog files and lock files.\n");

  fprintf (stderr,"\t-c file     Use the given file as the config file instead of the\n");
  fprintf (stderr,"\t            default of %s\n",CONFIG_FILE);

  fprintf (stderr,"\t-C          Check the config file and then exit.\n") ;
  fprintf (stderr,"\t-d num      set the logging level to num (an integer).\n");
  fprintf (stderr,"\t            Larger value means more logging. 0 means no\n");
  fprintf (stderr,"\t            logging. The default is 0\n");

  fprintf (stderr,"\t-e bytes    Keep the output backlog files to no bigger\n");
  fprintf (stderr,"\t            than %.2f times this number\n",LIMIT_FUDGE);

  fprintf (stderr,"\t-h          print this message\n");

  fprintf (stderr,"\t-l file     redirect stderr and stdout to the given file.\n");
  fprintf (stderr,"\t            When run under INN they normally are redirected to\n");
  fprintf (stderr,"\t            /dev/null. This is needed if using '-d'.\n");

  fprintf (stderr,"\t-m          Log information on all missing articles\n");

  fprintf (stderr,"\t-M          Turn *off* use of mmap\n") ;
#if ! defined (HAVE_MMAP)
  fprintf (stderr,"\t            (a no-op as this excutable has been built without mmap support\n") ;
#endif

  fprintf (stderr,"\t-p file     Write the process id to the given file\n") ;
  fprintf (stderr,"\t            instead of the default of %s\n",PID_FILE);
  fprintf (stderr,"\t            A relative path is relative to the backlog directory\n") ;

  fprintf (stderr,"\t-s command  run the given command in a subprocess and use\n");
  fprintf (stderr,"\t            its output as article information instead of\n");
  fprintf (stderr,"\t            running under innd\n");

  fprintf (stderr,"\t-S file     Use the give filename instead of innfeed.status\n") ;
  fprintf (stderr,"\t            relative pathnames start from the backlog directory\n") ;

  fprintf (stderr,"\t-v          print version information\n");

  fprintf (stderr,"\t-x          Do not read any article information off stdin,\n");
  fprintf (stderr,"\t            but simply process backlog files and then exit\n");
  fprintf (stderr,"\t            when done\n");

  fprintf (stderr,"\t-y          Add peers dynamically. If an unrecognized peername\n");
  fprintf (stderr,"\t            is received from innd, then it is presumed to also\n");
  fprintf (stderr,"\t            be the ip name and a new peer binding is set up\n");

  fprintf (stderr,"\t-z          have each of the connections issue their own stats\n");
  fprintf (stderr,"\t            whenever they close, or whenever their controller\n");
  fprintf (stderr,"\t            issues its own stats\n");

  exit (val) ;
}

static void sigterm (int sig)
{
  (void) sig ;
  
  syslog(LOG_NOTICE, SHUTDOWN_SIGNAL);
  shutDown (mainListener) ;
}

static void sigquit (int sig)
{
  (void) sig ;
  
  sigterm (0) ;
}

static void sigint (int sig)
{
  (void) sig ;

  gprintinfo () ;
}

static void sighup (int sig)
{
  (void) sig ;
  
  syslog(LOG_NOTICE, CONFIG_RELOAD, configFile);

  if (!readConfig (configFile,NULL,false,loggingLevel > 0))
    {
      syslog (LOG_ERR,PARSE_FAILURE) ;
      exit (1) ;
    }

  configHosts (talkToSelf) ;
}

static void sigiot (int sig)
{
  (void) sig ;

  gFlushTapes () ;
}

static void sigalrm (int sig)
{
  (void) sig ;
  
  if (InputFile == NULL)
    syslog (LOG_ERR,IGNORE_SIGALRM) ;
  else 
    {
      RollInputFile = true;
      syslog(LOG_NOTICE, "ME preparing to roll %s", InputFile);
    }
}

static void sigchld (int sig)
{

  (void) sig ;                  /* keep lint happy */

#if 0
  wait (&status) ;              /* we don't care */
#endif

  signal (sig,sigchld) ;
}

  /* SIGUSR1 increments logging level. SIGUSR2 decrements. */
static void sigusr (int sig)
{
  if (sig == SIGUSR1) {
    syslog(LOG_NOTICE, INCR_LOGLEVEL, loggingLevel);
    loggingLevel++ ;
  } else if (sig == SIGUSR2 && loggingLevel > 0) {
    syslog(LOG_NOTICE, DECR_LOGLEVEL, loggingLevel);
    loggingLevel-- ;
  }    
}

static void openLogFile (void)
{
  FILE *fpr ;

  if (logFile)
    {
      fpr = freopen (logFile,"a",stdout) ;
      if (fpr != stdout)
        logAndExit (1,"freopen (%s, \"a\", stdout): %s",
                    logFile, strerror (errno)) ;
      
      fpr = freopen (logFile,"a",stderr) ;
      if (fpr != stderr)
        logAndExit (1,"freopen (%s, \"a\", stderr): %s",
                    logFile, strerror (errno)) ;
      
#if defined (DO_HAVE_SETBUFFER)
      setbuffer (stdout, NULL, 0) ;
      setbuffer (stderr, NULL, 0) ;
#else
      setbuf (stdout, NULL) ;
      setbuf (stderr, NULL) ;
#endif
    }
}

static void writePidFile (void)
{
  FILE *F;
  int pid;

  if (pidFile == NULL)
    logAndExit (1,"NULL pidFile\n") ;

  /* Record our PID. */
  pid = getpid();
  if ((F = fopen(pidFile, "w")) == NULL)
    {
      syslog(LOG_ERR, "ME cant fopen %s %m", pidFile);
    }
  else
    {
      if (fprintf(F, "%ld\n", (long)pid) == EOF || ferror(F))
	{
	  syslog(LOG_ERR, "ME cant fprintf %s %m", pidFile);
        }
      if (fclose(F) == EOF)
	{
	  syslog(LOG_ERR, "ME cant fclose %s %m", pidFile);
        }
      if (chmod(pidFile, 0664) < 0)
	{
	  syslog(LOG_ERR, "ME cant chmod %s %m", pidFile);
        }
    }
}

static void gprintinfo (void)
{
  FILE *fp = fopen (SNAPSHOT_FILE,"a") ;
  time_t now = theTime() ;

  if (fp == NULL)
    {
      syslog (LOG_ERR,FOPEN_FAILURE,SNAPSHOT_FILE) ;
      return ;
    }

#if defined (DO_HAVE_SETBUFFER)
  setbuffer (fp, NULL, 0) ;
#else
  setbuf (fp, NULL) ;
#endif

  fprintf (fp,"----------------------------System snaphot taken at: %s\n",
           ctime (&now)) ;
  gPrintListenerInfo (fp,0) ;
  fprintf (fp,"\n\n\n\n") ;
  gPrintHostInfo (fp,0) ;
  fprintf (fp,"\n\n\n\n") ;
  gPrintCxnInfo (fp,0) ;
  fprintf (fp,"\n\n\n\n") ;
  gPrintArticleInfo (fp,0) ;
  fprintf (fp,"\n\n\n\n") ;
  gPrintBufferInfo (fp,0) ;
  fprintf (fp,"\n\n\n\n") ;
  fclose (fp) ;
}

/* called after the config file is loaded and after the config data has
  been updated with command line options. */
static int mainConfigLoadCbk (void *data)
{
  FILE *fp = (FILE *) data ;
  char *p ;
  long ival ;
  int bval ;

  if (getString (topScope,"news-spool", &p,NO_INHERIT))
    {
      if ( !isDirectory (p) && isDirectory (NEWSSPOOL) )
        {
          logOrPrint (LOG_WARNING,fp,BADSPOOL_CHANGE,p,NEWSSPOOL) ;
          p = strdup (NEWSSPOOL) ;
        }
      else if (!isDirectory (p))
        logAndExit (1,"Bad spool directories: %s, %s\n",p,NEWSSPOOL) ;
    }
  else if (!isDirectory (NEWSSPOOL))
    logAndExit (1,SPOOL_NODEF,NEWSSPOOL);
  else
    p = strdup (NEWSSPOOL) ;
  newsspool = p ;

  /***************************************************/
  
  if (getString (topScope,"pid-file",&p,NO_INHERIT))
    {
      pidFile = buildFilename (getTapeDirectory(),p) ;
      free (p) ;
    }
  else
    pidFile = buildFilename (getTapeDirectory(),PID_FILE) ;
  
  if (getInteger (topScope,"debug-level",&ival,NO_INHERIT))
    loggingLevel = (u_int) ival ;
  
  
  if (getInteger (topScope,"initial-sleep",&ival,NO_INHERIT))
    initialSleep = (u_int) ival ;
  
  
  if (getBool (topScope,"use-mmap",&bval,NO_INHERIT))
    useMMap = (bval ? true : false) ;

  
  if (getString (topScope,"log-file",&p,NO_INHERIT))
    {
      logFile = buildFilename (getTapeDirectory(),p) ;
      FREE (p) ;
    }
  

  return 1 ;
}

/*
 * called after config file is loaded but before other callbacks, so we
 * can adjust config file values from options. They will be validated in the
 * second callback.
 */
static int mainOptionsProcess (void *data)
{
  value *v ;

  (void) data ;
  
  if (bopt != NULL)
    {
      if ((v = findValue (topScope,"backlog-directory",NO_INHERIT)) != NULL) 
        {
          FREE (v->v.charp_val) ;
          v->v.charp_val = strdup (bopt) ;
        }
      else
        addString (topScope,"backlog-directory",strdup (bopt)) ;
    }

  if (aopt != NULL)
    {
      if ((v = findValue (topScope,"news-spool",NO_INHERIT)) != NULL)
        {
          FREE (v->v.charp_val) ;
          v->v.charp_val = strdup (aopt) ;
        }
      else
        addString (topScope,"news-spool",strdup (aopt)) ;
    }

  if (sopt != NULL)
    {
      if ((v = findValue (topScope,"status-file",NO_INHERIT)) != NULL)
        {
          FREE (v->v.charp_val) ;
          v->v.charp_val = strdup (sopt) ;
        }
      else
        addString (topScope,"status-file",strdup (sopt)) ;
    }


  if (Dopt)
    {
      if ((v = findValue (topScope,"debug-level",NO_INHERIT)) != NULL)
        v->v.int_val = debugLevel ;
      else
        addInteger (topScope,"debug-level",debugLevel) ;
    }

  
  if (eopt || talkToSelf)
    {
      if (talkToSelf)
        elimit = 0 ;
      
      if ((v = findValue (topScope,"backlog-limit",NO_INHERIT)) != NULL)
        v->v.int_val = elimit ;
      else
        addInteger (topScope,"backlog-limit",debugLevel) ;
    }

  
  if (Mopt)
    {
      if ((v = findValue (topScope,"use-mmap",NO_INHERIT)) != NULL)
        v->v.bool_val = 0 ;
      else
        addBoolean (topScope,"use-mmap",0) ;
    }
  

  if (popt != NULL)
    {
      if ((v = findValue (topScope,"pid-file",NO_INHERIT)) != NULL)
        {
          FREE (v->v.charp_val) ;
          v->v.charp_val = strdup (popt) ;
        }
      else
        addString (topScope,"pid-file",strdup (popt)) ;
    }

  if (Zopt)
    {
      if ((v = findValue (topScope,"connection-stats",NO_INHERIT)) != NULL)
        v->v.bool_val = 1 ;
      else
        addBoolean (topScope,"connection-stats",1) ;
    }

  if (lopt != NULL)
    {
      if ((v = findValue (topScope,"log-file",NO_INHERIT)) != NULL)
        {
          FREE (v->v.charp_val) ;
          v->v.charp_val = strdup (lopt) ;
        }
      else
        addString (topScope,"log-file",strdup (lopt)) ;
    }

  return 1 ;
}



static void mainCleanup (void)
{
  FREE (configFile) ;
  FREE (pidFile) ;
  FREE (logFile) ;
  FREE (newsspool) ;
}


void mainLogStatus (FILE *fp)
{
  fprintf (fp,"Global configuration parameters:\n") ;
  fprintf (fp,"          Mode: ") ;
  if (InputFile != NULL)
    fprintf (fp,"Funnel file") ;
  else if (talkToSelf)
    fprintf (fp,"Batch") ;
  else
    fprintf (fp,"Channel") ;
  if (InputFile != NULL)
    fprintf (fp,"   (%s)",(*InputFile == '\0' ? "stdin" : InputFile)) ;
  fprintf (fp,"\n") ;
  fprintf (fp,"    News spool: %s\n",newsspool) ;
  fprintf (fp,"      Pid file: %s\n",pidFile) ;
  fprintf (fp,"      Log file: %s\n",(logFile == NULL ? "(none)" : logFile));
  fprintf (fp,"   Debug level: %2ld                Mmap: %s\n",
           (long)loggingLevel,boolToString(useMMap)) ;
  fprintf (fp,"\n") ;
}
