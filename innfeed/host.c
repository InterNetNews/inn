/* -*- c -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Thu Dec 28 17:29:05 1995
 * Project:     INN (innfeed)
 * File:        host.c
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
 * Description: Implementation of the Host class.
 * 
 */

#if ! defined (lint)
static const char *rcsid = "$Id$" ;
static void use_rcsid (const char *rid) {   /* Never called */
  use_rcsid (rcsid) ; use_rcsid (rid) ;
}
#endif


#include "config.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/param.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <math.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h> /* LONG_MAX */
#include <float.h>

#include "host.h"
#include "tape.h"
#include "connection.h"
#include "article.h"
#include "buffer.h"
#include "endpoint.h"
#include "innlistener.h"
#include "msgs.h"
#include "configfile.h"

#define REQ 1
#define NOTREQ 0
#define NOTREQNOADD 2

#define VALUE_OK 0
#define VALUE_TOO_HIGH 1
#define VALUE_TOO_LOW 2
#define VALUE_MISSING 3
#define VALUE_WRONG_TYPE 4

extern char *configFile ;
extern mainLogStatus (FILE *fp) ;

/* the host keeps a couple lists of these */
typedef struct proc_q_elem 
{
    Article article ;
    struct proc_q_elem *next ;
    struct proc_q_elem *prev ;
} *ProcQElem ;

struct host_s 
{
    InnListener listener ;      /* who created me. */
    char *peerName ;            /* name INN calls the remote */
    char *ipName ;              /* IP name/address from config file. */
    char **ipAddrs ;		/* the ip addresses of the remote */
    char **nextIpAddr ;		/* the next ip address to hand out */

    Connection *connections ;   /* NULL-terminated list of all connections */
    bool *cxnActive ;           /* true if the corresponding cxn is active */
    bool *cxnSleeping ;         /* true if the connection is sleeping */
    u_int maxConnections ;      /* max # of connections to set up */
    u_int activeCxns ;          /* number of connections currently active */
    u_int sleepingCxns ;        /* number of connections currently sleeping */
    u_int initialCxns ;         /* number of connections to create at start */
    Connection blockedCxn ;     /* the first connection to get the 400 banner*/
    Connection notThisCxn ;	/* don't offer articles to this connection */
    double lowPassLow ;		/* value of low-pass filter off threshold */
    double lowPassHigh ;	/* value of low-pass filter on threshold */

    bool wantStreaming ;        /* true if config file says to stream. */
    bool remoteStreams ;        /* true if remote supports streaming */
    
    u_int maxChecks ;           /* max number of CHECK commands to send */
    u_int articleTimeout ;      /* max time to wait for new article */
    u_int responseTimeout ;     /* max time to wait for response from remote */
    u_short port ;              /* port the remote listens on */

    ProcQElem queued ;          /* articles done nothing with yet. */
    ProcQElem queuedTail ;

    ProcQElem processed ;       /* articles given to a Connection */
    ProcQElem processedTail ;
    
    TimeoutId statsId ;         /* timeout id for stats logging. */
#ifdef DYNAMIC_CONNECTIONS
    TimeoutId ChkCxnsId ;     /* timeout id for dynamic connections */
#endif

    Tape myTape ;
    
    bool backedUp ;             /* set to true when all cxns are full */
    u_int backlog ;             /* number of arts in `queued' queue */

    bool loggedModeOn ;         /* true if we logged going into no-CHECK mode */
    bool loggedModeOff ;        /* true if we logged going out of no-CHECK mode */

    bool loggedBacklog ;        /* true if we already logged the fact */
    bool notifiedChangedRemBlckd ; /* true if we logged a new response 400 */
    bool removeOnReload ;	/* set to false when host is in config */

    /* these numbers get reset periodically (after a 'final' logging). */
    u_int artsOffered ;         /* # of articles we offered to remote. */
    u_int artsAccepted ;        /* # of articles succesfully transferred */
    u_int artsNotWanted ;       /* # of articles remote already had */
    u_int artsRejected ;        /* # of articles remote rejected */
    u_int artsDeferred ;        /* # of articles remote asked us to retry */
    u_int artsMissing ;         /* # of articles whose file was missing. */
    u_int artsToTape ;          /* # of articles given to tape */
    u_int artsQueueOverflow ;   /* # of articles that overflowed `queued' */
    u_int artsCxnDrop ;         /* # of articles caught in dead cxn */
    u_int artsHostSleep ;       /* # of articles spooled by sleeping host */
    u_int artsHostClose ;       /* # of articles caught by closing host */
    u_int artsFromTape ;        /* # of articles we pulled off tape */

#ifdef DYNAMIC_CONNECTIONS
    /* Dynamic Peerage - MGF */
    u_int absMaxConnections ;   /* perHost limit on number of connections */
    u_int artsProcLastPeriod ;  /* # of articles processed in last period */
    u_int secsInLastPeriod ;    /* Number of seconds in last period */
    u_int lastCheckPoint ;      /* total articles at end of last period */
    bool maxCxnChk ;            /* check for maxConnections */
    time_t lastMaxCxnTime ;     /* last time a maxConnections increased */
    time_t lastChkTime;         /* last time a check was made for maxConnect */
    u_int nextCxnTimeChk ;      /* next check for maxConnect */
#endif

    /* These numbers are as above, but for the life of the process. */
    u_int gArtsOffered ;        
    u_int gArtsAccepted ;
    u_int gArtsNotWanted ;
    u_int gArtsRejected ;
    u_int gArtsDeferred ;
    u_int gArtsMissing ;
    u_int gArtsToTape ;
    u_int gArtsQueueOverflow ;
    u_int gArtsCxnDrop ;
    u_int gArtsHostSleep ;
    u_int gArtsHostClose ;
    u_int gArtsFromTape ;

    time_t firstConnectTime ;   /* time of first connect. */
    time_t connectTime ;        /* the time the first connection was fully
                                   set up (MODE STREAM and everything
                                   else). */
    time_t spoolTime ;          /* the time the Host had to revert to
                                   spooling articles to tape. */
    time_t lastSpoolTime ;      /* the time the last time the Host had to
                                   revert to spooling articles to tape. */
    time_t nextIpLookup ;	/* time of last IP name resolution */

    char *blockedReason ;       /* what the 400 from the remote says. */
    
    Host next ;                 /* for global list of hosts. */

    u_int blNone ;              /* number of times the backlog was 0 */
    u_int blFull ;              /* number of times the backlog was full */
    u_int blQuartile[4] ;       /* number of times in each quartile */
    u_long blAccum ;            /* cumulative backlog for computing mean */
    u_int blCount ;             /* the sample count */
};

/* A holder for the info we got out of the config file, but couldn't create
   the Host object for (normally due to lock-file problems).*/
typedef struct host_holder_s
{
    char *name ;
    char *ipname ;
    u_int arttout ;
    u_int resptout ;
    u_int initcxns ;
    u_int maxcxns ;
    u_int maxchecks ;
    bool stream ;
    double lowf ;
    double highf ;
    u_short portnum ;
    u_int backloglim ;

    struct host_holder_s *next ;
} *HostHolder ;


/* These numbers are as above, but for all hosts over
   the life of the process. */
long procArtsOffered ;        
long procArtsAccepted ;
long procArtsNotWanted ;
long procArtsRejected ;
long procArtsDeferred ;
long procArtsMissing ;


static u_int defaultArticleTimeout ;
static u_int defaultResponseTimeout ;
static u_int defaultInitialConnections ;
static u_int defaultMaxConnections ;
static u_int defaultMaxChecks ;
static bool defaultStreaming ;
static double defaultLowFilter ;
static double defaultHighFilter ;
static u_short defaultPortNumber ;
static u_int defaultBacklogLimit ;
static u_int defaultBacklogLimitHigh ;
static double defaultBacklogFactor ;
static bool factorSet ;

static HostHolder blockedHosts ; /* lists of hosts we can't lock */
static time_t lastStatusLog ;

  /*
   * Host object private methods.
   */
static void articleGone (Host host, Connection cxn, Article article) ;
static void hostStopSpooling (Host host) ;
static void hostStartSpooling (Host host) ;
static void hostLogStats (Host host, bool final) ;
static void hostStatsTimeoutCbk (TimeoutId tid, void *data) ;
static void backlogToTape (Host host) ;
static void queuesToTape (Host host) ;
static bool amClosing (Host host) ;
static void hostLogStatus (void) ;
static void hostPrintStatus (Host host, FILE *fp) ;
static int validateBool (FILE *fp, const char *name,
                         int required, bool setval);
static int validateReal (FILE *fp, const char *name, double low,
                          double high, int required, double setval);
static int validateInteger (FILE *fp, const char *name,
                     long low, long high, int required, long setval);

static bool getHostInfo (char **name,
                         char **ipname,
                         u_int *articleTimeout,
                         u_int *responseTimeout,
                         u_int *initialConnections,
                         u_int *maxConnections,
                         u_int *maxChecks,
                         bool *streaming,
                         double *lowFilter,
                         double *highFilter,
                         u_short *portNumber) ;
static Host findHostByName (char *name) ;
static void hostCleanup (void) ;


/* article queue management functions */
static Article remHead (ProcQElem *head, ProcQElem *tail) ;
static void queueArticle (Article article, ProcQElem *head, ProcQElem *tail) ;
static bool remArticle (Article article, ProcQElem *head, ProcQElem *tail) ;





/*
 * Host class data
 */

/* if true then when a Host logs its stats, it has all its connections
   log theirs too. */
static bool logConnectionStats = (bool) LOG_CONNECTION_STATS ;

/* The frequency in seconds with which a Host will log its stats. */
static time_t statsPeriod = STATS_PERIOD ;
static time_t statsResetPeriod = STATS_RESET_PERIOD ;

static Host gHostList = NULL ;

static u_int gHostCount = 0 ;

static u_int maxIpNameLen = 0 ;
static u_int maxPeerNameLen = 0 ;

static u_int hostHighwater = HOST_HIGHWATER ;
static time_t start ;
static char startTime [30] ;    /* for ctime(3) */
static pid_t myPid ;

static char *statusFile = NULL ;
static u_int dnsRetPeriod ;
static u_int dnsExpPeriod ;
static u_int defClosePeriod ;

static bool genHtml = false ;

/*******************************************************************/
/*                  PUBLIC FUNCTIONS                               */
/*******************************************************************/


/* function called when the config file is loaded */
int hostConfigLoadCbk (void *data)
{
  int rval = 1, vival, bval ;
  long iv ;
  FILE *fp = (FILE *) data ;
  value *v ;
  double l, h ;
  char *p ;
  
  /* get optional global defaults */
  if (getInteger (topScope,"dns-retry",&iv,NO_INHERIT))
    {
      if (iv < 1)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,LESS_THAN_ONE,"dns-retry",
                      iv,"global scope",(long)DNS_RETRY_PERIOD) ;
          iv = DNS_RETRY_PERIOD ;
        }
    }
  else
    iv = DNS_RETRY_PERIOD ;
  dnsRetPeriod = (u_int) iv ;
  
  
  if (getInteger (topScope,"dns-expire",&iv,NO_INHERIT))
    {
      if (iv < 1)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,LESS_THAN_ONE,"dns-expire",iv,
                      "global scope",(long)DNS_EXPIRE_PERIOD) ;
          iv = DNS_EXPIRE_PERIOD ;
        }
    }
  else
    iv = DNS_EXPIRE_PERIOD ;
  dnsExpPeriod = (u_int) iv ;


  if (getInteger (topScope,"close-period",&iv,NO_INHERIT))
    {
      if (iv < 1)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,LESS_THAN_ONE,"close-period",iv,
                      "global scope",(long)CLOSE_PERIOD) ;
          iv = CLOSE_PERIOD ;
        }
    }
  else
    iv = CLOSE_PERIOD ;
  defClosePeriod = CLOSE_PERIOD ;
  
  
  if (getBool (topScope,"gen-html",&bval,NO_INHERIT))
    genHtml = (bval ? true : false) ;
  else
    genHtml = GEN_HTML ;
  
  
  if (getString (topScope,"status-file",&p,NO_INHERIT))
    {
      hostSetStatusFile (p) ;
      FREE (p) ;
    }
  else
    hostSetStatusFile (INNFEED_STATUS) ;
  
  
  if (getBool (topScope,"connection-stats",&bval,NO_INHERIT))
    logConnectionStats = (bval ? true : false) ;
  else
    logConnectionStats = (LOG_CONNECTION_STATS ? true : false) ;


  if (getInteger (topScope,"host-queue-highwater", &iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,LESS_THAN_ZERO,"host-queue-highwater",
                      iv,"global scope",(long) HOST_HIGHWATER) ;
          iv = HOST_HIGHWATER ;
        }
    }
  else
    iv = HOST_HIGHWATER ;
  hostHighwater = (u_int) iv ;

  if (getInteger (topScope,"stats-period",&iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,LESS_THAN_ZERO,"stats-period",
                      iv,"global scope",(long)STATS_PERIOD) ;
          iv = STATS_PERIOD ;
        }
    }
  else
    iv = STATS_PERIOD ;
  statsPeriod = (u_int) iv ;


  if (getInteger (topScope,"stats-reset",&iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,LESS_THAN_ZERO,"stats-reset",iv,
                      "global scope",(long)STATS_RESET_PERIOD) ;
          iv = STATS_RESET_PERIOD ;
        }
    }
  else
    iv = STATS_RESET_PERIOD ;
  statsResetPeriod = (u_int) iv ;
  
  
  /* check required global defaults are there and have good values */
  
#define ERROR_CHECK                                     \
  do{                                                   \
    if(vival==VALUE_WRONG_TYPE)                         \
      {                                                 \
        logOrPrint(LOG_CRIT,fp,"cannot continue");      \
        exit(1);                                        \
      }                                                 \
    else if(vival != VALUE_OK)                              \
      rval = 0;                                         \
  } while(0)

  vival = validateInteger(fp,"article-timeout",0,LONG_MAX,REQ,ARTTOUT);
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"article-timeout",&iv,NO_INHERIT) ;
  defaultArticleTimeout = (u_int) iv ;
  
  vival = validateInteger(fp,"response-timeout",0,LONG_MAX,REQ,RESPTOUT) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"response-timeout",&iv,NO_INHERIT) ;
  defaultResponseTimeout = (u_int) iv ;
    
  vival = validateInteger(fp,"initial-connections",0,LONG_MAX,REQ,INIT_CXNS) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"initial-connections",&iv,NO_INHERIT) ;
  defaultInitialConnections = (u_int) iv ;
  
  vival = validateInteger (fp,"max-connections",1,LONG_MAX,REQ,MAX_CXNS) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"max-connections",&iv,NO_INHERIT) ;
  defaultMaxConnections = (u_int) iv ;
  
  vival = validateInteger (fp,"max-queue-size",1,LONG_MAX,REQ,MAX_Q_SIZE) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"max-queue-size",&iv,NO_INHERIT) ;
  defaultMaxChecks = (u_int) iv ;
  
  vival = validateBool (fp,"streaming",REQ,STREAM) ;
  ERROR_CHECK ;
  bval = 0 ;
  (void) getBool (topScope,"streaming",&bval,NO_INHERIT) ;
  defaultStreaming = (bval ? true : false)  ;
  
  vival = validateReal (fp,"no-check-high",0.0,100.0,REQ,NOCHECKHIGH) ;
  ERROR_CHECK ;
  l = 0.0 ;
  (void) getReal (topScope,"no-check-high",&l,NO_INHERIT) ;
  defaultHighFilter = l / 10.0 ;

  vival = validateReal (fp,"no-check-low",0.0,100.0,REQ,NOCHECKLOW) ;
  ERROR_CHECK ;
  l = 0.0 ;
  (void) getReal (topScope,"no-check-low",&l,NO_INHERIT) ;
  defaultLowFilter = l / 10.0  ;

  vival = validateInteger (fp,"port-number",0,LONG_MAX,REQ,PORTNUM) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"port-number",&iv,NO_INHERIT) ;
  defaultPortNumber = (u_int) iv ;
  
  if (findValue (topScope,"backlog-factor",NO_INHERIT) == NULL &&
      findValue (topScope,"backlog-limit-high",NO_INHERIT) == NULL)
    {
      logOrPrint (LOG_ERR,fp,NOFACTORHIGH,"backlog-factor",LIMIT_FUDGE) ;
      addReal (topScope,"backlog-factor",LIMIT_FUDGE) ;
      rval = 0 ;
    }
  
  vival = validateInteger (fp,"backlog-limit",0,LONG_MAX,REQ,BLOGLIMIT) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"backlog-limit",&iv,NO_INHERIT) ;
  defaultBacklogLimit = (u_int) iv ;

  vival = validateInteger (fp,"backlog-limit-high",0,LONG_MAX,
                           NOTREQNOADD,BLOGLIMIT_HIGH) ;
  ERROR_CHECK ;
  iv = 0 ;
  (void) getInteger (topScope,"backlog-limit-high",&iv,NO_INHERIT) ;
  defaultBacklogLimitHigh = (u_int) iv ;

  vival = validateReal (fp,"backlog-factor",1.0,DBL_MAX,
                        NOTREQNOADD,LIMIT_FUDGE) ;
  ERROR_CHECK ;
  factorSet = (vival == VALUE_OK ? true : false) ;
  l = 0.0 ;
  (void) getReal (topScope,"backlog-factor",&l,NO_INHERIT) ;
  defaultBacklogFactor = l ;

  if (getReal (topScope,"no-check-low",&l,NO_INHERIT) &&
      getReal (topScope,"no-check-high",&h,NO_INHERIT))
    {
      if (l > h)
        {
          logOrPrint (LOG_ERR,fp,NOCK_LOWVSHIGH,l,h,NOCHECKLOW,NOCHECKHIGH) ;
          rval = 0 ;
          v = findValue (topScope,"no-check-now",NO_INHERIT) ;
          v->v.real_val = NOCHECKLOW ;
          v = findValue (topScope,"no-check-high",NO_INHERIT) ;
          v->v.real_val = NOCHECKHIGH ;
        }
      else if (h - l < 5.0)
        logOrPrint (LOG_WARNING,fp,NOCK_LOWHIGHCLOSE,l,h) ;
    }


  return rval ;
}




void configHosts (bool talkSelf)
{
  Host nHost, h, q ;
  char *name = NULL, *ipName = NULL ;
  u_int artTout, respTout, initCxns, maxCxns, maxChecks ;
  double lowFilter, highFilter ;
  bool streaming ;
  u_short portNum ;
  HostHolder hh, hi ;

  name = ipName = NULL ;

  for (hh = blockedHosts, hi = NULL ; hh != NULL ; hh = hi)
    {
      FREE (hh->name) ;
      FREE (hh->ipname) ;
      hi = hh->next ;
      FREE (hh) ;
    }
  blockedHosts = NULL ;

  closeDroppedArticleFile () ;
  openDroppedArticleFile () ;

  while (getHostInfo (&name, &ipName, &artTout, &respTout, &initCxns,
                      &maxCxns, &maxChecks, &streaming, &lowFilter,
                      &highFilter,&portNum))
    {
      h = findHostByName (name) ;
      if ( h != NULL )
        {
	  u_int i ;

	  if (strcmp(h->ipName, ipName) != 0)
	    {
	      FREE (h->ipName) ;
	      h->ipName = strdup (ipName) ;
	      h->nextIpLookup = theTime () ;
	    }

	  h->articleTimeout = artTout ;
	  h->responseTimeout = respTout ;
	  h->maxChecks = maxChecks ;
	  h->wantStreaming = streaming ;
	  h->port = portNum ;
	  h->removeOnReload = false ;
 	  h->lowPassLow = lowFilter ;   /* the off threshold */
 	  h->lowPassHigh = highFilter ; /* the on threshold */

	  if (maxCxns != h->maxConnections)
	    {
	      if (maxCxns < h->maxConnections)
		{
		  for ( i = h->maxConnections ; i > maxCxns ; i-- )
		    {
		      /* XXX this is harsh, and arguably there could be a
			 cleaner way of doing it.  the problem being addressed
			 by doing it this way is that eventually a connection
			 closed cleanly via cxnClose can end up ultimately
			 calling hostCxnDead after h->maxConnections has
			 been lowered and the relevant arrays downsized.
			 If trashing the old, unallocated space in
			 hostCxnDead doesn't kill the process, the
			 ASSERT against h->maxConnections surely will.
		       */
                      if (h->connections[i - 1] != NULL)
                        cxnNuke (h->connections[i-1]) ;
		    }
		  h->maxConnections = maxCxns ;
		}

	      h->connections =
		REALLOC (h->connections, Connection, maxCxns + 1);
	      ASSERT (h->connections != NULL) ;
	      h->cxnActive = REALLOC (h->cxnActive, bool, maxCxns) ;
	      ASSERT (h->cxnActive != NULL) ;
	      h->cxnSleeping = REALLOC (h->cxnSleeping, bool, maxCxns) ;
	      ASSERT (h->cxnSleeping != NULL) ;

	      /* if maximum was raised, establish the new connexions
		 (but don't start using them).
		 XXX maybe open them based on initCxns? */
	      if (maxCxns > h->maxConnections)
		{
		  i = h->maxConnections ;
		  /* need to set h->maxConnections before cxnWait() */
#ifndef DYNAMIC_CONNECTIONS
		  h->maxConnections = maxCxns ;
#else
                  if(maxCxns != 1)
                    h->absMaxConnections = maxCxns ;
                  else
                    h->absMaxConnections = 0 ;
                  h->maxConnections = maxCxns = 1 ;
#endif
		  while ( i < maxCxns )
		    {
		      /* XXX this does essentially the same thing that happens
			 in newHost, so they should probably be combined
			 to one new function */
		      h->connections [i] =
			newConnection (h, i,
				       h->ipName,
				       h->articleTimeout,
				       h->port,
				       h->responseTimeout,
				       defClosePeriod, lowFilter, highFilter) ;
		      h->cxnActive [i] = false ;
		      h->cxnSleeping [i] = false ;
		      cxnWait (h->connections [i]) ;
		      i++ ;
		    }
		}
	    }

	  for ( i = 0 ; i < maxCxns ; i++ )
            if (h->connections[i] != NULL)
              cxnSetCheckThresholds (h->connections[i],lowFilter,highFilter) ;

	  /* XXX how to handle initCxns change? */
        }
      else
        {
	  nHost = newHost (mainListener,name,ipName,artTout,respTout,initCxns,
			   maxCxns,maxChecks,portNum,CLOSE_PERIOD,
			   streaming,lowFilter,highFilter) ;

	  if (nHost == NULL)
	    {
              /* this is where we'll die if the locks are still held. */
              hh = ALLOC (struct host_holder_s,1) ;

              hh->name = name ; name = NULL ;
              hh->ipname = ipName ; ipName = NULL ;
              hh->arttout = artTout ;
              hh->resptout = respTout ;
              hh->initcxns = initCxns ;
              hh->maxcxns = maxCxns ;
              hh->maxchecks = maxChecks ;
              hh->stream = streaming ;
              hh->lowf = lowFilter ;
              hh->highf = highFilter ;
              hh->portnum = portNum ;
              hh->backloglim = 0 ;

              hh->next = blockedHosts ;
              blockedHosts = hh ;
               
	      syslog (LOG_ERR,NO_HOST,hh->name) ;
	    }
	  else 
	    {
	      if (initCxns == 0 && talkSelf)
		syslog (LOG_NOTICE,BATCH_AND_NO_CXNS,name) ;
	      
	      dprintf (1,"Adding %s %s article (%d) response (%d) initial (%d) max con (%d) max checks (%d) portnumber (%d) streaming (%s) lowFilter (%.2f) highFilter (%.2f)\n",
		       name, ipName, artTout, respTout, initCxns, maxCxns,
		       maxChecks, portNum, streaming ? "true" : "false",
		       lowFilter, highFilter) ;

              FREE (name) ;
              FREE (ipName) ;
              
	      if ( !listenerAddPeer (mainListener,nHost) )
		die ("failed to add a new peer\n") ;
	    }
	}

      name = ipName = NULL ;
    }
  

  for (h = gHostList; h != NULL; h = q) 
    {
      q = h->next ;
      if (h->removeOnReload)
        hostClose (h) ;         /* h may be deleted in here. */
      else
        /* prime it for the next config file read */
        h->removeOnReload = true ;
    }

  hostLogStatus () ;
}

/*
 * Create a new Host object. Called by the InnListener.
 */

static bool inited = false ;
Host newHost (InnListener listener,
              const char *name, 
              const char *ipName,
              u_int artTimeout, 
              u_int respTimeout,
              u_int initialCxns,
              u_int maxCxns,
              u_int maxCheck,
              u_short portNum,
              u_int closePeriod,
              bool streaming,
              double lowPassLow,
              double lowPassHigh) 
{
  u_int i ;
  Host nh ; 

  ASSERT (maxCxns > 0) ;
  ASSERT (maxCheck > 0) ;

  if (!inited)
    {
      inited = true ;
      atexit (hostCleanup) ;
    }
  

  nh =  CALLOC (struct host_s, 1) ;
  ASSERT (nh != NULL) ;

  nh->listener = listener ;
  nh->peerName = strdup (name) ;
  nh->ipName = strdup (ipName) ;

  nh->connections = CALLOC (Connection, maxCxns + 1) ;
  ASSERT (nh->connections != NULL) ;

  nh->cxnActive = CALLOC (bool, maxCxns) ;
  ASSERT (nh->cxnActive != NULL) ;

  nh->cxnSleeping = CALLOC (bool, maxCxns) ;
  ASSERT (nh->cxnSleeping != NULL) ;

#ifndef DYNAMIC_CONNECTIONS
  nh->maxConnections = maxCxns ;
#else
  /* if maxCxns == 1, then no limit on maxConnections */
  if(maxCxns != 1)
    nh->absMaxConnections = maxCxns ;
  else
    nh->absMaxConnections = 0 ;
  nh->maxConnections = maxCxns = 1;
#endif
  nh->activeCxns = 0 ;
  nh->sleepingCxns = 0 ;
  nh->initialCxns = initialCxns ;
  nh->lowPassLow = lowPassLow ;
  nh->lowPassHigh = lowPassHigh ;

  nh->blockedCxn = NULL ;
  nh->notThisCxn = NULL ;
  nh->maxChecks = maxCheck ;
  nh->articleTimeout = artTimeout ;
  nh->responseTimeout = respTimeout ;
  nh->port = portNum ;
  nh->wantStreaming = streaming ;

  nh->queued = NULL ;
  nh->queuedTail = NULL ;

  nh->processed = NULL ;
  nh->processedTail = NULL ;
  
  nh->statsId = 0 ;
#ifdef DYNAMIC_CONNECTIONS
  nh->ChkCxnsId = 0 ;
#endif

  nh->myTape = newTape (name,listenerIsDummy (listener)) ;
  if (nh->myTape == NULL)
    {                           /* tape couldn't be locked, probably */
      FREE (nh->peerName) ;
      FREE (nh->ipName) ;
      FREE (nh->connections) ;
      FREE (nh->cxnActive) ;
      FREE (nh->cxnSleeping) ;
      
      FREE (nh) ;
      return NULL ;
    }

  nh->backedUp = false ;
  nh->backlog = 0 ;

  nh->loggedBacklog = false ;
  nh->loggedModeOn = false ;
  nh->loggedModeOff = false ;
  nh->notifiedChangedRemBlckd = false ;
  nh->removeOnReload = false ;

  nh->artsOffered = 0 ;
  nh->artsAccepted = 0 ;
  nh->artsNotWanted = 0 ;
  nh->artsRejected = 0 ;
  nh->artsDeferred = 0 ;
  nh->artsMissing = 0 ;
  nh->artsToTape = 0 ;
  nh->artsQueueOverflow = 0 ;
  nh->artsCxnDrop = 0 ;
  nh->artsHostSleep = 0 ;
  nh->artsHostClose = 0 ;
  nh->artsFromTape = 0 ;

#ifdef DYNAMIC_CONNECTIONS
  nh->artsProcLastPeriod = 0;
  nh->secsInLastPeriod = 0;
  nh->lastCheckPoint = 0;
  nh->maxCxnChk = true;
  nh->lastMaxCxnTime = time(0);
  nh->lastChkTime = time(0);
  nh->nextCxnTimeChk = 30;
#endif

  nh->gArtsOffered = 0 ;
  nh->gArtsAccepted = 0 ;
  nh->gArtsNotWanted = 0 ;
  nh->gArtsRejected = 0 ;
  nh->gArtsDeferred = 0 ;
  nh->gArtsMissing = 0 ;
  nh->gArtsToTape = 0 ;
  nh->gArtsQueueOverflow = 0 ;
  nh->gArtsCxnDrop = 0 ;
  nh->gArtsHostSleep = 0 ;
  nh->gArtsHostClose = 0 ;
  nh->gArtsFromTape = 0 ;
  
  nh->firstConnectTime = 0 ;
  nh->connectTime = 0 ;
  
  nh->spoolTime = 0 ;

  nh->blNone = 0 ;
  nh->blFull = 0 ;
  nh->blQuartile[0] = nh->blQuartile[1] = nh->blQuartile[2] =
		      nh->blQuartile[3] = 0 ;
  nh->blAccum = 0;
  nh->blCount = 0;

  /* Create all the connections, but only the initial ones connect
     immediately */
  for (i = 0 ; i < maxCxns ; i++)
    {
      nh->cxnActive [i] = false ;
      nh->cxnSleeping [i] = false ;
      nh->connections [i] = newConnection (nh,
                                           i,
                                           nh->ipName,
                                           nh->articleTimeout,
                                           nh->port,
                                           nh->responseTimeout,
                                           closePeriod,
                                           nh->lowPassLow,
                                           nh->lowPassHigh) ;
      if (i < initialCxns)
        cxnConnect (nh->connections [i]) ;
      else
        cxnWait (nh->connections [i]) ;
    }

  nh->connections [maxCxns] = NULL ;

  nh->next = gHostList ;
  gHostList = nh ;
  gHostCount++ ;

  if (maxIpNameLen == 0)
    {
      start = theTime() ;
      strcpy (startTime,ctime (&start)) ;
      myPid = getpid() ;
    }
  
  if (strlen (nh->ipName) > maxIpNameLen)
    maxIpNameLen = strlen (nh->ipName) ;
  if (strlen (nh->peerName) > maxPeerNameLen)
    maxPeerNameLen = strlen (nh->peerName) ;
  
  return nh ;
}

struct in_addr *hostIpAddr (Host host)
{
  int i ;
  char *p ;
  char **newIpAddrs = NULL;
  struct in_addr ipAddr, *returnAddr ;
  struct hostent *hostEnt ;

  /* check to see if need to look up the host name */
  if (host->nextIpLookup <= theTime())
    {
      /* see if the ipName we're given is a dotted quad */
      if ( !inet_aton (host->ipName,&ipAddr) )
	{
	  if ((hostEnt = gethostbyname (host->ipName)) == NULL)
	    syslog (LOG_ERR, HOST_RESOLV_ERROR, host->peerName, host->ipName,
		    host_err_str ()) ;
	  else
	    {
	      /* figure number of pointers that need space */
	      for (i = 0 ; hostEnt->h_addr_list[i] ; i++)
		;
	      i++;		

	      newIpAddrs =
		(char **) MALLOC ( (i * sizeof(char *)) +
				  ( (i - 1) * hostEnt->h_length) ) ;
	      ASSERT (newIpAddrs != NULL) ;

	      /* copy the addresses from gethostbyname() static space */
	      p = (char *)&newIpAddrs [ i ] ;
	      i = 0;
	      for (i = 0 ; hostEnt->h_addr_list[i] ; i++)
		{
		  newIpAddrs[i] = p;
		  memcpy (p, hostEnt->h_addr_list[i], hostEnt->h_length) ;
		  p += hostEnt->h_length ;
		}
	      newIpAddrs[i] = NULL ;
	    }
	}
      else
	{
	  newIpAddrs = (char **) MALLOC (2 * sizeof(char *) + sizeof(ipAddr)) ;
	  ASSERT (newIpAddrs != NULL) ;
	  p = (char *)&newIpAddrs [ 2 ];
	  newIpAddrs[0] = p;
	  memcpy (p, (char *)&ipAddr, sizeof(ipAddr)) ;
	  newIpAddrs[1] = NULL;
	}

      if (newIpAddrs)
	{
	  if (host->ipAddrs)
	    FREE (host->ipAddrs) ;
	  host->ipAddrs = newIpAddrs ;
	  host->nextIpAddr = host->ipAddrs ;
	  host->nextIpLookup = theTime () + dnsExpPeriod ;
	}
      else
	{
	  /* failed to setup new addresses */
	  host->nextIpLookup = theTime () + dnsRetPeriod ;
	}
    }

  if (host->ipAddrs)
    {
      returnAddr = (struct in_addr *)(host->nextIpAddr[0]) ;
      if (*(++host->nextIpAddr) == NULL)
	host->nextIpAddr = host->ipAddrs ;
    }
  else
    returnAddr = NULL ;

  return returnAddr ;
}


void gPrintHostInfo (FILE *fp, u_int indentAmt)
{
  Host h ;
  char indent [INDENT_BUFFER_SIZE] ;
  u_int i ;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;
  
  fprintf (fp,"%sGlobal Host list : (count %d) {\n",indent,gHostCount) ;
  
  for (h = gHostList ; h != NULL ; h = h->next)
    printHostInfo (h,fp,indentAmt + INDENT_INCR) ;
  
  fprintf (fp,"%s}\n",indent) ;
}


void printHostInfo (Host host, FILE *fp, u_int indentAmt)
{
  char indent [INDENT_BUFFER_SIZE] ;
  u_int i ;
  ProcQElem qe ;
  double cnt = (host->blCount) ? (host->blCount) : 1.0;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sHost : %p {\n",indent,host) ;

  if (host == NULL)
    {
      fprintf (fp,"%s}\n",indent) ;
      return ;
    }
  
  fprintf (fp,"%s    peer-name : %s\n",indent,host->peerName) ;
  fprintf (fp,"%s    ip-name : %s\n",indent,host->ipName) ;
  fprintf (fp,"%s    max-connections : %d\n",indent,host->maxConnections) ;
  fprintf (fp,"%s    active-connections : %d\n",indent,host->activeCxns) ;
  fprintf (fp,"%s    sleeping-connections : %d\n",indent,host->sleepingCxns) ;
  fprintf (fp,"%s    remote-streams : %s\n",indent,
           boolToString (host->remoteStreams)) ;
  fprintf (fp,"%s    max-checks : %d\n",indent,host->maxChecks) ;
  fprintf (fp,"%s    article-timeout : %d\n",indent,host->articleTimeout) ;
  fprintf (fp,"%s    response-timeout : %d\n",indent,host->responseTimeout) ;
  fprintf (fp,"%s    port : %d\n",indent,host->port) ;
  fprintf (fp,"%s    statistics-id : %d\n",indent,host->statsId) ;
#ifdef DYNAMIC_CONNECTIONS
  fprintf (fp,"%s    ChkCxns-id : %d\n",indent,host->ChkCxnsId) ;
#endif
  fprintf (fp,"%s    backed-up : %s\n",indent,boolToString (host->backedUp));
  fprintf (fp,"%s    backlog : %d\n",indent,host->backlog) ;
  fprintf (fp,"%s    loggedModeOn : %s\n",indent,
           boolToString (host->loggedModeOn)) ;
  fprintf (fp,"%s    loggedModeOff : %s\n",indent,
           boolToString (host->loggedModeOff)) ;
  fprintf (fp,"%s    logged-backlog : %s\n",indent,
           boolToString (host->loggedBacklog)) ;
  fprintf (fp,"%s    streaming-type changed : %s\n",indent,
           boolToString (host->notifiedChangedRemBlckd)) ;
  fprintf (fp,"%s    articles offered : %d\n",indent,host->artsOffered) ;
  fprintf (fp,"%s    articles accepted : %d\n",indent,host->artsAccepted) ;
  fprintf (fp,"%s    articles not wanted : %d\n",indent,
           host->artsNotWanted) ;
  fprintf (fp,"%s    articles rejected : %d\n",indent,host->artsRejected);
  fprintf (fp,"%s    articles deferred : %d\n",indent,host->artsDeferred) ;
  fprintf (fp,"%s    articles missing : %d\n",indent,host->artsMissing) ;
  fprintf (fp,"%s    articles spooled : %d\n",indent,host->artsToTape) ;
  fprintf (fp,"%s      because of queue overflow : %d\n",indent,
           host->artsQueueOverflow) ;
  fprintf (fp,"%s      when the we closed the host : %d\n",indent,
           host->artsHostClose) ;
  fprintf (fp,"%s      because the host was asleep : %d\n",indent,
           host->artsHostSleep) ;
  fprintf (fp,"%s    articles unspooled : %d\n",indent,host->artsFromTape) ;
  fprintf (fp,"%s    articles requeued from dropped connections : %d\n",indent,
           host->artsCxnDrop) ;

  fprintf (fp,"%s    process articles offered : %d\n",indent,
           host->gArtsOffered) ;
  fprintf (fp,"%s    process articles accepted : %d\n",indent,
           host->gArtsAccepted) ;
  fprintf (fp,"%s    process articles not wanted : %d\n",indent,
           host->gArtsNotWanted) ;
  fprintf (fp,"%s    process articles rejected : %d\n",indent,
           host->gArtsRejected);
  fprintf (fp,"%s    process articles deferred : %d\n",indent,
           host->gArtsDeferred) ;
  fprintf (fp,"%s    process articles missing : %d\n",indent,
           host->gArtsMissing) ;
  fprintf (fp,"%s    process articles spooled : %d\n",indent,
           host->gArtsToTape) ;
  fprintf (fp,"%s      because of queue overflow : %d\n",indent,
           host->gArtsQueueOverflow) ;
  fprintf (fp,"%s      when the we closed the host : %d\n",indent,
           host->gArtsHostClose) ;
  fprintf (fp,"%s      because the host was asleep : %d\n",indent,
           host->gArtsHostSleep) ;
  fprintf (fp,"%s    process articles unspooled : %d\n",indent,
           host->gArtsFromTape) ;
  fprintf (fp,"%s    process articles requeued from dropped connections : %d\n",
           indent, host->gArtsCxnDrop) ;

  fprintf (fp,"%s    average (mean) queue length : %.1f\n", indent,
           (double) host->blAccum / cnt) ;
  fprintf (fp,"%s      percentage of the time empty : %.1f\n", indent,
           100.0 * host->blNone / cnt) ;
  fprintf (fp,"%s      percentage of the time >0%%-25%% : %.1f\n", indent,
           100.0 * host->blQuartile[0] / cnt) ;
  fprintf (fp,"%s      percentage of the time 25%%-50%% : %.1f\n", indent,
           100.0 * host->blQuartile[1] / cnt) ;
  fprintf (fp,"%s      percentage of the time 50%%-75%% : %.1f\n", indent,
           100.0 * host->blQuartile[2] / cnt) ;
  fprintf (fp,"%s      percentage of the time 75%%-<100%% : %.1f\n", indent,
           100.0 * host->blQuartile[3] / cnt) ;
  fprintf (fp,"%s      percentage of the time full : %.1f\n", indent,
           100.0 * host->blFull / cnt) ;
  fprintf (fp,"%s      number of samples : %.1f\n", indent, host->blCount) ;

  fprintf (fp,"%s    firstConnectTime : %s",indent,
           ctime (&host->firstConnectTime));
  fprintf (fp,"%s    connectTime : %s",indent,ctime (&host->connectTime));
  fprintf (fp,"%s    spoolTime : %s",indent,ctime (&host->spoolTime)) ;
  fprintf (fp,"%s    last-spool-time : %s",indent,
           ctime (&host->lastSpoolTime)) ;
  
#if 0
  fprintf (fp,"%s    tape {\n",indent) ;
  printTapeInfo (host->myTape,fp,indentAmt + INDENT_INCR) ;
  fprintf (fp,"%s    }\n",indent) ;
#else
  fprintf (fp,"%s    tape : %p\n",indent,host->myTape) ;
#endif
  
  fprintf (fp,"%s    QUEUED articles {\n",indent) ;
  for (qe = host->queued ; qe != NULL ; qe = qe->next)
    {
#if 0
      printArticleInfo (qe->article,fp,indentAmt + INDENT_INCR) ;
#else
      fprintf (fp,"%s    %p\n",indent,qe->article) ;
#endif
    }
  
  fprintf (fp,"%s    }\n",indent) ;
  
  fprintf (fp,"%s    IN PROCESS articles {\n",indent) ;
  for (qe = host->processed ; qe != NULL ; qe = qe->next)
    {
#if 0
      printArticleInfo (qe->article,fp,indentAmt + INDENT_INCR) ;
#else
      fprintf (fp,"%s    %p\n",indent,qe->article) ;
#endif
    }
  
  fprintf (fp,"%s    }\n",indent) ;

  
  
  fprintf (fp,"%s    Connections {\n",indent) ;
  for (i = 0 ; i < host->maxConnections ; i++)
    {
#if 0
      if (host->connections[i] != NULL)
        printCxnInfo (*cxn,fp,indentAmt + INDENT_INCR) ;
#else
      fprintf (fp,"%s        %p\n",indent,host->connections[i]) ;
#endif
    }
  fprintf (fp,"%s    }\n",indent) ;

  fprintf (fp,"%s    Active Connections {\n%s        ",indent,indent) ;
  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->cxnActive[i])
      fprintf (fp," [%d:%p]",i,host->connections[i]) ;
  fprintf (fp,"\n%s    }\n",indent) ;

  fprintf (fp,"%s    Sleeping Connections {\n%s        ",indent,indent) ;
  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->cxnSleeping[i])
      fprintf (fp," [%d:%p]",i,host->connections[i]) ;
  fprintf (fp,"\n%s    }\n",indent) ;

  fprintf (fp,"%s}\n",indent) ;
}







/* close down all the connections of the Host. All articles that are in
 * processes are still pushed out and then a QUIT is issued. The Host will
 * also spool all inprocess articles to tape incase the process is about to
 * be killed (they'll be refused next time around). When all Connections
 * report that they're gone, then the Host will delete itself.
 */
void hostClose (Host host)
{
  u_int i ;
  u_int cxnCount ;

  dprintf (1,"Closing host %s\n",host->peerName) ;
  
  queuesToTape (host) ;
  delTape (host->myTape) ;
  host->myTape = NULL ;
  
  hostLogStats (host,true) ;

  clearTimer (host->statsId) ;
#ifdef DYNAMIC_CONNECTIONS
  clearTimer (host->ChkCxnsId) ;
#endif
  
  host->connectTime = 0 ;

  /* when we call cxnTerminate() on the last Connection, the Host objects
     will end up getting deleted out from under us (via hostCxnGone()). If
     we are running with a malloc that scribbles over memory after freeing
     it, then we'd fail in the second for loop test. Trying to access
     host->maxConnections. */
  for (i = 0, cxnCount = 0 ; i < host->maxConnections ; i++) 
    cxnCount += (host->connections [i] != NULL ? 1 : 0) ;
  for (i = 0 ; i < cxnCount ; i++)
    if (host->connections[i] != NULL)
      cxnTerminate (host->connections [i]) ;
}


#ifdef DYNAMIC_CONNECTIONS
/*
 * check if host should get more connections opened, or some closed...
 */
void hostChkCxns(TimeoutId tid, void *data) {
  Host host = (Host) data;
  u_int absMaxCxns, currArticles ;
  float lastAPS, currAPS ;

  if(!host->maxCxnChk)
    return;

  if(host->secsInLastPeriod > 0) 
    lastAPS = host->artsProcLastPeriod / (host->secsInLastPeriod * 1.0);
  if(lastAPS < 0.0) lastAPS = 0.0;

  currArticles = (host->gArtsAccepted + host->gArtsRejected +
                 (host->gArtsNotWanted / 4)) - host->lastCheckPoint ;

  currAPS = currArticles / (host->nextCxnTimeChk * 1.0) ;

  host->lastCheckPoint = host->gArtsAccepted +
                         host->gArtsRejected +
                        (host->gArtsNotWanted / 4) ;

  if(!host->absMaxConnections) absMaxCxns = host->maxConnections + 1 ;
  else absMaxCxns = host->absMaxConnections ;

  syslog(LOG_NOTICE, HOST_MAX_CONNECTIONS,
         host->peerName, currAPS, lastAPS, absMaxCxns, host->maxConnections);
 
  dprintf(1, "hostChkCxns: Chngs %f\n", currAPS - lastAPS);
 
  if(((currAPS - lastAPS) >= 0.1) && (host->maxConnections < absMaxCxns)) {
    u_int ii = host->maxConnections ;
    double lowFilter = host->lowPassLow ;   /* the off threshold */
    double highFilter = host->lowPassHigh ; /* the on threshold */
 
    dprintf(1, "hostChkCxns increasing, Chngs %f\n", currAPS - lastAPS);
 
    host->maxConnections += (int)(currAPS - lastAPS) + 1 ;
 
    host->connections =
      REALLOC (host->connections, Connection, host->maxConnections + 1);
    ASSERT (host->connections != NULL) ;
    host->cxnActive = REALLOC (host->cxnActive, bool,
                               host->maxConnections) ;
    ASSERT (host->cxnActive != NULL) ;
    host->cxnSleeping = REALLOC (host->cxnSleeping, bool,
                                 host->maxConnections) ;
    ASSERT (host->cxnSleeping != NULL) ;
 
    dprintf(1, "hostChkCxns %s maxC %d ii %d\n", host->ipName,
            host->maxConnections, ii);
    while(ii < host->maxConnections) {
 
      /* XXX this does essentially the same thing that happens
         in newHost, so they should probably be combined
         to one new function */
      dprintf(1, "hostChkCxns newConnection %d\n", ii);
      host->connections [ii] = newConnection (host,
                                              ii,
                                              host->ipName,
                                              host->articleTimeout,
                                              host->port,
                                              host->responseTimeout,
                                              defClosePeriod,
                                              lowFilter,
                                              highFilter) ;
      host->cxnActive [ii] = false ;
      host->cxnSleeping [ii] = false ;
      cxnConnect (host->connections [ii]) ;
      ii++;
      host->artsProcLastPeriod = currArticles;
      host->secsInLastPeriod = host->nextCxnTimeChk ;
    }
  } else {
    if ((currAPS - lastAPS) < -.2) {
      dprintf(1, "hostChkCxns decreasing, Chngs %f\n", currAPS - lastAPS);
      if(host->maxConnections != 1) {
 
        u_int ii = host->maxConnections;

        if (host->connections[ii - 1] != NULL)
          cxnNuke (host->connections[ii - 1]) ;
        host->maxConnections--;
        ii = host->maxConnections;

        host->connections =
          REALLOC (host->connections, Connection, ii + 1);
        ASSERT (host->connections != NULL) ;
        host->cxnActive = REALLOC (host->cxnActive, bool, ii) ;
        ASSERT (host->cxnActive != NULL) ;
        host->cxnSleeping = REALLOC (host->cxnSleeping, bool, ii) ;
        ASSERT (host->cxnSleeping != NULL) ;
      } 
      host->artsProcLastPeriod = currArticles ;
      host->secsInLastPeriod = host->nextCxnTimeChk ;
    } else 
      dprintf(1, "hostChkCxns doing nothing, Chngs %f\n", currAPS - lastAPS);
  }
  if(host->nextCxnTimeChk <= 480)
    host->nextCxnTimeChk *= 2;
  dprintf(1, "prepareSleep hostChkCxns, %d\n", host->nextCxnTimeChk);
  host->ChkCxnsId = prepareSleep(hostChkCxns, host->nextCxnTimeChk, host);
}
#endif





/*
 * have the Host transmit the Article if possible.
 */
void hostSendArticle (Host host, Article article)
{
  if (host->spoolTime > 0)
    {                           /* all connections are asleep */
      host->artsHostSleep++ ;
      host->gArtsHostSleep++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      tapeTakeArticle (host->myTape, article) ;
      return ;
    }

  /* at least one connection is feeding or waiting and there's no backlog */
  if (host->queued == NULL)
    {
      u_int idx ;
      Article extraRef ;
      
      extraRef = artTakeRef (article) ; /* the referrence we give away */
      
      /* stick on the queue of articles we've handed off--we're hopeful. */
      queueArticle (article,&host->processed,&host->processedTail) ;

      /* first we try to give it to one of our active connections. We
         simply start at the bottom and work our way up. This way
         connections near the end of the list will get closed sooner from
         idleness. */
      for (idx = 0 ; idx < host->maxConnections ; idx++)
        {
          if (host->cxnActive [idx] &&
              host->connections[idx] != host->notThisCxn &&
              cxnTakeArticle (host->connections [idx],extraRef))
	    return ;
        }

      /* Wasn't taken so try to give it to one of the waiting connections. */
      for (idx = 0 ; idx < host->maxConnections ; idx++)
        if (!host->cxnActive [idx] && !host->cxnSleeping [idx] &&
            host->connections[idx] != host->notThisCxn)
          {
            if (cxnTakeArticle (host->connections [idx], extraRef))
              return ;
            else
              dprintf (1,"%s Inactive connection %d refused an article\n",
                       host->peerName,idx) ;
          }

      /* this'll happen if all connections are feeding and all
         their queues are full, or if those not feeding are asleep. */
      dprintf (1, "Couldn't give the article to a connection\n") ;
      
      delArticle (extraRef) ;
          
      remArticle (article,&host->processed,&host->processedTail) ;
    }

  /* either all the per connection queues were full or we already had
     a backlog, so there was no sense in checking. */
  queueArticle (article,&host->queued,&host->queuedTail) ;
    
  host->backlog++ ;
  backlogToTape (host) ;
}







/*
 * called by the Host's connection when the remote is refusing postings
 * from us becasue we're not allowed (banner code 400).
 */
void hostCxnBlocked (Host host, Connection cxn, char *reason)
{
#ifndef NDEBUG
  {
    u_int i ;
    
    for (i = 0 ; i < host->maxConnections ; i++)
      if (host->connections [i] == cxn)
        ASSERT (host->cxnActive [i] == false) ;
  }
#endif

  if (host->blockedReason == NULL)
    host->blockedReason = strdup (reason) ;
  
  if (host->activeCxns == 0 && host->spoolTime == 0)
    {
      host->blockedCxn = cxn ;  /* to limit log notices */
      syslog (LOG_NOTICE,REMOTE_BLOCKED, host->peerName, reason) ;
    }
  else if (host->activeCxns > 0 && !host->notifiedChangedRemBlckd)
    {
      syslog (LOG_NOTICE,CHANGED_REMOTE_BLOCKED, host->peerName,reason) ;
      host->notifiedChangedRemBlckd = true ;
    }
  else if (host->spoolTime != 0 && host->blockedCxn == cxn)
    {
      syslog (LOG_NOTICE,REMOTE_STILL_BLOCKED, host->peerName, reason) ;
    }
  
}







/*
 * Called by the Connection when it gets a response back to the MODE
 * STREAM command. It's now that we consider the connection usable.
 */
void hostRemoteStreams (Host host, Connection cxn, bool doesStreaming)
{
  u_int i ;

  host->blockedCxn = NULL ;
  if (host->blockedReason != NULL)
    FREE (host->blockedReason) ;
  host->blockedReason = NULL ;
  
  /* we may have told the connection to quit while it was in the middle
     of connecting */
  if (amClosing (host))
    return ;
  
  if (host->connectTime == 0)   /* first connection for this cycle. */
    {
      if (doesStreaming && host->wantStreaming)
        syslog (LOG_NOTICE, REMOTE_DOES_STREAMING, host->peerName);
      else if (doesStreaming)
        syslog (LOG_NOTICE, REMOTE_STREAMING_OFF, host->peerName);
      else
        syslog (LOG_NOTICE, REMOTE_NO_STREAMING, host->peerName);

      if (host->spoolTime > 0)
        hostStopSpooling (host) ;

      /* set up the callback for statistics logging. */
      if (host->statsId != 0)
        clearTimer (host->statsId) ;
      host->statsId = prepareSleep (hostStatsTimeoutCbk, statsPeriod, host) ;

#ifdef DYNAMIC_CONNECTIONS
      if (host->ChkCxnsId != 0)
      clearTimer (host->ChkCxnsId);
      host->ChkCxnsId = prepareSleep (hostChkCxns, 30, host) ;
#endif

      host->remoteStreams = (host->wantStreaming ? doesStreaming : false) ;

      host->connectTime = theTime() ;
      if (host->firstConnectTime == 0)
        host->firstConnectTime = host->connectTime ;
    }
  else if (host->remoteStreams != doesStreaming && host->wantStreaming)
    syslog (LOG_NOTICE,STREAMING_CHANGE,host->peerName) ;

  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->connections [i] == cxn)
      {
        host->cxnActive [i] = true ;
        if (host->cxnSleeping [i])
          host->sleepingCxns-- ;
        host->cxnSleeping [i] = false ;
        break ;
      }

  ASSERT (i != host->maxConnections) ;

  host->activeCxns++ ;

  hostLogStatus () ;
}







/*
 * Called by the connection when it is no longer connected to the
 * remote. Perhaps due to getting a code 400 to an IHAVE, or due to a
 * periodic close.
 */
void hostCxnDead (Host host, Connection cxn)
{
  u_int i ;
    
  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->connections [i] == cxn)
      {
        if (host->cxnActive [i]) /* won't be active if got 400 on banner */
          {
            host->cxnActive [i] = false ;
            host->activeCxns-- ;

            if (!amClosing (host) && host->activeCxns == 0)
              {
                clearTimer (host->statsId) ;
#ifdef DYNAMIC_CONNECTIONS
                clearTimer (host->ChkCxnsId) ;
#endif
                hostLogStats (host,true) ;
                host->connectTime = 0 ;
              }
          }
        else if (host->cxnSleeping [i]) /* cxnNuke can be called on sleepers  */
          {
            host->cxnSleeping [i] = false ;
            host->sleepingCxns-- ;
          }

        break ;
      }

  ASSERT (i < host->maxConnections) ;
  hostLogStatus () ;
}







/*
 * Called by the Connection when it is going to sleep so the Host won't
 * bother trying to give it Articles
 */
void hostCxnSleeping (Host host, Connection cxn)
{
  u_int i ;

  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->connections [i] == cxn)
      {
        if (!host->cxnSleeping [i]) 
          {
            host->cxnSleeping [i] = true ;
            host->sleepingCxns++ ;
          }

        if (host->spoolTime == 0 && host->sleepingCxns >= host->maxConnections)
          hostStartSpooling (host) ;

        break ;
      }

  ASSERT (i < host->maxConnections) ;

  hostLogStatus () ;
}







/*
 * Called by the Connection when it goes into the waiting state.
 */
void hostCxnWaiting (Host host, Connection cxn)
{
  u_int i ;

  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->connections [i] == cxn)
      {
        if (host->cxnSleeping [i])
          host->sleepingCxns-- ;
        host->cxnSleeping [i] = false ;
        break ;
      }

  ASSERT (i < host->maxConnections) ;

  if (host->spoolTime > 0)
    hostStopSpooling (host) ;

  hostLogStatus () ;
}







/*
 * Called by the Connection when it is about to delete itself.
 */
bool hostCxnGone (Host host, Connection cxn)
{
  u_int i;
  bool oneThere = false ;

  /* forget about the Connection and see if we are still holding any live
     connections still. */
  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->connections [i] == cxn)
      {
        if (!amClosing (host))
          syslog (LOG_ERR,CONNECTION_DISAPPEARING,host->peerName,i) ;
        host->connections [i] = NULL ;
        if (host->cxnActive [i])
          {
            host->cxnActive [i] = false ;
            host->activeCxns-- ;
          }
        else if (host->cxnSleeping [i])
          {
            host->cxnSleeping [i] = false ;
            host->sleepingCxns-- ;
          }
      }
    else if (host->connections [i] != NULL)
      oneThere = true ;

  /* remove the host if it has no connexions */
  if ( !oneThere )
    {
      time_t now = theTime() ;
      u_int hostsLeft ;

      if (host->firstConnectTime > 0)
        syslog (LOG_NOTICE,REALLY_FINAL_STATS,
                host->peerName,
                (long) (now - host->firstConnectTime),
                host->gArtsOffered, host->gArtsAccepted,
                host->gArtsNotWanted, host->gArtsRejected,
                host->gArtsMissing) ;

      hostsLeft = listenerHostGone (host->listener, host) ;
      delHost (host) ;

      if (hostsLeft == 0)
        syslog (LOG_NOTICE,PROCESS_FINAL_STATS,
                (long) (now - start),
                procArtsOffered, procArtsAccepted,
                procArtsNotWanted,procArtsRejected,
                procArtsMissing) ;
      
      /* return true if that was the last host */
      return (hostsLeft == 0 ? true : false) ;
    }

  /* return false because there is still at least one host (this one) */
  return false ;
}







/*
 * The connections has offered an article to the remote.
 */
void hostArticleOffered (Host host, Connection cxn) 
{
  (void) cxn ;                  /* keep lint happy. */
  
  host->artsOffered++ ;
  host->gArtsOffered++ ;
  procArtsOffered++ ;
}







/*
 * Article was succesfully transferred.
 */
void hostArticleAccepted (Host host, Connection cxn, Article article)
{
  const char *filename = artFileName (article) ;
  const char *msgid = artMsgId (article) ;

  dprintf (5,"Article %s (%s) was transferred\n", msgid, filename) ;
  
  host->artsAccepted++ ;
  host->gArtsAccepted++ ;
  procArtsAccepted++ ;

  /* host has two references to the article here... the parameter `article'
     and the queue */

  delArticle (article) ;        /* drop the parameter reference */

  if (!amClosing (host))
    articleGone (host,cxn,article) ; /* and the one in the queue */
}







/*
 * remote said no thanks to an article.
 */
void hostArticleNotWanted (Host host, Connection cxn, Article article)
{
  const char *filename = artFileName (article) ;
  const char *msgid = artMsgId (article) ;

  dprintf (5,"Article %s (%s) was not wanted\n", msgid, filename) ;
  
  host->artsNotWanted++ ;
  host->gArtsNotWanted++ ;
  procArtsNotWanted++ ;
  
  
  /* host has two references to the article here... `article' and the
     queue */

  delArticle (article) ;        /* drop the `article' reference */
  
  if (!amClosing (host)) 
    articleGone (host,cxn,article) ; /* and the one in the queue */
}







/*
 * remote rejected the article after it was was transferred
 */
void hostArticleRejected (Host host, Connection cxn, Article article) 
{
  const char *filename = artFileName (article) ;
  const char *msgid = artMsgId (article) ;

  dprintf (5,"Article %s (%s) was rejected\n", msgid, filename) ;
  
  host->artsRejected++ ;
  host->gArtsRejected++ ;
  procArtsRejected++ ;

  /* host has two references to the article here... `article' and the queue */

  delArticle (article) ;        /* drop the `article' reference */

  if (!amClosing (host))
    articleGone (host,cxn,article) ;
}







/*
 * The remote wants us to retry the article later.
 */
void hostArticleDeferred (Host host, Connection cxn, Article article) 
{
  host->artsDeferred++ ;
  host->gArtsDeferred++ ;
  procArtsDeferred++ ;


  if (!amClosing (host))
    {
      Article extraRef ;

      extraRef = artTakeRef (article) ; /* hold a reference until requeued */
      articleGone (host,cxn,article) ; /* drop from the queue */
      hostSendArticle (host, article) ; /* requeue it */
      delArticle (extraRef) ;
    }
  else
    delArticle(article); /*drop parameter reference if not sent to tape*/
}







/*
 * The Connection is giving the article back to the Host, but it doesn't
 * want a new one in return.
 */
void hostTakeBackArticle (Host host, Connection cxn, Article article) 
{
  (void) cxn ;                  /* keep lint happy */
  
  if (!amClosing (host)) 
    {
      Article extraRef ;

      host->artsCxnDrop++ ;
      host->gArtsCxnDrop++ ;
      extraRef = artTakeRef (article) ; /* hold a reference until requeued */
      articleGone (host,NULL,article) ; /* drop from the queue */
      host->notThisCxn = cxn;
      hostSendArticle (host, article) ; /* requeue it */
      host->notThisCxn = NULL;
      delArticle (extraRef) ;
    }
  else
    delArticle(article); /*drop parameter reference if not sent to tape*/

}







/*
 * The disk file for the article is no longer valid
 */
void hostArticleIsMissing (Host host, Connection cxn, Article article)
{
  const char *filename = artFileName (article) ;
  const char *msgid = artMsgId (article) ;

  dprintf (5, "%s article is missing %s %s\n", host->peerName, msgid, filename) ;
    
  host->artsMissing++ ;
  host->gArtsMissing++ ;
  procArtsMissing++ ;

  /* host has two references to the article here... `article' and the
     queue */

  delArticle (article) ;        /* drop the `article' reference */

  if (!amClosing (host))
    articleGone (host,cxn,article) ; /* and the one in the queue */
}







/* The Connection wants something to do. This is called by the Connection
 * after it has transferred an article. This is what keeps the pipes full
 * of data off the tapes if the input from inn is idle.
 */
bool hostGimmeArticle (Host host, Connection cxn)
{
  Article article = NULL ;
  bool gaveSomething = false ;
  size_t amtToGive = cxnQueueSpace (cxn) ; /* may be more than one */

  if (amClosing (host))
    {
      dprintf (5,"%s no article to give due to closing\n",host->peerName) ;

      return false ;
    }

  if (amtToGive == 0)
    dprintf (5,"%s Queue space is zero....\n",host->peerName) ;
  
  while (amtToGive > 0)
    {
      bool tookIt ;
      
      if ((article = remHead (&host->queued,&host->queuedTail)) != NULL)
        {
          host->backlog-- ;
          tookIt = cxnQueueArticle (cxn,artTakeRef (article)) ;

          ASSERT (tookIt == true) ;

          queueArticle (article,&host->processed,&host->processedTail) ;
          amtToGive-- ;

          gaveSomething = true ;
        }
      else if ((article = getArticle (host->myTape)) != NULL) 
        {                       /* go to the tapes */
          tookIt = cxnQueueArticle (cxn,artTakeRef (article)) ;

          ASSERT (tookIt == true) ;

          host->artsFromTape++ ;
          host->gArtsFromTape++ ;
          queueArticle (article,&host->processed,&host->processedTail) ;
          amtToGive-- ;

          gaveSomething = true ;
        }
      else
        {
          /* we had nothing left to give... */
          
          if (host->processed == NULL) /* and if nothing outstanding... */
            listenerHostIsIdle (host->listener,host) ; /* tell our owner */
  
          amtToGive = 0 ;
        }
    }

  return gaveSomething ;
}







/*
 * get the name that INN uses for this host
 */
const char *hostPeerName (Host host)
{
  ASSERT (host != NULL) ;
    
  return host->peerName ;
}


/* return true if the Connections for this host should attempt to do
   streaming. */
bool hostWantsStreaming (Host host)
{
  return host->wantStreaming ;
}

u_int hostMaxChecks (Host host)
{
  return host->maxChecks ;
}







/**********************************************************************/
/**                       CLASS FUNCTIONS                            **/
/**********************************************************************/

/*
 * Set the state of whether each Connection is told to log its stats when
 * its controlling Host logs its stats.
 */
void hostLogConnectionStats (bool val)
{
  logConnectionStats = val ;
}


bool hostLogConnectionStatsP (void)
{
  return logConnectionStats ;
}



/*
 * Called by one of the Host's Connection's when it (the Connection)
 * switches into or out of no-CHECK mode.
 */
void hostLogNoCheckMode (Host host, bool on)
{
  if (on && host->loggedModeOn == false)
    {
      syslog (LOG_NOTICE, STREAMING_MODE_SWITCH, host->peerName) ;
      host->loggedModeOn = true ;
    }
  else if (!on && host->loggedModeOff == false) 
    {
      syslog (LOG_NOTICE, STREAMING_MODE_UNDO, host->peerName) ;
      host->loggedModeOff = true ;
    }
}



void hostSetStatusFile (const char *filename)
{
  FILE *fp ;
  
  if (filename == NULL)
    die ("Can't set status file name with a NULL filename\n") ;
  else if (*filename == NULL)
    die ("Can't set status file name with a empty string\n") ;

  if (*filename == '/')
    statusFile = strdup (filename) ;
  else
    {
      const char *tapeDir = getTapeDirectory() ;
      
      statusFile = malloc (strlen (tapeDir) + strlen (filename) + 2) ;
      sprintf (statusFile,"%s/%s",tapeDir,filename) ;
    }

  if ((fp = fopen (statusFile,"w")) == NULL)
    {
      FREE (statusFile) ;
      syslog (LOG_ERR,"Status file is not a valie pathname: %s",
              statusFile) ;
      statusFile = NULL ;
    }
  else
    fclose (fp) ;
}



/**********************************************************************/
/**                      PRIVATE FUNCTIONS                           **/
/**********************************************************************/




#define INHERIT	1
#define NO_INHERIT 0


static void hostDetails (scope *s,
                         char *name,
                         char **iname,
                         u_int *articleTimeout,
                         u_int *responseTimeout,
                         u_int *initialConnections,
                         u_int *maxConnections,
                         u_int *maxChecks,
                         bool *streaming,
                         double *lowFilter,
                         double *highFilter,
                         u_short *portNumber)
{
  long iv ;
  int bv ;
  char *p ;
  double rv ;

  
  if (s != NULL)
    if (getString (s,IP_NAME,&p,NO_INHERIT))
      *iname = p ;
    else
      *iname = strdup (name) ;

      
  if (!getInteger (s,ARTICLE_TIMEOUT,&iv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_INT,ARTICLE_TIMEOUT,name,(long) ARTTOUT) ;
      iv = ARTTOUT ;
    }
  else if (iv < 1)
    {
      syslog (LOG_ERR,LESS_THAN_ONE,ARTICLE_TIMEOUT,iv,name,(long)ARTTOUT) ;
      iv = ARTTOUT ;
    }
  *articleTimeout = (u_int) iv ;
      

  
  if (!getInteger (s,RESP_TIMEOUT,&iv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_INT,RESP_TIMEOUT,name,(long)RESPTOUT) ;
      iv = RESPTOUT ;
    }
  else if (iv < 1)
    {
      syslog (LOG_ERR,LESS_THAN_ONE,RESP_TIMEOUT,iv,name,(long)RESPTOUT) ;
      iv = RESPTOUT ;
    }
  *responseTimeout = (u_int) iv ;

      

  if (!getInteger (s,INITIAL_CONNECTIONS,&iv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_INT,INITIAL_CONNECTIONS,name,
              (long)INIT_CXNS) ;
      iv = INIT_CXNS ;
    }
  else if (iv < 0)
    {
      syslog (LOG_ERR,LESS_THAN_ZERO,INITIAL_CONNECTIONS,iv,
              name,(long)INIT_CXNS);
      iv = INIT_CXNS ;
    }
  *initialConnections = (u_int) iv ;

      

  if (!getInteger (s,MAX_CONNECTIONS,&iv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_INT,MAX_CONNECTIONS,name,(long)MAX_CXNS) ;
      iv = MAX_CXNS ;
    }
  else if (iv < 1)
    {
      syslog (LOG_ERR,LESS_THAN_ONE,MAX_CONNECTIONS,iv,name,(long)1) ;
      iv = 1 ;
    }
  else if (iv > MAX_CONNECTION_COUNT)
    {
      syslog (LOG_ERR,INT_TOO_LARGE,MAX_CONNECTIONS,iv,name,(long)MAX_CXNS) ;
      iv = MAX_CXNS ;
    }
  *maxConnections = (u_int) iv ;

  

  if (!getInteger (s,MAX_QUEUE_SIZE,&iv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_INT,MAX_QUEUE_SIZE,name,(long)MAX_Q_SIZE) ;
      iv = MAX_Q_SIZE ;
    }
  else if (iv < 1)
    {
      syslog(LOG_ERR,LESS_THAN_ONE,MAX_QUEUE_SIZE,iv,name,(long)MAX_Q_SIZE);
      iv = MAX_Q_SIZE ;
    }
  *maxChecks = (u_int) iv ;
      


  if (!getBool (s,STREAMING,&bv,INHERIT))
    {
      syslog(LOG_ERR,NO_PEER_FIELD_BOOL,STREAMING,name,(STREAM ? "true" : "false"));
      bv = STREAM ;
    }
  *streaming = (bv ? true : false) ;
  
      

  if (!getReal (s,NO_CHECK_LOW,&rv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_REAL,NO_CHECK_LOW,name,NOCHECKLOW) ;
      rv = NOCHECKLOW ;
    }
  else if (rv < 0.0 || rv > 100.0)
    {
      syslog (LOG_ERR,
              "ME %s value (%.2f) in peer %s must be in range [0.0, 100.0]",
              NO_CHECK_LOW,rv,name) ;
      rv = NOCHECKLOW ;
    }
  *lowFilter = rv / 10.0 ;
      

  if (!getReal (s,NO_CHECK_HIGH,&rv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_REAL,NO_CHECK_HIGH,name,NOCHECKHIGH) ;
      rv = NOCHECKHIGH ;
    }
  else if (rv < 0.0 || rv > 100.0)
    {
      syslog (LOG_ERR,
              "ME %s value (%.2f) in peer %s must be in range [0.0, 100.0]",
              NO_CHECK_HIGH,rv,name) ;
      rv = NOCHECKHIGH ;
    }
  else if (rv < *lowFilter)
    {
      syslog (LOG_ERR,
              "ME %s's value (%.2f) in peer %s cannot be smaller than %s's value (%.2f)",
              NO_CHECK_HIGH, rv, name, NO_CHECK_LOW, *lowFilter);
      rv = 100.0 ;
    }
  *highFilter = rv / 10.0 ;
      

  if (!getInteger (s,PORT_NUMBER,&iv,INHERIT))
    {
      syslog (LOG_ERR,NO_PEER_FIELD_INT,PORT_NUMBER,name,(long)PORTNUM) ;
      iv = PORTNUM ;
    }
  else if (iv < 1)
    {
      syslog (LOG_ERR,"ME %s value (%ld) in peer %s cannot be less than 1",
              PORT_NUMBER,iv,name) ;
      iv = PORTNUM ;
    }
  *portNumber = (u_short) iv ;
}




static bool getHostInfo (char **name,
                         char **ipName,
                         u_int *articleTimeout,
                         u_int *responseTimeout,
                         u_int *initialConnections,
                         u_int *maxConnections,
                         u_int *maxChecks,
                         bool *streaming,
                         double *lowFilter,
                         double *highFilter,
                         u_short *portNumber)
{
  static int idx = 0 ;
  value *v ;
  scope *s ;

  bool isGood = false ;

  if (topScope == NULL)
    return false ;
  
  while ((v = getNextPeer (&idx)) != NULL) 
    {
      if (!ISPEER (v))
        continue ;

      s = v->v.scope_val ;
      *name = strdup (v->name) ;

      hostDetails(s,*name,ipName,articleTimeout,responseTimeout,
                  initialConnections,maxConnections,maxChecks,streaming,
                  lowFilter,highFilter,portNumber) ;

      isGood = true ;
      
      break ;
    }

  if (v == NULL)
    idx = 0 ;                   /* start over next time around */

  return isGood ;
}



void getHostDefaults (u_int *articleTout,
                      u_int *respTout,
                      u_int *initialCxns,
                      u_int *maxCxns,
                      u_int *maxChecks,
                      bool *streaming,
                      double *lowFilter,
                      double *highFilter,
                      u_short *portNum)
{
  ASSERT (defaultMaxConnections > 0) ;
  
  ASSERT (articleTout != NULL) ;
  ASSERT (respTout != NULL) ;
  ASSERT (initialCxns != NULL) ;
  ASSERT (maxCxns != NULL) ;
  ASSERT (maxChecks != NULL) ;
  ASSERT (portNum != NULL) ;
  ASSERT (streaming != NULL) ;
  ASSERT (lowFilter != NULL) ;
  ASSERT (highFilter != NULL) ;
  
  *articleTout = defaultArticleTimeout ;
  *respTout = defaultResponseTimeout ;
  *initialCxns = defaultInitialConnections ;
  *maxCxns = defaultMaxConnections ;
  *maxChecks = defaultMaxChecks ;
  *streaming = defaultStreaming ;
  *lowFilter = defaultLowFilter ;
  *highFilter = defaultHighFilter ;
  *portNum = defaultPortNumber ;
}


/*
 * fully delete and clean up the Host object.
 */
void delHost (Host host)
{
  Host h,q ;

  for (h = gHostList, q = NULL ; h != NULL ; q = h, h = h->next)
    if (h == host)
      {
        if (gHostList == h)
          gHostList = gHostList->next ;
        else
          q->next = h->next ;
        break ;
      }

  ASSERT (h != NULL) ;
        
  delTape (host->myTape) ;
  
  FREE (host->connections) ;
  FREE (host->cxnActive) ;
  FREE (host->cxnSleeping) ;
  FREE (host->peerName) ;
  FREE (host->ipName) ;

  if (host->ipAddrs)
    FREE (host->ipAddrs) ;

  FREE (host) ;
  gHostCount-- ;
}



static Host findHostByName (char *name) 
{
  Host h;

  for (h = gHostList; h != NULL; h = h->next)
    if ( strcmp(h->peerName, name) == 0 )
      return h;

  return NULL;
}



/* 
 * the article can be dropped from the process queue and the connection can
 * take a new article if there are any to be had.
 */
static void articleGone (Host host, Connection cxn, Article article)
{
  if ( !remArticle (article,&host->processed,&host->processedTail) )
    die ("remArticle in articleGone failed") ;

  delArticle (article) ;

  if (cxn != NULL)
    hostGimmeArticle (host,cxn) ; /* may not give anything over */
}







/*
 * One of the Connections for this Host has reestablished itself, so stop
 * spooling article info to disk.
 */
static void hostStopSpooling (Host host)
{
  ASSERT (host->spoolTime != 0) ;
  
  clearTimer (host->statsId) ;
  hostLogStats (host,true) ;
 
  host->spoolTime = 0 ;
}







/*
 * No connections are active and we're getting response 201 or 400 (or some
 * such) so that we need to start spooling article info to disk.
 */
static void hostStartSpooling (Host host)
{
  ASSERT (host->spoolTime == 0) ;

  queuesToTape (host) ;

  hostLogStats (host,true) ;
  
  host->spoolTime = theTime() ;
  if (host->firstConnectTime == 0)
    host->firstConnectTime = host->spoolTime ;

  /* don't want to log too frequently */
  if (SPOOL_LOG_PERIOD > 0 &&
      (host->spoolTime - host->lastSpoolTime) > SPOOL_LOG_PERIOD)
    {
      syslog (LOG_NOTICE,SPOOLING,host->peerName) ;
      host->lastSpoolTime = host->spoolTime ;
    }
  
  host->connectTime = 0 ;

  host->notifiedChangedRemBlckd = false ;

  clearTimer (host->statsId) ;
  host->statsId = prepareSleep (hostStatsTimeoutCbk, statsPeriod, host) ;
}







/*
 * Time to log the statistics for the Host. If FINAL is true then the
 * counters will be reset.
 */
static void hostLogStats (Host host, bool final)
{
  time_t now = theTime() ;
  time_t *startPeriod ;
  double cnt = (host->blCount) ? (host->blCount) : 1.0;

  if (host->spoolTime == 0 && host->connectTime == 0)
    return ;        /* host has never connected and never started spooling*/

  startPeriod = (host->spoolTime != 0 ? &host->spoolTime : &host->connectTime);

  if (now - *startPeriod >= statsResetPeriod)
    final = true ;
  
  if (host->spoolTime != 0)
    syslog (LOG_NOTICE, HOST_SPOOL_STATS, host->peerName,
            (final ? "final" : "checkpoint"),
            (long) (now - host->spoolTime), host->artsToTape,
            host->artsHostClose, host->artsHostSleep) ;
  else
    syslog (LOG_NOTICE, HOST_STATS_MSG, host->peerName, 
            (final ? "final" : "checkpoint"),
            (long) (now - host->connectTime),
            host->artsOffered, host->artsAccepted,
            host->artsNotWanted, host->artsRejected,
            host->artsMissing, host->artsToTape,
            host->artsHostClose, host->artsFromTape,
            host->artsDeferred, host->artsCxnDrop,
            (double)host->blAccum/cnt, hostHighwater,
            (100.0*host->blNone)/cnt,
            (100.0*host->blQuartile[0])/cnt, (100.0*host->blQuartile[1])/cnt,
            (100.0*host->blQuartile[2])/cnt, (100.0*host->blQuartile[3])/cnt,
            (100.0*host->blFull)/cnt) ;

  if (logConnectionStats) 
    {
      u_int i ;
      
      for (i = 0 ; i < host->maxConnections ; i++)
        if (host->connections [i] != NULL && host->cxnActive [i])
          cxnLogStats (host->connections [i],final) ;
    }

  /* one 'spooling backlog' message per stats logging period */
  host->loggedBacklog = false ;
  host->loggedModeOn = host->loggedModeOff = false ;

  if (final)
    {
      host->artsOffered = 0 ;
      host->artsAccepted = 0 ;
      host->artsNotWanted = 0 ;
      host->artsRejected = 0 ;
      host->artsDeferred = 0 ;
      host->artsMissing = 0 ;
      host->artsToTape = 0 ;
      host->artsQueueOverflow = 0 ;
      host->artsCxnDrop = 0 ;
      host->artsHostSleep = 0 ;
      host->artsHostClose = 0 ;
      host->artsFromTape = 0 ;
      
      *startPeriod = theTime () ; /* in of case STATS_RESET_PERIOD */
    }

    /* reset these each log period */
    host->blNone = 0 ;
    host->blFull = 0 ;
    host->blQuartile[0] = host->blQuartile[1] = host->blQuartile[2] =
                          host->blQuartile[3] = 0;
    host->blAccum = 0;
    host->blCount = 0;

#if 0
  /* XXX turn this section on to get a snapshot at each log period. */
  if (gPrintInfo != NULL)
    gPrintInfo () ;
#endif
}







/*
 * Log the status of the Hosts.
 */
extern char *versionInfo ;
static void hostLogStatus (void)
{
  FILE *fp = NULL ;
  Host h ;
  bool anyToLog = false ;
  u_int peerNum = 0, actConn = 0, slpConn = 0, maxcon = 0 ;
  static bool logged = false ;
  static bool flogged = false ;


  if (statusFile == NULL && !logged)
    {
      syslog (LOG_ERR,"No status file to write to") ;
      logged = true ;
      return ;
    }

  logged = false ;
    
  for (h = gHostList ; h != NULL ; h = h->next)
    if (h->myTape != NULL)   /* the host deletes its tape when it's closing */
      {
        anyToLog = true ;
        peerNum++ ;
        actConn += h->activeCxns ;
        slpConn += h->sleepingCxns ;
        maxcon += h->maxConnections ;
      }

  if (!anyToLog)
    return ;

  lastStatusLog = theTime() ;
  
  if ((fp = fopen (statusFile,"w")) == NULL)
    {
      if ( !flogged )
        syslog (LOG_ERR,NO_STATUS,statusFile) ;
      flogged = true ;
    }
  else
    {
      char timeString [30] ;
      time_t now ;

      flogged = false ;
      
      now = time (NULL) ;
      strcpy (timeString,ctime (&now)) ;

      if (genHtml)
        fprintf (fp,"<HTML><META HTTP-EQUIV=\"Refresh\" CONTENT=\"20;\"><PRE>\n\n") ;

      fprintf (fp,"%s\npid %d started %s\nUpdated: %s",
               versionInfo,(int) myPid,startTime,timeString) ;
      fprintf (fp,"(peers: %d active-cxns: %d sleeping-cxns: %d idle-cxns: %d)\n\n",
               peerNum, actConn, slpConn,(maxcon - (actConn + slpConn))) ;

      fprintf (fp,"Configuration file: %s\n\n",configFile) ;

      mainLogStatus (fp) ;
      listenerLogStatus (fp) ;
      
      fprintf(fp,"Default peer configuration parameters:\n") ;
      fprintf(fp,"    article timeout: %-5d     initial connections: %d\n",
	       defaultArticleTimeout, defaultInitialConnections) ;
      fprintf(fp,"   response timeout: %-5d         max connections: %d\n",
	       defaultResponseTimeout, defaultMaxConnections) ;
      fprintf(fp,"         max checks: %-5d               streaming: %s\n",
	       defaultMaxChecks, (defaultStreaming ? "true" : "false")) ;
      fprintf(fp,"       on threshold: %-2.1f%%                port num: %d\n",
	       (defaultHighFilter * 10), defaultPortNumber) ;
      fprintf(fp,"      off threshold: %-2.1f%%\n\n",
	       (defaultLowFilter * 10)) ;

      tapeLogGlobalStatus (fp) ;

      fprintf (fp,"\n") ;
      fprintf (fp,"global (process)\n") ;
      
      fprintf (fp, "   seconds: %-7ld\n", (long) (now - start)) ;
      fprintf (fp, "   offered: %-7ld\n", procArtsOffered) ;
      fprintf (fp, "  accepted: %-7ld\n", procArtsAccepted) ;
      fprintf (fp, "   refused: %-7ld\n", procArtsNotWanted) ;
      fprintf (fp, "  rejected: %-7ld\n", procArtsRejected) ;
      fprintf (fp, "   missing: %-7ld\n", procArtsMissing) ;
      fprintf (fp, "  deferred: %-7ld\n", procArtsDeferred) ;
      fprintf (fp, "\n");
      
      for (h = gHostList ; h != NULL ; h = h->next)
        hostPrintStatus (h,fp) ;

      if (genHtml) 
        fprintf (fp,"</PRE></HTML>\n") ;
      
      fclose (fp) ;
    }
}


/*
 * This prints status information for each host.  An example of the
 * format of the output is:
 *
 * sitename
 *   seconds: 351       art. timeout: 400          ip name: foo.bar
 *   offered: 1194     resp. timeout: 240             port: 119
 *  accepted: 178     want streaming: yes      active cxns: 6
 *   refused: 948       is streaming: yes    sleeping cxns: 0
 *  rejected: 31          max checks: 25      initial cxns: 5
 *   missing: 0         on threshold: 95.0%      idle cxns: 4
 *  deferred: 0        off threshold: 95.0%       max cxns: 10
 *  requeued: 0                               queue length: 0
 *   spooled: 0                                      empty: 100.0%
 *[overflow]: 0                                    >0%-25%: 0.0%
 *[on_close]: 0                                    25%-50%: 0.0%
 *[sleeping]: 0                                    50%-75%: 0.0%
 * unspooled: 0                                  75%-<100%: 0.0%
 *                                                    full: 0.0%
 *                 backlog low limit: 1000000
 *                backlog high limit: 2000000     (factor 2.0)
 *                 backlog shrinkage: 0 bytes (from current file)
 *
 */
static void hostPrintStatus (Host host, FILE *fp)
{
  time_t now = theTime() ;
  double cnt = (host->blCount) ? (host->blCount) : 1.0;

  ASSERT (host != NULL) ;
  ASSERT (fp != NULL) ;

  fprintf (fp,"%s",host->peerName);

  if (host->blockedReason != NULL)
    fprintf (fp,"  (remote status: ``%s'')",host->blockedReason) ;

  fputc ('\n',fp) ;

  fprintf (fp, "   seconds: %-7ld   art. timeout: %-5d        ip name: %s\n",
	   host->firstConnectTime > 0 ? (long)(now - host->firstConnectTime) : 0,
	   host->articleTimeout, host->ipName) ;
           
  fprintf (fp, "   offered: %-7ld  resp. timeout: %-5d           port: %d\n",
	   (long) host->gArtsOffered, host->responseTimeout, host->port);

  fprintf (fp, "  accepted: %-7ld want streaming: %s      active cxns: %d\n",
	   (long) host->gArtsAccepted, 
           (host->wantStreaming ? "yes" : "no "),
	   host->activeCxns) ;

  fprintf (fp, "   refused: %-7ld   is streaming: %s    sleeping cxns: %d\n",
	   (long) host->gArtsNotWanted,
           (host->remoteStreams ? "yes" : "no "),
	   host->sleepingCxns) ;

  fprintf (fp, "  rejected: %-7ld     max checks: %-5d   initial cxns: %d\n",
	   (long) host->gArtsRejected, host->maxChecks,
	   host->initialCxns) ;

  fprintf (fp, "   missing: %-7ld   on threshold: %-3.1f%%      idle cxns: %d\n",
	   (long) host->gArtsMissing, (host->lowPassHigh * 10),
           host->maxConnections - (host->activeCxns + host->sleepingCxns)) ;

  fprintf (fp, "  deferred: %-7ld  off threshold: %-3.1f%%       max cxns: %d\n",
	   (long) host->gArtsDeferred, (host->lowPassLow * 10),
	   host->maxConnections) ;

  fprintf (fp, "  requeued: %-7ld                         queue length: %-3.1f\n",
	   (long) host->gArtsCxnDrop, (double)host->blAccum / cnt) ;

  fprintf (fp, "   spooled: %-7ld                                empty: %-3.1f%%\n",
	   (long) host->gArtsToTape, 100.0 * host->blNone / cnt) ;

  fprintf (fp, "[overflow]: %-7ld                              >0%%-25%%: %-3.1f%%\n",
	   (long) host->gArtsQueueOverflow, 100.0 * host->blQuartile[0] / cnt) ;

  fprintf (fp, "[on_close]: %-7ld                              25%%-50%%: %-3.1f%%\n",
	   (long) host->gArtsHostClose, 100.0 * host->blQuartile[1] / cnt) ;

  fprintf (fp, "[sleeping]: %-7ld                              50%%-75%%: %-3.1f%%\n",
	   (long) host->gArtsHostSleep, 100.0 * host->blQuartile[2] / cnt) ;

  fprintf (fp, " unspooled: %-7ld                            75%%-<100%%: %-3.1f%%\n",
	   (long) host->gArtsFromTape, 100.0 * host->blQuartile[3] / cnt) ;

  fprintf (fp, "                                                    full: %-3.1f%%\n",
	   100.0 * host->blFull / cnt) ;

  tapeLogStatus (host->myTape,fp) ;
  
  fprintf (fp, "\n\n");
}







/*
 * The callback function for the statistics timer to call.
 */
static void hostStatsTimeoutCbk (TimeoutId tid, void *data)
{
  Host host = (Host) data ;
  time_t now = theTime () ;

  (void) tid ;                  /* keep lint happy */

  ASSERT (tid == host->statsId) ;
  
  if (!amClosing (host))
    hostLogStats (host, false) ;

  if (now - lastStatusLog >= statsPeriod)
    hostLogStatus () ;
  
  
  host->statsId = prepareSleep (hostStatsTimeoutCbk, statsPeriod, host) ;
}


/* if the host has too many unprocessed articles so we send some to the tape. */
static void backlogToTape (Host host)
{
  Article article ;

  while (host->backlog > hostHighwater)
    {
      if (!host->loggedBacklog)
	{
#if 0               /* this message is pretty useless and confuses people */
	  syslog (LOG_NOTICE,BACKLOG_TO_TAPE,host->peerName /* ,host->backlog */) ;
#endif
	  host->loggedBacklog = true ;
	}
  
      article = remHead (&host->queued,&host->queuedTail) ;

      ASSERT(article != NULL);

      host->artsQueueOverflow++ ;
      host->gArtsQueueOverflow++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      host->backlog--;
      tapeTakeArticle (host->myTape,article) ;
    }
}







/*
 * Returns true of the Host is in the middle of closing down.
 */
static bool amClosing (Host host)
{
  return (host->myTape == NULL ? true : false) ;
}







/*
 * flush all queued articles all the way out to disk.
 */
static void queuesToTape (Host host)
{
  Article art ;
  
  while ((art = remHead (&host->processed,&host->processedTail)) != NULL)
    {
      host->artsHostClose++ ;
      host->gArtsHostClose++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      tapeTakeArticle (host->myTape,art) ;
    }
  
  while ((art = remHead (&host->queued,&host->queuedTail)) != NULL)
    {
      host->backlog-- ;
      host->artsHostClose++ ;
      host->gArtsHostClose++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      tapeTakeArticle (host->myTape,art) ;
    }
}







#define QUEUE_ELEM_POOL_SIZE ((4096 - 2 * (sizeof (void *))) / (sizeof (struct proc_q_elem)))

static ProcQElem queueElemPool ;

/*
 * Add an article to the given queue.
 */
static void queueArticle (Article article, ProcQElem *head, ProcQElem *tail)
{
  ProcQElem elem ;

  if (queueElemPool == NULL)
    {
      int i ;

      queueElemPool = ALLOC (struct proc_q_elem, QUEUE_ELEM_POOL_SIZE) ;
      ASSERT (queueElemPool != NULL) ;

      for (i = 0; i < QUEUE_ELEM_POOL_SIZE - 1; i++)
        queueElemPool[i] . next = &(queueElemPool [i + 1]) ;
      queueElemPool [QUEUE_ELEM_POOL_SIZE-1] . next = NULL ;
    }

  elem = queueElemPool ;
  ASSERT (elem != NULL) ;
  queueElemPool = queueElemPool->next ;

  elem->article = article ;
  elem->next = NULL ;
  elem->prev = *tail ;
  if (*tail != NULL)
    (*tail)->next = elem ;
  else
    *head = elem ;
  *tail = elem ;  
}







/*
 * remove the article from the queue
 */
static bool remArticle (Article article, ProcQElem *head, ProcQElem *tail)
{
  ProcQElem elem ;

  ASSERT (head != NULL) ;
  ASSERT (tail != NULL) ;

  /* we go backwards down the list--probably faster */
  elem = *tail ;
  while (elem != NULL && elem->article != article)
    elem = elem->prev ;

  if (elem != NULL)
    {
      if (elem->prev != NULL)
        elem->prev->next = elem->next ;
      if (elem->next != NULL)
        elem->next->prev = elem->prev ;
      if (*head == elem)
        *head = elem->next ;
      if (*tail == elem)
        *tail = elem->prev ;

      elem->next = queueElemPool ;
      queueElemPool = elem ;
      
      return true ;
    }
  else
    return false ;
}







/*
 * remove the article that's at the head of the queue and return
 * it. Returns NULL if the queue is empty.
 */
static Article remHead (ProcQElem *head, ProcQElem *tail)
{
  ProcQElem elem ;
  Article art ;

  ASSERT (head != NULL) ;
  ASSERT (tail != NULL) ;
  ASSERT ((*head == NULL && *tail == NULL) ||
          (*head != NULL && *tail != NULL)) ;

  if (*head == NULL)
    return NULL ;

  elem = *head ;
  art = elem->article ;
  *head = elem->next ;
  if (elem->next != NULL)
    elem->next->prev = NULL ;

  if (*tail == elem)
    *tail = NULL ;

  elem->next = queueElemPool ;
  queueElemPool = elem ;

  return art ;
}



static int validateInteger (FILE *fp, const char *name,
                     long low, long high, int required, long setval)
{
  int rval = VALUE_OK ;
  value *v ;
  scope *s ;
  char *p = strrchr (name,':') ;
  
  v = findValue (topScope,name,NO_INHERIT) ;
  if (v == NULL && required != NOTREQNOADD)
    {
      s = findScope (topScope,name,0) ;
      addInteger (s,p ? p + 1 : name,setval) ;
      if (required == REQ)
        {
          rval = VALUE_MISSING ;
          logOrPrint (LOG_ERR,fp,NODEFN,name) ;
        }
      else if (required)
        logOrPrint (LOG_INFO,fp,ADDMISSINGINT,name,setval) ;
    }
  else if (v != NULL && v->type != intval)
    {
      rval = VALUE_WRONG_TYPE ;
      logOrPrint (LOG_ERR,fp,NOTINT,name) ;
    }
  else if (v != NULL && low != LONG_MIN && v->v.int_val < low)
    {
      rval = VALUE_TOO_LOW ;
      logOrPrint (LOG_ERR,fp,INT_TO_LOW,name,v->v.int_val,
                  "global scope",low,low) ;
      v->v.int_val = low ;
    }
  else if (v != NULL && high != LONG_MAX && v->v.int_val > high)
    {
      rval = VALUE_TOO_HIGH ;
      logOrPrint(LOG_ERR,fp,INT_TO_HIGH,name,v->v.int_val,
                 "global scope",high,high);
      v->v.int_val = high ;
    }
  
  return rval ;
}



static int validateReal (FILE *fp, const char *name, double low,
                         double high, int required, double setval)
{
  int rval = VALUE_OK ;
  value *v ;
  scope *s ;
  char *p = strrchr (name,':') ;
  
  v = findValue (topScope,name,NO_INHERIT) ;
  if (v == NULL && required != NOTREQNOADD)
    {
      s = findScope (topScope,name,0) ;
      addReal (s,p ? p + 1 : name,setval) ;
      if (required == REQ)
        {
          rval = VALUE_MISSING ;
          logOrPrint (LOG_ERR,fp,NODEFN,name) ;
        }
      else
        logOrPrint (LOG_INFO,fp,ADDMISSINGREAL,name,setval) ;
    }
  else if (v != NULL && v->type != realval)
    {
      rval = VALUE_WRONG_TYPE ;
      logOrPrint (LOG_ERR,fp,NOTREAL,name) ;
    }
  else if (v != NULL && low != -DBL_MAX && v->v.real_val < low)
    {
      logOrPrint (LOG_ERR,fp,REALTOLOW,name,v->v.real_val,low) ; /* bleh */
      v->v.real_val = setval ;
    }
  else if (high != DBL_MAX && v->v.real_val > high)
    {
      logOrPrint (LOG_ERR,fp,REALTOHIGH,name,v->v.real_val,high) ;
      v->v.real_val = setval ;
    }
    
  return rval ;
}



static int validateBool (FILE *fp, const char *name, int required, bool setval)
{
  int rval = VALUE_OK ;
  value *v ;
  scope *s ;
  char *p = strrchr (name,':') ;
  
  v = findValue (topScope,name,NO_INHERIT) ;
  if (v == NULL && required != NOTREQNOADD)
    {
      s = findScope (topScope,name,0) ;
      addBoolean (s,p ? p + 1 : name, setval ? 1 : 0)  ;
      if (required == REQ)
        {
          rval = VALUE_MISSING ;
          logOrPrint (LOG_ERR,fp,NODEFN,name) ;
        }
      else
        logOrPrint (LOG_INFO,fp,ADDMISSINGBOOL,name,(setval?"true":"false")) ;
    }
  else if (v != NULL && v->type != boolval)
    {
      rval = VALUE_WRONG_TYPE ;
      logOrPrint (LOG_ERR,fp,NOTBOOLEAN,name) ;
    }
  
  return rval ;
}


void gCalcHostBlStat (void)
{
  Host h ;
  
  for (h = gHostList ; h != NULL ; h = h->next)
    {
      h->blAccum += h->backlog ;
      if (h->backlog == 0)
	   h->blNone++ ;
      else if (h->backlog >= hostHighwater)
	   h->blFull++ ;
      else
	   h->blQuartile[(4*h->backlog) / hostHighwater]++ ;
      h->blCount++ ;
    }
}
static void hostCleanup (void)
{
  if (statusFile != NULL)
    FREE (statusFile) ;
}

