/*  $Id$
**
**  innfeed's configuration values.
**
**  Written by James Brister <brister@vix.com>
**
**  The application configuration values.  This file is #include'd before any
**  system header files, so it can't rely on any CPP symbols other that what
**  the compiler defines.
*/

#if ! defined ( innfeed_h__ )
#define innfeed_h__

#include "inn/timer.h"

/**********************************************************************/
/*                     Application specific defines                   */
/**********************************************************************/

/* the path to the run-time config file. If relative, then relative to
   @ETCDIR@. Overridden by ``-c'' option. */
#define CONFIG_FILE "innfeed.conf"


/*
 * This next section contains things than can be overriden in the config
 * file. The strings inside each comment is the key used in the
 * innfeed.conf file to override the value here. See innfeed.conf for a
 * description of each./
 */

/* in tape.c */
#define TAPE_DIRECTORY 	        "innfeed"   /* [pathspool]/backlog-directory */
#define TAPE_HIGHWATER 		5 		/* backlog-highwater */
#define TAPE_ROTATE_PERIOD 	60 		/* backlog-rotate-period */
#define TAPE_CHECKPOINT_PERIOD 	30 		/* backlog-ckpt-period */
#define TAPE_NEWFILE_PERIOD 	600 		/* backlog-newfile-period */
#define TAPE_DISABLE		false		/* no-backlog */

/* in main.c */
#define PID_FILE 		"innfeed.pid" 	/* [pathrun]/pid-file */
#define LOG_FILE 		"innfeed.log"	/* [pathlog]/log-file */

/* in host.c */
#define DNS_RETRY_PERIOD 	900 		/* dns-retry */
#define DNS_EXPIRE_PERIOD 	86400 		/* dns-expire */
#define CLOSE_PERIOD 		(60 * 60 * 24) 	/* close-period */
#define GEN_HTML		false 		/* gen-html */
#define INNFEED_STATUS 		"innfeed.status" /* status-file */
#define LOG_CONNECTION_STATS 	0 		/* connection-stats */
#define HOST_HIGHWATER 		10 		/* host-highwater */
#define STATS_PERIOD 		(60 * 10) 	/* stats-period */
#define STATS_RESET_PERIOD 	(60 * 60 * 12) 	/* stats-reset-period */

#define ARTTOUT		 	600 		/* article-timeout */
#define RESPTOUT	 	300 		/* response-timeout */
#define INIT_CXNS		1 		/* initial-connections */
#define MAX_CXNS		2 		/* max-connections */
#define MAX_Q_SIZE		5 		/* max-queue-size */
#define STREAM			true 		/* streaming */
#define NOCHECKHIGH 		95.0 		/* no-check-high */
#define NOCHECKLOW 		90.0 		/* no-check-low */
#define PORTNUM 		119 		/* port-number */
#define BLOGLIMIT		0 		/* backlog-limit */
#define LIMIT_FUDGE 		1.10 		/* backlog-factor */
#define BLOGLIMIT_HIGH		0 		/* backlog-limit-high */

#define INIT_RECON_PER 30 	/* initial-reconnect-time */
#define MAX_RECON_PER (60 * 60 * 1)/* max-reconnect-time */







/****************************************************************************/
/* 
 * The rest below are not run-time configurable.
 */

/* If this file exists at startup then it's the same as having done
   '-d 1' on the command line. This is a cheap way of avoiding continual
   reloading of the newsfeeds file when debugging. */
#define DEBUG_FILE "innfeed.debug" /* Relative to pathlog */

/* if defined to a non-zero number, then a snapshot will be printed
   whenever die() is called (e.g. on assert failure). This can use up a
   lot of disk space. */
#define SNAPSHOT_ON_DIE 0

/* the full pathname of the file to get a printed dump of the system when
   a SIGINT is delivered (or SNAPSHOT_ON_DIE is non-zero--see below). */
#define SNAPSHOT_FILE "innfeed.snapshot" /* Relative to pathlog */

/* Define this be an existing directory (or NULL). If innfeed deliberatly
   dumps core it will chdir() to this directory first (if non-NULL). If
   NULL then it will chdir to TAPE_DIRECTORY (as possibly modified by
   the '-b' option). */
#define CORE_DIRECTORY NULL

/* strings that get added to the end of a peer name for generating
   backlog file names.  A peername cannot end in any of these string
   (e.g. having a peer called 'mypeer.input' will not work) */
#define OUTPUT_TAIL ".output"
#define INPUT_TAIL ".input"
#define LOCK_TAIL ".lock"

/* rough estimate of average article line length (including
   headers). Smaller number means more efficient article preparation (for
   transfer), but, if much smaller than reality, then more memory
   wastage. */
#define CHARS_PER_LINE 60

/* How many seconds between logging statistics on article allocation.
   For no logging set to 0 */
#define ARTICLE_STATS_PERIOD (10 * 60) /* 10 minutes */

/* max number of parallel connections to a single remote. This is just a
   sanity check for the runtime config file. */
#define MAX_CONNECTION_COUNT 50

/* default size in bytes for buffers */
#define BUFFER_SIZE 256

/* amount we expand buffers on partial reads */
#define BUFFER_EXPAND_AMOUNT 128 

/* minimum number of seconds between log messages for starting
   spooling. i.e. if the connection bounces up and down this will prevent
   frequent logging of the spooling message. 0 turns off this logging. */
#define SPOOL_LOG_PERIOD 600

/* some big numbers just for sanity checking */
#define MAX_MAXCHECKS 10000     /* no more than 10000 articles at a time */
#define MAX_MAXART_TOUT 86400   /* one day max between articles from inn */
#define MAX_RESP_TOUT 3600      /* one hour max to wait for response */

/* the check / no-check filter value, i.e. roughly how many past
   articles we take into account whilst doing the average for
   check / no-check mode.
   Ensure it's a float. */
#define FILTERVALUE 50.0

/* the maximum number of peers we'll handle (not connections) */
#define MAX_HOSTS 100

/* We try to keep article memory allocation below this limit. Doesn't work
   very well, though. */
#define SOFT_ARTICLE_BYTE_LIMIT (1024 * 1024 * 10) /* 10MB */

/* define SELECT_RATIO to the number of times through the main loop before
   checking on the fd from inn again.... */
#define SELECT_RATIO 3


#if defined (DBTIMES)

  /* some small values for testing things. */

#undef STATS_PERIOD
#define STATS_PERIOD 30   /* 30 seconds */

#undef STATS_RESET_PERIOD
#define STATS_RESET_PERIOD (6 * 60) /* 6 minutes */

#undef ARTICLE_STATS_PERIOD
#define ARTICLE_STATS_PERIOD (6 * 60) /* 7 minutes */

#undef CLOSE_PERIOD
#define CLOSE_PERIOD (3 * 60)   /* 5 minutes */

#endif /* DBTIMES */


/* Additional OS-specific defines.  These should really be moved into
   configure at some point. */

/* Some broken system (all SunOS versions) have a lower limit for the
   maximum number of stdio files that can be open, than the limit of open
   file the OS will let you have. If this value is > 0 (and ``stdio-fdmax''
   is *not* used in the config file), then all non-stdio file descriptors
   will be kept above this value (by dup'ing them). */
#if defined (sun)
# if defined (__SVR4)
#  define MAX_STDIO_FD 256
# else
#  define MAX_STDIO_FD 128
# endif
#else
# define MAX_STDIO_FD 0
#endif

/* some timer constants */

typedef enum { TMR_IDLE = TMR_APPLICATION, TMR_BACKLOGSTATS,
  TMR_STATUSFILE, TMR_NEWARTICLE, TMR_READART, TMR_PREPART, TMR_READ,
  TMR_WRITE, TMR_CALLBACK, TMR_MAX
} TMRTYPE;

#endif /* innfeed_h__ */
