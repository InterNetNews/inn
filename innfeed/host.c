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


#include "innfeed.h"
#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <math.h>
#include <netdb.h>
#include <syslog.h>
#include <sys/param.h>

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#include "inn/messages.h"
#include "libinn.h"

#include "article.h"
#include "buffer.h"
#include "configfile.h"
#include "connection.h"
#include "endpoint.h"
#include "host.h"
#include "innlistener.h"
#include "msgs.h"
#include "tape.h"

#define REQ 1
#define NOTREQ 0
#define NOTREQNOADD 2

#define VALUE_OK 0
#define VALUE_TOO_HIGH 1
#define VALUE_TOO_LOW 2
#define VALUE_MISSING 3
#define VALUE_WRONG_TYPE 4

#define METHOD_STATIC 0
#define METHOD_APS 1
#define METHOD_QUEUE 2
#define METHOD_COMBINED 3

/* the limit of number of connections open when a host is
   set to 0 to mean "infinite" */
#define MAXCON 500
#define MAXCONLIMIT(xx) ((xx==0)?MAXCON:xx)

#define BACKLOGFILTER 0.7
#define BACKLOGLWM 20.0
#define BACKLOGHWM 50.0

/* time between retrying blocked hosts in seconds */
#define TRYBLOCKEDHOSTPERIOD 120

extern char *configFile ;
#if defined(hpux) || defined(__hpux) || defined(_SCO_DS)
extern int h_errno;
#endif

/* the host keeps a couple lists of these */
typedef struct proc_q_elem 
{
    Article article ;
    struct proc_q_elem *next ;
    struct proc_q_elem *prev ;
    time_t whenToRequeue ;
} *ProcQElem ;

typedef struct host_param_s
{
  char *peerName;
  char *ipName;
  u_int articleTimeout;
  u_int responseTimeout;
  u_int initialConnections;
  u_int absMaxConnections;
  u_int maxChecks;
  u_short portNum;
  u_int closePeriod;
  u_int dynamicMethod;
  bool wantStreaming;
  bool dropDeferred;
  bool minQueueCxn;
  double lowPassLow; /* as percentages */
  double lowPassHigh;
  double lowPassFilter;
  u_int backlogLimit ;
  u_int backlogLimitHigh ;
  double backlogFactor ;
  double dynBacklogFilter ;
  double dynBacklogLowWaterMark ;
  double dynBacklogHighWaterMark ;
} *HostParams ;

struct host_s 
{
    InnListener listener ;      /* who created me. */
    struct sockaddr **ipAddrs ;	/* the ip addresses of the remote */
    int nextIpAddr ;		/* the next ip address to hand out */

    Connection *connections ;   /* NULL-terminated list of all connections */
    bool *cxnActive ;           /* true if the corresponding cxn is active */
    bool *cxnSleeping ;         /* true if the connection is sleeping */
    u_int maxConnections;       /* maximum no of cxns controlled by method */
    u_int activeCxns ;          /* number of connections currently active */
    u_int sleepingCxns ;        /* number of connections currently sleeping */
    Connection blockedCxn ;     /* the first connection to get the 400 banner*/
    Connection notThisCxn ;	/* don't offer articles to this connection */

    HostParams params;          /* Parameters from config file */

    bool remoteStreams ;        /* true if remote supports streaming */
    
    ProcQElem queued ;          /* articles done nothing with yet. */
    ProcQElem queuedTail ;

    ProcQElem processed ;       /* articles given to a Connection */
    ProcQElem processedTail ;

    ProcQElem deferred ;	/* articles which have been deferred by */
    ProcQElem deferredTail ;	/* a connection */
    
    TimeoutId statsId ;         /* timeout id for stats logging. */
    TimeoutId ChkCxnsId ;	/* timeout id for dynamic connections */
    TimeoutId deferredId ;	/* timeout id for deferred articles */

    Tape myTape ;
    
    bool backedUp ;             /* set to true when all cxns are full */
    u_int backlog ;             /* number of arts in `queued' queue */
    u_int deferLen ;		/* number of arts in `deferred' queue */

    bool loggedModeOn ;         /* true if we logged going into no-CHECK mode */
    bool loggedModeOff ;        /* true if we logged going out of no-CHECK mode */

    bool loggedBacklog ;        /* true if we already logged the fact */
    bool notifiedChangedRemBlckd ; /* true if we logged a new response 400 */
    bool removeOnReload ;	/* true if host should be removed at end of
				 * config reload
				 */
    bool isDynamic;             /* true if host created dynamically */

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
    double artsSizeAccepted ;	/* size of articles succesfully transferred */
    double artsSizeRejected ;	/* size of articles remote rejected */

    /* Dynamic Peerage - MGF */
    u_int artsProcLastPeriod ;  /* # of articles processed in last period */
    u_int secsInLastPeriod ;    /* Number of seconds in last period */
    u_int lastCheckPoint ;      /* total articles at end of last period */
    u_int lastSentCheckPoint ;  /* total articles sent end of last period */
    u_int lastTotalCheckPoint ; /* total articles total end of last period */
    bool maxCxnChk ;            /* check for maxConnections */
    time_t lastMaxCxnTime ;     /* last time a maxConnections increased */
    time_t lastChkTime;         /* last time a check was made for maxConnect */
    u_int nextCxnTimeChk ;      /* next check for maxConnect */

    double backlogFilter;        /* IIR filter for size of backlog */

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
    double gArtsSizeAccepted ;
    double gArtsSizeRejected ;
    u_int gCxnQueue ;
    u_int gNoQueue ;

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

    u_long dlAccum ;		/* cumulative deferLen */
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
  HostParams params;
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
double procArtsSizeAccepted ;
double procArtsSizeRejected ;
long procArtsToTape ;
long procArtsFromTape ;

static HostParams defaultParams=NULL;

static HostHolder blockedHosts ; /* lists of hosts we can't lock */
static TimeoutId tryBlockedHostsId = 0 ;
static time_t lastStatusLog ;

  /*
   * Host object private methods.
   */
static void articleGone (Host host, Connection cxn, Article article) ;
static void hostStopSpooling (Host host) ;
static void hostStartSpooling (Host host) ;
static void hostLogStats (Host host, bool final) ;
static void hostStatsTimeoutCbk (TimeoutId tid, void *data) ;
static void hostDeferredArtCbk (TimeoutId tid, void *data) ;
static void backlogToTape (Host host) ;
static void queuesToTape (Host host) ;
static bool amClosing (Host host) ;
static void hostLogStatus (void) ;
static void hostPrintStatus (Host host, FILE *fp) ;
static int validateBool (FILE *fp, const char *name,
                         int required, bool setval,
			 scope * sc, u_int inh);
static int validateReal (FILE *fp, const char *name, double low,
			 double high, int required, double setval,
			 scope * sc, u_int inh);
static int validateInteger (FILE *fp, const char *name,
			    long low, long high, int required, long setval,
			    scope * sc, u_int inh);

static HostParams newHostParams(HostParams p);
static void freeHostParams(HostParams params);

static HostHolder FindBlockedHost(const char *name);
static void addBlockedHost(HostParams params);
static void tryBlockedHosts(TimeoutId tid, void *data);
static Host newHost (InnListener listener, HostParams p);

static HostParams getHostInfo (void);
static HostParams hostDetails (scope *s,
			       char *name,
			       bool isDefault,
			       FILE *fp);

static Host findHostByName (char *name) ;
static void hostCleanup (void) ;
static void hostAlterMaxConnections(Host host,
				    u_int absMaxCxns, u_int maxCxns,
				    bool makeConnect);

/* article queue management functions */
static Article remHead (ProcQElem *head, ProcQElem *tail) ;
static void queueArticle (Article article, ProcQElem *head, ProcQElem *tail,
			  time_t when) ;
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

bool genHtml = false ;

/*******************************************************************/
/*                  PUBLIC FUNCTIONS                               */
/*******************************************************************/


/* function called when the config file is loaded */
int hostConfigLoadCbk (void *data)
{
  int rval = 1, bval ;
  long iv ;
  FILE *fp = (FILE *) data ;
  char *p ;


  d_printf(1,"hostConfigLoadCbk\n");

  if (defaultParams)
    {
      freeHostParams(defaultParams);
      defaultParams=NULL;
    }
   
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
  
  defaultParams=hostDetails(topScope, NULL, true, fp);
  ASSERT(defaultParams!=NULL);

  return rval ;
}

/*
 * make a new HostParams structure copying an existing one
 * or from compiled defaults
 */

HostParams newHostParams(HostParams p)
{
  HostParams params;

  params = ALLOC (struct host_param_s,1) ;
  ASSERT (params != NULL);

  if (p != NULL)
    {
      /* Copy old stuff in */
      memcpy ((char *) params, (char *) p, sizeof(struct host_param_s));
      if (params->peerName)
	params->peerName = xstrdup(params->peerName);
      if (params->ipName)
	params->ipName = xstrdup(params->ipName);
    }
  else
    {
      /* Fill in defaults */
      params->peerName=NULL;
      params->ipName=NULL;
      params->articleTimeout=ARTTOUT;
      params->responseTimeout=RESPTOUT;
      params->initialConnections=INIT_CXNS;
      params->absMaxConnections=MAX_CXNS;
      params->maxChecks=MAX_Q_SIZE;
      params->portNum=PORTNUM;
      params->closePeriod=CLOSE_PERIOD;
      params->dynamicMethod=METHOD_STATIC;
      params->wantStreaming=STREAM;
      params->dropDeferred=FALSE;
      params->minQueueCxn=FALSE;
      params->lowPassLow=NOCHECKLOW;
      params->lowPassHigh=NOCHECKHIGH;
      params->lowPassFilter=FILTERVALUE;
      params->backlogLimit=BLOGLIMIT;
      params->backlogLimitHigh=BLOGLIMIT_HIGH ;
      params->backlogFactor=LIMIT_FUDGE ;
      params->dynBacklogFilter = BACKLOGFILTER ;
      params->dynBacklogLowWaterMark = BACKLOGLWM;
      params->dynBacklogHighWaterMark = BACKLOGHWM;
    }
  return (params);
}
            
/*
 * Free up a param structure
 */

void freeHostParams(HostParams params)
{
  ASSERT(params != NULL);
  if (params->peerName)
    FREE (params->peerName) ;
  if (params->ipName)
    FREE (params->ipName) ;
  FREE (params) ;
}  

static void hostReconfigure(Host h, HostParams params)
{
  u_int i, absMaxCxns ;
  double oldBacklogFilter ;
  
  if (strcmp(h->params->ipName, params->ipName) != 0)
    {
      FREE (h->params->ipName) ;
      h->params->ipName = xstrdup (params->ipName) ;
      h->nextIpLookup = theTime () ;
    }
  
  /* Put in new parameters
     Unfortunately we can't blat on top of absMaxConnections
     as we need to do some resizing here
     */
  
  ASSERT (h->params != NULL);
  
  oldBacklogFilter = h->params->dynBacklogFilter;
  i = h->params->absMaxConnections; /* keep old value */
  absMaxCxns = params->absMaxConnections;
  /* Use this set of params and allocate, and free
   * up the old
   */
  freeHostParams(h->params);
  h->params = params;
  h->params->absMaxConnections = i; /* restore old value */
  
  /* If the backlog filter value has changed, reset the
   * filter as the value therein will be screwy
   */
  if (h->params->dynBacklogFilter != oldBacklogFilter)
    h->backlogFilter = ((h->params->dynBacklogLowWaterMark
			 + h->params->dynBacklogHighWaterMark)
			/200.0 /(1.0-h->params->dynBacklogFilter));
  
  /* We call this anyway - it does nothing if the values
   * haven't changed. This is because doing things like
   * just changing "dynamic-method" requires this call
   * to be made
   */
  hostAlterMaxConnections(h, absMaxCxns, h->maxConnections, false);
  
  for ( i = 0 ; i < MAXCONLIMIT(h->params->absMaxConnections) ; i++ )
    if (h->connections[i] != NULL)
      cxnSetCheckThresholds (h->connections[i],
			     h->params->lowPassLow,
			     h->params->lowPassHigh,
			     h->params->lowPassFilter) ;
  
  /* XXX how to handle initCxns change? */
}


void configHosts (bool talkSelf)
{
  Host nHost, h, q ;
  HostHolder hh, hi ;
  HostParams params;

  /* Remove the current blocked host list */
  for (hh = blockedHosts, hi = NULL ; hh != NULL ; hh = hi)
    {
      freeHostParams(hh->params);
      hi = hh->next ;
      FREE (hh) ;
    }
  blockedHosts = NULL ;

  closeDroppedArticleFile () ;
  openDroppedArticleFile () ;
  
  while ((params = getHostInfo ()) !=NULL )
    {
      h = findHostByName (params->peerName) ;
      /* We know the host isn't blocked as we cleared the blocked list */
      /* Have we already got this host up and running ?*/
      if ( h != NULL )
	{
	  hostReconfigure(h, params);
	  h->removeOnReload = false ; /* Don't remove at the end */
	}
      else
        {
	    
	  /* It's a host we haven't seen from the config file before */
	  nHost = newHost (mainListener, params);

	  if (nHost == NULL)
	    {
	      addBlockedHost(params);
               
	      syslog (LOG_ERR,NO_HOST,params->peerName) ;
	    }
	  else 
	    {
	      if (params->initialConnections == 0 && talkSelf)
		syslog (LOG_NOTICE,BATCH_AND_NO_CXNS,params->peerName) ;

	      if ( !listenerAddPeer (mainListener,nHost) )
		die ("failed to add a new peer\n") ;
	    }
	}

    }
  

  for (h = gHostList; h != NULL; h = q) 
    {
      q = h->next ;
      if (h->removeOnReload)
	{
	  if (h->isDynamic)
	    {
	      /* change to the new default parameters */
	      params = newHostParams(defaultParams);
	      ASSERT(params->peerName == NULL);
	      ASSERT(params->ipName == NULL);
	      ASSERT(h->params->peerName != NULL);
	      ASSERT(h->params->ipName != NULL);
	      params->peerName = xstrdup(h->params->peerName);
	      params->ipName = xstrdup(h->params->ipName);
	      hostReconfigure(h, params);
	      h->removeOnReload = true;
	    }
	  else
	    hostClose (h) ;         /* h may be deleted in here. */
	}
      else
        /* prime it for the next config file read */
        h->removeOnReload = true ;
    }

  hostLogStatus () ;
}


void hostAlterMaxConnections(Host host,
			     u_int absMaxCxns, u_int maxCxns,
			     bool makeConnect)
{
  u_int lAbsMaxCxns;
  u_int i;
  
  /* Fix 0 unlimited case */
  lAbsMaxCxns = MAXCONLIMIT(absMaxCxns);
  
  /* Don't accept 0 for maxCxns */
  maxCxns=MAXCONLIMIT(maxCxns);
  
  if ( host->params->dynamicMethod == METHOD_STATIC)
    {
      /* If running static, ignore the maxCxns passed in, we'll
	 just use absMaxCxns
	 */
      maxCxns = lAbsMaxCxns;
    }
  
  if ( maxCxns > lAbsMaxCxns)
    {
      /* ensure maxCxns is of the correct form */
      maxCxns = lAbsMaxCxns;
    }

  if ((maxCxns < host->maxConnections) && (host->connections != NULL))
    {
      /* We are going to have to nuke some connections, as the current
         max is now greater than the new max
	 */
      for ( i = host->maxConnections ; i > maxCxns ; i-- )
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
	  if (host->connections[i - 1] != NULL)
	    {
	      cxnLogStats (host->connections [i-1], true) ;
	      cxnNuke (host->connections[i-1]) ;
	      host->connections[i-1] = NULL;
	    }
	}
      host->maxConnections = maxCxns ;
    }
  
  if (host->connections)
    for (i = host->maxConnections ; i <= MAXCONLIMIT(host->params->absMaxConnections) ; i++)
      {
	/* Ensure we've got an empty values only beyond the maxConnection
	   water mark.
	   */
	ASSERT (host->connections[i] == NULL);
      }
  
  if ((lAbsMaxCxns != MAXCONLIMIT(host->params->absMaxConnections)) ||
      (host->connections == NULL))
    {
      /* we need to change the size of the connection array */
      if (host->connections == NULL)
	{
	  /* not yet allocated */
	  
	  host->connections = CALLOC (Connection, lAbsMaxCxns + 1) ;
	  ASSERT (host->connections != NULL) ;
	  
	  ASSERT (host->cxnActive == NULL);
	  host->cxnActive = CALLOC (bool, lAbsMaxCxns) ;
	  ASSERT (host->cxnActive != NULL) ;
	  
	  ASSERT (host->cxnSleeping == NULL) ;
	  host->cxnSleeping = CALLOC (bool, lAbsMaxCxns) ;
	  ASSERT (host->cxnSleeping != NULL) ;
	  
	  for (i = 0 ; i < lAbsMaxCxns ; i++)
	    {
	      host->connections [i] = NULL ;
	      host->cxnActive[i] = false ;
	      host->cxnSleeping[i] = false ;
	    }
	  host->connections[lAbsMaxCxns] = NULL;
	}
      else
	{
	  host->connections =
	    REALLOC (host->connections, Connection, lAbsMaxCxns + 1);
	  ASSERT (host->connections != NULL) ;
	  host->cxnActive = REALLOC (host->cxnActive, bool, lAbsMaxCxns) ;
	  ASSERT (host->cxnActive != NULL) ;
	  host->cxnSleeping = REALLOC (host->cxnSleeping, bool, lAbsMaxCxns) ;
	  ASSERT (host->cxnSleeping != NULL) ;

	  if (lAbsMaxCxns > MAXCONLIMIT(host->params->absMaxConnections))
	    {
	      for (i = MAXCONLIMIT(host->params->absMaxConnections) ;
		   i < lAbsMaxCxns ; i++)
		{
		  host->connections[i+1] = NULL; /* array always 1 larger */
		  host->cxnActive[i] = false ;
		  host->cxnSleeping[i] = false ;
		}
	    }
	}
      host->params->absMaxConnections = absMaxCxns;
    }    
  /* if maximum was raised, establish the new connexions
     (but don't start using them).
     */
  if ( maxCxns > host->maxConnections)
    {
      i = host->maxConnections ;
      /* need to set host->maxConnections before cxnWait() */
      host->maxConnections = maxCxns;

      while ( i < maxCxns )
	{
	  host->cxnActive [i] = false ;
	  host->cxnSleeping [i] = false ;
	  /* create a new connection */
	  host->connections [i] =
	    newConnection (host, i,
			   host->params->ipName,
			   host->params->articleTimeout,
			   host->params->portNum,
			   host->params->responseTimeout,
			   host->params->closePeriod,
			   host->params->lowPassLow,
			   host->params->lowPassHigh,
			   host->params->lowPassFilter) ;

	  /* connect if low enough numbered, or we were forced to */
	  if ((i < host->params->initialConnections) || makeConnect)
	    cxnConnect (host->connections [i]) ;
	  else
	    cxnWait (host->connections [i]) ;
	  i++ ;
	}
    }

}

/*
 * Find a host on the blocked host list
 */

static HostHolder FindBlockedHost(const char *name)
{
  HostHolder hh = blockedHosts;
  while (hh != NULL)
    if ((hh->params) && (hh->params->peerName) &&
	(strcmp(name,hh->params->peerName) == 0))
      return hh;
    else
      hh=hh->next;
  return NULL;
}

static void addBlockedHost(HostParams params)
{
  HostHolder hh;

  hh = ALLOC (struct host_holder_s,1) ;
  /* Use this set of params */
	  
  hh->params = params;
  
  hh->next = blockedHosts ;
  blockedHosts = hh ;
}

/*
 * We iterate through the blocked host list and try and reconnect ones
 * where we couldn't get a lock
 */
static void tryBlockedHosts(TimeoutId tid UNUSED , void *data UNUSED )
{
  HostHolder hh,hi;
  HostParams params;
  
  hh = blockedHosts; /* Get start of our queue */
  blockedHosts = NULL ; /* remove them all from the queue of hosts */

  while (hh != NULL)
    {
      params = hh->params;
      hi= hh->next;
      FREE(hh);
      hh = hi;

      if (params && params->peerName)
	{
	  if (findHostByName(params->peerName)!=NULL)
	    {
	      /* Wierd, someone's managed to start it when it's on
	       * the blocked list. Just silently discard.
	       */
	      freeHostParams(params);
	    }
	  else
	    {
	      Host nHost;
	      nHost = newHost (mainListener, params);

	      if (nHost == NULL)
		{
		  addBlockedHost(params);
               
		  syslog (LOG_ERR,NO_HOST,params->peerName) ;
		}
	      else 
		{
		  d_printf(1,"Unblocked host %s\n",params->peerName);

		  if (params->initialConnections == 0 &&
		      listenerIsDummy(mainListener) /*talk to self*/)
		    syslog (LOG_NOTICE,BATCH_AND_NO_CXNS,params->peerName) ;

		  if ( !listenerAddPeer (mainListener,nHost) )
		    die ("failed to add a new peer\n") ;
		}
	    }
	}
    }
  tryBlockedHostsId = prepareSleep(tryBlockedHosts,
				   TRYBLOCKEDHOSTPERIOD, NULL);
}


/*
 * Create a new Host object with default parameters. Called by the
 * InnListener.
 */

Host newDefaultHost (InnListener listener,
		     const char *name) 
{
  HostParams p;
  Host h = NULL;

  if (FindBlockedHost(name)==NULL)
    {

      p=newHostParams(defaultParams);
      ASSERT(p!=NULL);

      /* relies on fact listener and names are null in default*/
      p->peerName=xstrdup(name);
      p->ipName=xstrdup(name);
      
      h=newHost (listener,p);
      if (h==NULL)
	{
	  /* Couldn't get a lock - add to list of blocked peers */
	  addBlockedHost(p);
	  
	  syslog (LOG_ERR,NO_HOST,p->peerName) ;

	  return NULL;
	}

      h->isDynamic = true;
      h->removeOnReload = true;

      syslog (LOG_NOTICE,DYNAMIC_PEER,p->peerName) ;

    }
  return h;
}

/*
 * Create a new host and attach the supplied param structure
 */

static bool inited = false ;
Host newHost (InnListener listener, HostParams p)
{
  Host nh ; 

  ASSERT (p->maxChecks > 0) ;

  if (!inited)
    {
      inited = true ;
      atexit (hostCleanup) ;
    }

  /*
   * Once only, init the first blocked host check
   */
  if (tryBlockedHostsId==0)
    tryBlockedHostsId = prepareSleep(tryBlockedHosts,
				     TRYBLOCKEDHOSTPERIOD, NULL);

  nh =  CALLOC (struct host_s, 1) ;
  ASSERT (nh != NULL) ;

  nh->params = p;
  nh->listener = listener;

  nh->connections = NULL; /* We'll get these allocated later */
  nh->cxnActive = NULL;
  nh->cxnSleeping = NULL;

  nh->activeCxns = 0 ;
  nh->sleepingCxns = 0 ;

  nh->blockedCxn = NULL ;
  nh->notThisCxn = NULL ;

  nh->queued = NULL ;
  nh->queuedTail = NULL ;

  nh->processed = NULL ;
  nh->processedTail = NULL ;

  nh->deferred = NULL ;
  nh->deferredTail = NULL ;
  
  nh->statsId = 0 ;
  nh->ChkCxnsId = 0 ;
  nh->deferredId = 0;

  nh->myTape = newTape (nh->params->peerName,
			listenerIsDummy (nh->listener)) ;
  if (nh->myTape == NULL)
    {                           /* tape couldn't be locked, probably */
      FREE (nh->connections) ;
      FREE (nh->cxnActive) ;
      FREE (nh->cxnSleeping) ;
      
      FREE (nh) ;
      return NULL ; /* note we don't free up p */
    }

  nh->backedUp = false ;
  nh->backlog = 0 ;
  nh->deferLen = 0 ;

  nh->loggedBacklog = false ;
  nh->loggedModeOn = false ;
  nh->loggedModeOff = false ;
  nh->notifiedChangedRemBlckd = false ;
  nh->removeOnReload = false ; /* ready for config file reload */
  nh->isDynamic = false ;

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
  nh->artsSizeAccepted = 0 ;
  nh->artsSizeRejected = 0 ;

  nh->artsProcLastPeriod = 0;
  nh->secsInLastPeriod = 0;
  nh->lastCheckPoint = 0;
  nh->lastSentCheckPoint = 0;
  nh->lastTotalCheckPoint = 0;
  nh->maxCxnChk = true;
  nh->lastMaxCxnTime = time(0);
  nh->lastChkTime = time(0);
  nh->nextCxnTimeChk = 30;
  nh->backlogFilter = ((nh->params->dynBacklogLowWaterMark
			+ nh->params->dynBacklogHighWaterMark)
		       /200.0 /(1.0-nh->params->dynBacklogFilter));

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
  nh->gArtsSizeAccepted = 0 ;
  nh->gArtsSizeRejected = 0 ;
  nh->gCxnQueue = 0 ;
  nh->gNoQueue = 0 ;
  
  nh->firstConnectTime = 0 ;
  nh->connectTime = 0 ;
  
  nh->spoolTime = 0 ;

  nh->blNone = 0 ;
  nh->blFull = 0 ;
  nh->blQuartile[0] = nh->blQuartile[1] = nh->blQuartile[2] =
		      nh->blQuartile[3] = 0 ;
  nh->dlAccum = 0;
  nh->blAccum = 0;
  nh->blCount = 0;


  nh->maxConnections = 0; /* we currently have no connections allocated */

  /* Note that the following will override the initialCxns specified as
     maxCxns if we are on non-dyamic feed
   */
  hostAlterMaxConnections(nh, nh->params->absMaxConnections,
			  nh->params->initialConnections, false);

  nh->next = gHostList ;
  gHostList = nh ;
  gHostCount++ ;

  if (maxIpNameLen == 0)
    {
      start = theTime() ;
      strcpy (startTime,ctime (&start)) ;
      myPid = getpid() ;
    }
  
  if (strlen (nh->params->ipName) > maxIpNameLen)
    maxIpNameLen = strlen (nh->params->ipName) ;
  if (strlen (nh->params->peerName) > maxPeerNameLen)
    maxPeerNameLen = strlen (nh->params->peerName) ;
  
  return nh ;
}

struct sockaddr *hostIpAddr (Host host)
{
  int i ;
  struct sockaddr **newIpAddrPtrs = NULL;
  struct sockaddr_storage *newIpAddrs = NULL;
  struct sockaddr *returnAddr;

  ASSERT(host->params != NULL);

  /* check to see if need to look up the host name */
  if (host->nextIpLookup <= theTime())
    {
#ifdef HAVE_INET6
      int gai_ret;
      struct addrinfo *res, *p;

      if(( gai_ret = getaddrinfo(host->params->ipName, NULL, NULL, &res)) != 0
		      || res == NULL )

	{
	  syslog (LOG_ERR, HOST_RESOLV_ERROR, host->params->peerName,
		host->params->ipName, gai_ret == 0 ? "no addresses returned"
		: gai_strerror(gai_ret)) ;
	}
      else
	{
	  /* figure number of pointers that need space */
	  i = 0;
	  for ( p = res ; p ; p = p->ai_next ) ++i;

	  newIpAddrPtrs = (struct sockaddr **)
	    MALLOC ( (i + 1) * sizeof(struct sockaddr *) );
	  ASSERT (newIpAddrPtrs != NULL) ;

	  newIpAddrs = (struct sockaddr_storage *)
	    MALLOC ( i * sizeof(struct sockaddr_storage) );
	  ASSERT (newIpAddrs != NULL) ;

	  i = 0;
	  /* copy the addresses from the getaddrinfo linked list */
	  for( p = res ; p ; p = p->ai_next )
	    {
	      memcpy( &newIpAddrs[i], p->ai_addr, p->ai_addrlen );
	      newIpAddrPtrs[i] = (struct sockaddr *)(&newIpAddrs[i]);
	      ++i;
	    }
	  newIpAddrPtrs[i] = NULL ;
	  freeaddrinfo( res );
	}
#else
      struct hostent *hostEnt ;
      struct in_addr ipAddr;

      /* see if the ipName we're given is a dotted quad */
      if ( !inet_aton (host->params->ipName,&ipAddr) )
	{
	  if ((hostEnt = gethostbyname (host->params->ipName)) == NULL)
	    {
	      syslog (LOG_ERR, HOST_RESOLV_ERROR, host->params->peerName,
		    host->params->ipName, hstrerror(h_errno)) ;
	    }
	  else
	    {
	      /* figure number of pointers that need space */
	      for (i = 0 ; hostEnt->h_addr_list[i] ; i++)
		;

	      newIpAddrPtrs = (struct sockaddr **)
		MALLOC ( (i + 1) * sizeof(struct sockaddr *) );
	      ASSERT (newIpAddrPtrs != NULL) ;

	      newIpAddrs = (struct sockaddr_storage *)
		MALLOC ( i * sizeof( struct sockaddr_storage ) );
	      ASSERT (newIpAddrs != NULL) ;

	      /* copy the addresses from gethostbyname() static space */
	      i = 0;
	      for (i = 0 ; hostEnt->h_addr_list[i] ; i++)
		{
		  make_sin( (struct sockaddr_in *)(&newIpAddrs[i]),
			(struct in_addr *)(hostEnt->h_addr_list[i]) );
		  newIpAddrPtrs[i] = (struct sockaddr *)(&newIpAddrs[i]);
		}
	      newIpAddrPtrs[i] = NULL ;
	    }
	}
      else
	{
	  newIpAddrPtrs = (struct sockaddr **)
		  MALLOC( 2 * sizeof( struct sockaddr * ) );
	  ASSERT (newIpAddrPtrs != NULL) ;
	  newIpAddrs = (struct sockaddr_storage *)
		  MALLOC( sizeof( struct sockaddr_storage ) );
	  ASSERT (newIpAddrs != NULL) ;

	  make_sin( (struct sockaddr_in *)newIpAddrs, &ipAddr );
	  newIpAddrPtrs[0] = (struct sockaddr *)newIpAddrs;
	  newIpAddrPtrs[1] = NULL;
	}
#endif

      if (newIpAddrs)
	{
	  if (host->ipAddrs)
	  {
	    if(host->ipAddrs[0])
	      FREE (host->ipAddrs[0]);
	    FREE (host->ipAddrs) ;
	  }
	  host->ipAddrs = newIpAddrPtrs ;
	  host->nextIpAddr = 0 ;
	  host->nextIpLookup = theTime () + dnsExpPeriod ;
	}
      else
	{
	  /* failed to setup new addresses */
	  host->nextIpLookup = theTime () + dnsRetPeriod ;
	}
    }

  if (host->ipAddrs)
    returnAddr = host->ipAddrs[host->nextIpAddr] ;
  else
    returnAddr = NULL ;

  return returnAddr ;
}


void hostIpFailed (Host host)
{
  if (host->ipAddrs)
      if (host->ipAddrs[++host->nextIpAddr] == NULL)
	host->nextIpAddr = 0 ;
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
  
  fprintf (fp,"%s    peer-name : %s\n",indent,host->params->peerName) ;
  fprintf (fp,"%s    ip-name : %s\n",indent,host->params->ipName) ;
  fprintf (fp,"%s    abs-max-connections : %d\n",indent,
	   host->params->absMaxConnections) ;
  fprintf (fp,"%s    active-connections : %d\n",indent,host->activeCxns) ;
  fprintf (fp,"%s    sleeping-connections : %d\n",indent,host->sleepingCxns) ;
  fprintf (fp,"%s    initial-connections : %d\n",indent,
	   host->params->initialConnections) ;
  fprintf (fp,"%s    want-streaming : %s\n",indent,
           boolToString (host->params->wantStreaming)) ;
  fprintf (fp,"%s    drop-deferred : %s\n",indent,
           boolToString (host->params->dropDeferred)) ;
  fprintf (fp,"%s    min-queue-connection : %s\n",indent,
           boolToString (host->params->minQueueCxn)) ;
  fprintf (fp,"%s    remote-streams : %s\n",indent,
           boolToString (host->remoteStreams)) ;
  fprintf (fp,"%s    max-checks : %d\n",indent,host->params->maxChecks) ;
  fprintf (fp,"%s    article-timeout : %d\n",indent,
	   host->params->articleTimeout) ;
  fprintf (fp,"%s    response-timeout : %d\n",indent,
	   host->params->responseTimeout) ;
  fprintf (fp,"%s    close-period : %d\n",indent,
	   host->params->closePeriod) ;
  fprintf (fp,"%s    port : %d\n",indent,host->params->portNum) ;
  fprintf (fp,"%s    dynamic-method : %d\n",indent,
	   host->params->dynamicMethod) ;
  fprintf (fp,"%s    dynamic-backlog-filter : %2.1f\n",indent,
	   host->params->dynBacklogFilter) ;
  fprintf (fp,"%s    dynamic-backlog-lwm : %2.1f\n",indent,
	   host->params->dynBacklogLowWaterMark) ;
  fprintf (fp,"%s    dynamic-backlog-hwm : %2.1f\n",indent,
	   host->params->dynBacklogHighWaterMark) ;
  fprintf (fp,"%s    no-check on : %2.1f\n",indent,
	   host->params->lowPassHigh) ;
  fprintf (fp,"%s    no-check off : %2.1f\n",indent,
	   host->params->lowPassLow) ;
  fprintf (fp,"%s    no-check filter : %2.1f\n",indent,
	   host->params->lowPassFilter) ;
  fprintf (fp,"%s    backlog-limit : %d\n",indent,
	   host->params->backlogLimit) ;
  fprintf (fp,"%s    backlog-limit-high : %d\n",indent,
	   host->params->backlogLimitHigh) ;
  fprintf (fp,"%s    backlog-factor : %2.1f\n",indent,
	   host->params->backlogFactor) ;
  fprintf (fp,"%s    max-connections : %d\n",indent,
	   host->maxConnections) ;


  fprintf (fp,"%s    statistics-id : %d\n",indent,host->statsId) ;
  fprintf (fp,"%s    ChkCxns-id : %d\n",indent,host->ChkCxnsId) ;
  fprintf (fp,"%s    deferred-id : %d\n",indent,host->deferredId) ;
  fprintf (fp,"%s    backed-up : %s\n",indent,boolToString (host->backedUp));
  fprintf (fp,"%s    backlog : %d\n",indent,host->backlog) ;
  fprintf (fp,"%s    deferLen : %d\n",indent,host->deferLen) ;
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

  fprintf (fp,"%s    average (mean) defer length : %.1f\n", indent,
	   (double) host->dlAccum / cnt) ;
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
  fprintf (fp,"%s      number of samples : %u\n", indent, host->blCount) ;

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
  fprintf (fp,"%s    DEFERRED articles {\n",indent) ;
  for (qe = host->deferred ; qe != NULL ; qe = qe->next)
    {
#if 0
	printArticleInfo (qe->article,fp,indentAmt + INDENT_INCR) ;
#else
	fprintf (fp,"%s    %p\n",indent,qe->article) ;
#endif
    }

  fprintf (fp,"%s    }\n",indent) ;
  fprintf (fp,"%s    DEFERRED articles {\n",indent) ;
  for (qe = host->deferred ; qe != NULL ; qe = qe->next)
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

  d_printf (1,"Closing host %s\n",host->params->peerName) ;
  
  queuesToTape (host) ;
  delTape (host->myTape) ;
  host->myTape = NULL ;
  
  hostLogStats (host,true) ;

  clearTimer (host->statsId) ;
  clearTimer (host->ChkCxnsId) ;
  clearTimer (host->deferredId) ;
  
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


/*
 * check if host should get more connections opened, or some closed...
 */
void hostChkCxns(TimeoutId tid UNUSED, void *data) {
  Host host = (Host) data;
  u_int currArticles, currSentArticles, currTotalArticles, newMaxCxns ;
  double lastAPS, currAPS, percentTaken, ratio ;
  double backlogRatio, backlogMult;

  if(!host->maxCxnChk)
    return;

  ASSERT(host->params != NULL);

  if(host->secsInLastPeriod > 0) 
    lastAPS = host->artsProcLastPeriod / (host->secsInLastPeriod * 1.0);
  else
    lastAPS = host->artsProcLastPeriod * 1.0;

  newMaxCxns = host->maxConnections;

  currArticles =        (host->gArtsAccepted + host->gArtsRejected +
                        (host->gArtsNotWanted / 4)) - host->lastCheckPoint ;

  host->lastCheckPoint = (host->gArtsAccepted + host->gArtsRejected +
			 (host->gArtsNotWanted / 4));

  currSentArticles = host->gArtsAccepted + host->gArtsRejected
                      - host->lastSentCheckPoint ;

  host->lastSentCheckPoint = host->gArtsAccepted + host->gArtsRejected;

  currTotalArticles = host->gArtsAccepted + host->gArtsRejected
                      + host->gArtsRejected + host->gArtsQueueOverflow
		      - host->lastTotalCheckPoint ;

  host->lastTotalCheckPoint = host->gArtsAccepted + host->gArtsRejected
                      + host->gArtsRejected + host->gArtsQueueOverflow ;

  currAPS = currArticles / (host->nextCxnTimeChk * 1.0) ;

  percentTaken = currSentArticles * 1.0 /
    ((currTotalArticles==0)?1:currTotalArticles);

  /* Get how full the queue is currently */
  backlogRatio = (host->backlog * 1.0 / hostHighwater);
  backlogMult = 1.0/(1.0-host->params->dynBacklogFilter);

  d_printf(1,"%s hostChkCxns - entry filter=%3.3f blmult=%3.3f blratio=%3.3f\n",host->params->peerName,host->backlogFilter, backlogMult, backlogRatio);

  ratio = 0.0; /* ignore APS by default */

  switch (host->params->dynamicMethod)
    {
      case METHOD_COMBINED:
        /* When a high % of articles is being taken, take notice of the
	 * APS values. However for smaller %s, quickly start to ignore this
	 * and concentrate on queue sizes
	 */
        ratio = percentTaken * percentTaken;
	/* nobreak; */
      case METHOD_QUEUE:
        /* backlogFilter is an IIR filtered version of the backlogRatio.
	 */
        host->backlogFilter *= host->params->dynBacklogFilter;
	/* Penalise anything over the backlog HWM twice as severely
	 * (otherwise we end up feeding some sites constantly
	 * just below the HWM. This way random noise makes
	 * such sites jump to one more connection
	 *
	 * Use factor (1-ratio) so if ratio is near 1 we ignore this
	 */
	if (backlogRatio>host->params->dynBacklogLowWaterMark/100.0)
	  host->backlogFilter += (backlogRatio+1.0)/2.0 * (1.0-ratio);
	else
	  host->backlogFilter += backlogRatio * (1.0-ratio);

	/*
	 * Now bump it around for APS too
	 */
	if ((currAPS - lastAPS) >= 0.1)
	  host->backlogFilter += ratio*((currAPS - lastAPS) + 1.0);
	else if ((currAPS - lastAPS) < -.2)
	  host->backlogFilter -= ratio;
	
	d_printf(1,"%s hostChkCxns - entry hwm=%3.3f lwm=%3.3f new=%3.3f [%3.3f,%3.3f]\n",
	       host->params->peerName,host->params->dynBacklogHighWaterMark,
	       host->params->dynBacklogLowWaterMark,host->backlogFilter, 
	       (host->params->dynBacklogLowWaterMark * backlogMult / 100.0),
	       (host->params->dynBacklogHighWaterMark * backlogMult / 100.0));

        if (host->backlogFilter <
	    (host->params->dynBacklogLowWaterMark * backlogMult / 100.0))
	  newMaxCxns--;
	else if (host->backlogFilter >
		 (host->params->dynBacklogHighWaterMark * backlogMult / 100.0))
	  newMaxCxns++;
	break;
      case METHOD_STATIC:
	/* well not much to do, just check maxConnection = absMaxConnections */
	ASSERT (host->maxConnections == MAXCONLIMIT(host->params->absMaxConnections));
	break;
      case METHOD_APS:
	if ((currAPS - lastAPS) >= 0.1)
	  newMaxCxns += (int)(currAPS - lastAPS) + 1 ;
	else if ((currAPS - lastAPS) < -.2)
	  newMaxCxns--;
	break;
    }

  d_printf(1, "hostChkCxns: Chngs %f\n", currAPS - lastAPS);

  if (newMaxCxns < 1) newMaxCxns=1;
  if (newMaxCxns > MAXCONLIMIT(host->params->absMaxConnections))
    newMaxCxns = MAXCONLIMIT(host->params->absMaxConnections);

  if (newMaxCxns != host->maxConnections)
    {
      syslog(LOG_NOTICE, HOST_MAX_CONNECTIONS,
	     host->params->peerName, host->maxConnections,newMaxCxns);
 
      host->backlogFilter= ((host->params->dynBacklogLowWaterMark
			     + host->params->dynBacklogHighWaterMark)
			    /200.0 * backlogMult);
      host->artsProcLastPeriod = currArticles ;
      host->secsInLastPeriod = host->nextCxnTimeChk ;

      /* Alter MaxConnections and in doing so ensure we connect new
	 cxns immediately if we are adding stuff
       */
      hostAlterMaxConnections(host, host->params->absMaxConnections,
			      newMaxCxns, true);
  }

  if(host->nextCxnTimeChk <= 240) host->nextCxnTimeChk *= 2;
  else host->nextCxnTimeChk = 300;
  d_printf(1, "prepareSleep hostChkCxns, %d\n", host->nextCxnTimeChk);
  host->ChkCxnsId = prepareSleep(hostChkCxns, host->nextCxnTimeChk, host);
}


/*
 * have the Host transmit the Article if possible.
 */
void hostSendArticle (Host host, Article article)
{
  ASSERT(host->params != NULL);
  if (host->spoolTime > 0)
    {                           /* all connections are asleep */
      host->artsHostSleep++ ;
      host->gArtsHostSleep++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      procArtsToTape++ ;
      tapeTakeArticle (host->myTape, article) ;
      return ;
    }

  /* at least one connection is feeding or waiting and there's no backlog */
  if (host->queued == NULL)
    {
      u_int idx ;
      Article extraRef ;
      Connection cxn ;
      
      extraRef = artTakeRef (article) ; /* the referrence we give away */
      
      /* stick on the queue of articles we've handed off--we're hopeful. */
      queueArticle (article,&host->processed,&host->processedTail, 0) ;

      if (host->params->minQueueCxn) {
        Connection x_cxn = NULL ;
        u_int x_queue = host->params->maxChecks + 1 ;

        for (idx = 0 ; x_queue > 0 && idx < host->maxConnections ; idx++)
          if ((cxn = host->connections[idx]) != host->notThisCxn) {
            if (!host->cxnActive [idx]) {
              if (!host->cxnSleeping [idx]) {
                if (cxnTakeArticle (cxn, extraRef)) {
                  host->gNoQueue++ ;
                  return ;
                } else
                  d_printf (1,"%s Inactive connection %d refused an article\n",
                           host->params->peerName,idx) ;
              }
            } else {
              u_int queue = host->params->maxChecks - cxnQueueSpace (cxn) ;
              if (queue < x_queue) {
                x_queue = queue ;
                x_cxn = cxn ;
              }
            }
          }

        if (x_cxn != NULL && cxnTakeArticle (x_cxn, extraRef)) {
          if (x_queue == 0) host->gNoQueue++ ;
          else              host->gCxnQueue += x_queue ;
          return ;
        }

      } else {

        /* first we try to give it to one of our active connections. We
           simply start at the bottom and work our way up. This way
           connections near the end of the list will get closed sooner from
           idleness. */
        for (idx = 0 ; idx < host->maxConnections ; idx++)
          {
            if (host->cxnActive [idx] &&
                (cxn = host->connections[idx]) != host->notThisCxn &&
                cxnTakeArticle (cxn, extraRef)) {
              u_int queue = host->params->maxChecks - cxnQueueSpace (cxn) - 1;
              if (queue == 0) host->gNoQueue++ ;
              else            host->gCxnQueue += queue ;
	      return ;
            }
          }

        /* Wasn't taken so try to give it to one of the waiting connections. */
        for (idx = 0 ; idx < host->maxConnections ; idx++)
          if (!host->cxnActive [idx] && !host->cxnSleeping [idx] &&
              (cxn = host->connections[idx]) != host->notThisCxn)
            {
              if (cxnTakeArticle (cxn, extraRef)) {
                u_int queue = host->params->maxChecks - cxnQueueSpace (cxn) - 1;
                if (queue == 0) host->gNoQueue++ ;
                else            host->gCxnQueue += queue ;
                return ;
              } else
                d_printf (1,"%s Inactive connection %d refused an article\n",
                         host->params->peerName,idx) ;
            }
      }

      /* this'll happen if all connections are feeding and all
         their queues are full, or if those not feeding are asleep. */
      d_printf (1, "Couldn't give the article to a connection\n") ;
      
      delArticle (extraRef) ;
          
      remArticle (article,&host->processed,&host->processedTail) ;
      if (!cxnCheckstate (cxn))
        {
          host->artsToTape++ ;
          host->gArtsToTape++ ;
          procArtsToTape++ ;
          tapeTakeArticle (host->myTape,article) ;
          return ;
        }
    }

  /* either all the per connection queues were full or we already had
     a backlog, so there was no sense in checking. */
  queueArticle (article,&host->queued,&host->queuedTail, 0) ;
    
  host->backlog++ ;
  backlogToTape (host) ;
}







/*
 * called by the Host's connection when the remote is refusing postings
 * from us becasue we're not allowed (banner code 400).
 */
void hostCxnBlocked (Host host, Connection cxn, char *reason)
{
  ASSERT(host->params != NULL);
#ifndef NDEBUG
  {
    u_int i ;
    
    for (i = 0 ; i < host->maxConnections ; i++)
      if (host->connections [i] == cxn)
        ASSERT (host->cxnActive [i] == false) ;
  }
#endif

  if (host->blockedReason == NULL)
    host->blockedReason = xstrdup (reason) ;
  
  if (host->activeCxns == 0 && host->spoolTime == 0)
    {
      host->blockedCxn = cxn ;  /* to limit log notices */
      syslog (LOG_NOTICE,REMOTE_BLOCKED, host->params->peerName, reason) ;
    }
  else if (host->activeCxns > 0 && !host->notifiedChangedRemBlckd)
    {
      syslog (LOG_NOTICE,CHANGED_REMOTE_BLOCKED, host->params->peerName,reason) ;
      host->notifiedChangedRemBlckd = true ;
    }
  else if (host->spoolTime != 0 && host->blockedCxn == cxn)
    {
      syslog (LOG_NOTICE,REMOTE_STILL_BLOCKED, host->params->peerName, reason) ;
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
      if (doesStreaming && host->params->wantStreaming)
        syslog (LOG_NOTICE, REMOTE_DOES_STREAMING, host->params->peerName);
      else if (doesStreaming)
        syslog (LOG_NOTICE, REMOTE_STREAMING_OFF, host->params->peerName);
      else
        syslog (LOG_NOTICE, REMOTE_NO_STREAMING, host->params->peerName);

      if (host->spoolTime > 0)
        hostStopSpooling (host) ;

      /* set up the callback for statistics logging. */
      if (host->statsId != 0)
        clearTimer (host->statsId) ;
      host->statsId = prepareSleep (hostStatsTimeoutCbk, statsPeriod, host) ;

      if (host->ChkCxnsId != 0)
      clearTimer (host->ChkCxnsId);
      host->ChkCxnsId = prepareSleep (hostChkCxns, 30, host) ;

      host->remoteStreams = (host->params->wantStreaming ? doesStreaming : false) ;

      host->connectTime = theTime() ;
      if (host->firstConnectTime == 0)
        host->firstConnectTime = host->connectTime ;
    }
  else if (host->remoteStreams != doesStreaming && host->params->wantStreaming)
    syslog (LOG_NOTICE,STREAMING_CHANGE,host->params->peerName) ;

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
                clearTimer (host->ChkCxnsId) ;
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
  char msgstr[SMBUF] ;

  /* forget about the Connection and see if we are still holding any live
     connections still. */
  for (i = 0 ; i < host->maxConnections ; i++)
    if (host->connections [i] == cxn)
      {
        if (!amClosing (host))
          {
            syslog (LOG_ERR,CONNECTION_DISAPPEARING,host->params->peerName,i) ;
          }
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

      if (host->firstConnectTime > 0) {
        sprintf(msgstr, "accsize %.0f rejsize %.0f", host->gArtsSizeAccepted, host->gArtsSizeRejected);
        syslog (LOG_NOTICE,REALLY_FINAL_STATS,
                host->params->peerName,
                (long) (now - host->firstConnectTime),
                host->gArtsOffered, host->gArtsAccepted,
                host->gArtsNotWanted, host->gArtsRejected,
                host->gArtsMissing, msgstr,
                host->gArtsToTape, host->gArtsFromTape) ;
      }

      hostsLeft = listenerHostGone (host->listener, host) ;
      delHost (host) ;

      if (hostsLeft == 0) {
        sprintf(msgstr, "accsize %.0f rejsize %.0f", procArtsSizeAccepted, procArtsSizeRejected);
        syslog (LOG_NOTICE,PROCESS_FINAL_STATS,
                (long) (now - start),
                procArtsOffered, procArtsAccepted,
                procArtsNotWanted,procArtsRejected,
                procArtsMissing, msgstr,
                procArtsToTape, procArtsFromTape) ;
      }
      
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
  double len = artSize (article);

  d_printf (5,"Article %s (%s) was transferred\n", msgid, filename) ;
  
  host->artsAccepted++ ;
  host->gArtsAccepted++ ;
  procArtsAccepted++ ;
  host->artsSizeAccepted += len ;
  host->gArtsSizeAccepted += len ;
  procArtsSizeAccepted += len ;

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

  d_printf (5,"Article %s (%s) was not wanted\n", msgid, filename) ;
  
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
  double len = artSize (article);

  d_printf (5,"Article %s (%s) was rejected\n", msgid, filename) ;
  
  host->artsRejected++ ;
  host->gArtsRejected++ ;
  procArtsRejected++ ;
  host->artsSizeRejected += len ;
  host->gArtsSizeRejected += len ;
  procArtsSizeRejected += len ;

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
      int deferTimeout = 5 ; /* XXX - should be tunable */
      time_t now = theTime() ;

      extraRef = artTakeRef (article) ; /* hold a reference until requeued */
      articleGone (host,cxn,article) ; /* drop from the queue */

      if (host->deferred == NULL)
       {
           if (host->deferredId != 0)
             clearTimer (host->deferredId) ;
           host->deferredId = prepareSleep (hostDeferredArtCbk, deferTimeout,
                                            host) ;
        }

      queueArticle (article,&host->deferred,&host->deferredTail,
                   now + deferTimeout) ;
      host->deferLen++ ;
      backlogToTape (host) ;
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

  d_printf (5, "%s article is missing %s %s\n", host->params->peerName, msgid, filename) ;
    
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
      d_printf (5,"%s no article to give due to closing\n",host->params->peerName) ;

      return false ;
    }

  if (amtToGive == 0)
    d_printf (5,"%s Queue space is zero....\n",host->params->peerName) ;
  
  while (amtToGive > 0)
    {
      bool tookIt ;
      u_int queue = host->params->maxChecks - amtToGive ;
      
      if ((article = remHead (&host->queued,&host->queuedTail)) != NULL)
        {
          host->backlog-- ;
          tookIt = cxnQueueArticle (cxn,artTakeRef (article)) ;

          ASSERT (tookIt == true) ;

          if (queue == 0) host->gNoQueue++ ;
          else            host->gCxnQueue += queue ;

          queueArticle (article,&host->processed,&host->processedTail, 0) ;
          amtToGive-- ;

          gaveSomething = true ;
        }
      else if ((article = getArticle (host->myTape)) != NULL) 
        {                       /* go to the tapes */
          tookIt = cxnQueueArticle (cxn,artTakeRef (article)) ;

          ASSERT (tookIt == true) ;

          if (queue == 0) host->gNoQueue++ ;
          else            host->gCxnQueue += queue ;

          host->artsFromTape++ ;
          host->gArtsFromTape++ ;
          procArtsFromTape++ ;
          queueArticle (article,&host->processed,&host->processedTail, 0) ;
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
    
  return host->params->peerName ;
}


/* return true if the Connections for this host should attempt to do
   streaming. */
bool hostWantsStreaming (Host host)
{
  return host->params->wantStreaming ;
}

u_int hostMaxChecks (Host host)
{
  return host->params->maxChecks ;
}

bool hostDropDeferred (Host host)
{
  return host->params->dropDeferred ;
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
void hostLogNoCheckMode (Host host, bool on, double low, double cur, double high)
{
  if (on && host->loggedModeOn == false)
    {
      syslog (LOG_NOTICE, STREAMING_MODE_SWITCH, host->params->peerName, low, cur, high) ;
      host->loggedModeOn = true ;
    }
  else if (!on && host->loggedModeOff == false) 
    {
      syslog (LOG_NOTICE, STREAMING_MODE_UNDO, host->params->peerName, low, cur, high) ;
      host->loggedModeOff = true ;
    }
}



void hostSetStatusFile (const char *filename)
{
  FILE *fp ;
  
  if (filename == NULL)
    die ("Can't set status file name with a NULL filename\n") ;
  else if (*filename == '\0')
    die ("Can't set status file name with a empty string\n") ;

  if (*filename == '/')
    statusFile = xstrdup (filename) ;
  else
    {
      const char *logDir = innconf->pathlog;
      
      statusFile = malloc (strlen (logDir) + strlen (filename) + 2) ;
      sprintf (statusFile,"%s/%s",logDir,filename) ;
    }

  if ((fp = fopen (statusFile,"w")) == NULL)
    {
      syslog (LOG_ERR,"Status file is not a valid pathname: %s",
              statusFile) ;
      FREE (statusFile) ;
      statusFile = NULL ;
    }
  else
    fclose (fp) ;
}

void gHostStats (void)
{
  Host h ;
  time_t now = theTime() ;
  char msgstr[SMBUF] ;

  for (h = gHostList ; h != NULL ; h = h->next)
      if (h->firstConnectTime > 0) {
        sprintf(msgstr, "accsize %.0f rejsize %.0f", h->gArtsSizeAccepted, h->gArtsSizeRejected);
        syslog (LOG_NOTICE,REALLY_FINAL_STATS,
                h->params->peerName,
                (long) (now - h->firstConnectTime),
                h->gArtsOffered, h->gArtsAccepted,
                h->gArtsNotWanted, h->gArtsRejected,
                h->gArtsMissing, msgstr,
                h->gArtsToTape, h->gArtsFromTape) ;
      }
}



/**********************************************************************/
/**                      PRIVATE FUNCTIONS                           **/
/**********************************************************************/




#define INHERIT	1
#define NO_INHERIT 0


static HostParams hostDetails (scope *s,
			       char *name,
			       bool isDefault,
			       FILE *fp)
{
  long iv ;
  int bv, vival, inherit ;
  HostParams p;
  char * q;
  double rv, l, h ;
  value * v;

  p=newHostParams(isDefault?NULL:defaultParams);

  if (isDefault)
    {
      ASSERT (name==NULL);
    }
  else
    {
      if (name)
	{
	  p->peerName=xstrdup(name);
	}
  
      if (s != NULL)
        {
	  if (getString (s,IP_NAME,&q,NO_INHERIT))
	    p->ipName = q ;
	  else
	    p->ipName = xstrdup (name) ;
        }
    }


  /* check required global defaults are there and have good values */
  

#define GETINT(sc,f,n,min,max,req,val,inh)              \
  vival = validateInteger(f,n,min,max,req,val,sc,inh);  \
  if (isDefault) do{                                    \
    if(vival==VALUE_WRONG_TYPE)                         \
      {                                                 \
        logOrPrint(LOG_CRIT,fp,"cannot continue");      \
        exit(1);                                        \
      }                                                 \
    else if(vival != VALUE_OK)                          \
      val = 0;                                          \
  } while(0);                                           \
  iv = 0 ;                                              \
  (void) getInteger (sc,n,&iv,inh) ;                    \
  val = (u_int) iv ;                                    \

#define GETREAL(sc,f,n,min,max,req,val,inh)             \
  vival = validateReal(f,n,min,max,req,val,sc,inh);     \
  if (isDefault) do{                                    \
    if(vival==VALUE_WRONG_TYPE)                         \
      {                                                 \
        logOrPrint(LOG_CRIT,fp,"cannot continue");      \
        exit(1);                                        \
      }                                                 \
    else if(vival != VALUE_OK)                          \
      rv = 0;                                           \
  } while(0);                                           \
  rv = 0 ;                                              \
  (void) getReal (sc,n,&rv,inh) ;                       \
  val = rv ;                                            \

#define GETBOOL(sc,f,n,req,val,inh)                     \
  vival = validateBool(f,n,req,val,sc,inh);             \
  if (isDefault) do{                                    \
    if(vival==VALUE_WRONG_TYPE)                         \
      {                                                 \
        logOrPrint(LOG_CRIT,fp,"cannot continue");      \
        exit(1);                                        \
      }                                                 \
    else if(vival != VALUE_OK)                          \
      bv = 0;                                           \
  } while(0);                                           \
  bv = 0 ;                                              \
  (void) getBool (sc,n,&bv,inh)  ;                      \
  val = (bv ? true : false);                            \

  inherit = isDefault?NO_INHERIT:INHERIT;
  GETINT(s,fp,"article-timeout",0,LONG_MAX,REQ,p->articleTimeout, inherit);
  GETINT(s,fp,"response-timeout",0,LONG_MAX,REQ,p->responseTimeout, inherit);
  GETINT(s,fp,"close-period",0,LONG_MAX,REQ,p->closePeriod, inherit);
  GETINT(s,fp,"initial-connections",0,LONG_MAX,REQ,p->initialConnections, inherit);
  GETINT(s,fp,"max-connections",0,LONG_MAX,REQ,p->absMaxConnections, inherit);
  GETINT(s,fp,"max-queue-size",1,LONG_MAX,REQ,p->maxChecks, inherit);
  GETBOOL(s,fp,"streaming",REQ,p->wantStreaming, inherit);
  GETBOOL(s,fp,"drop-deferred",REQ,p->dropDeferred, inherit);
  GETBOOL(s,fp,"min-queue-connection",REQ,p->minQueueCxn, inherit);
  GETREAL(s,fp,"no-check-high",0.0,100.0,REQ,p->lowPassHigh, inherit);
  GETREAL(s,fp,"no-check-low",0.0,100.0,REQ,p->lowPassLow, inherit);
  GETREAL(s,fp,"no-check-filter",0.1,DBL_MAX,REQ,p->lowPassFilter, inherit);
  GETINT(s,fp,"port-number",0,LONG_MAX,REQ,p->portNum, inherit);
  GETINT(s,fp,"backlog-limit",0,LONG_MAX,REQ,p->backlogLimit, inherit);

  if (findValue (s,"backlog-factor",inherit) == NULL &&
      findValue (s,"backlog-limit-high",inherit) == NULL)
    {
      logOrPrint (LOG_ERR,fp,NOFACTORHIGH,"backlog-factor",LIMIT_FUDGE) ;
      addReal (s,"backlog-factor",LIMIT_FUDGE) ;
      rv = 0 ;
    }

  GETINT(s,fp,"backlog-limit-high",0,LONG_MAX,NOTREQNOADD,p->backlogLimitHigh, inherit);
  GETREAL(s,fp,"backlog-factor",1.0,DBL_MAX,NOTREQNOADD,p->backlogFactor, inherit);

  GETINT(s,fp,"dynamic-method",0,3,REQ,p->dynamicMethod, inherit);
  GETREAL(s,fp,"dynamic-backlog-filter",0.0,DBL_MAX,REQ,p->dynBacklogFilter, inherit);
  GETREAL(s,fp,"dynamic-backlog-low",0.0,100.0,REQ,p->dynBacklogLowWaterMark, inherit);
  GETREAL(s,fp,"dynamic-backlog-high",0.0,100.0,REQ,p->dynBacklogHighWaterMark, inherit);

  l=p->lowPassLow;
  h=p->lowPassHigh;
  if (l > h)
    {
      logOrPrint (LOG_ERR,fp,NOCK_LOWVSHIGH,l,h,NOCHECKLOW,NOCHECKHIGH) ;
      rv = 0 ;
      v = findValue (s,"no-check-low",NO_INHERIT) ;
      v->v.real_val = p->lowPassLow = NOCHECKLOW ;
      v = findValue (s,"no-check-high",NO_INHERIT) ;
      v->v.real_val = p->lowPassHigh = NOCHECKHIGH ;
    }
  else if (h - l < 5.0)
    logOrPrint (LOG_WARNING,fp,NOCK_LOWHIGHCLOSE,l,h) ;

  return p;
}




static HostParams getHostInfo (void)
{
  static int idx = 0 ;
  value *v ;
  scope *s ;
  HostParams p=NULL;

  bool isGood = false ;

  if (topScope == NULL)
    return p;
  
  while ((v = getNextPeer (&idx)) != NULL) 
    {
      if (!ISPEER (v))
        continue ;

      s = v->v.scope_val ;

      p=hostDetails(s,v->name,false,NULL);

      isGood = true ;
      
      break ;
    }

  if (v == NULL)
    idx = 0 ;                   /* start over next time around */

  return p;
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
  FREE (host->params->peerName) ;
  FREE (host->params->ipName) ;

  if (host->ipAddrs)
  {
    if(host->ipAddrs[0])
      FREE (host->ipAddrs[0]);
    FREE (host->ipAddrs) ;
  }

  FREE (host) ;
  gHostCount-- ;
}



static Host findHostByName (char *name) 
{
  Host h;

  for (h = gHostList; h != NULL; h = h->next)
    if ( strcmp(h->params->peerName, name) == 0 )
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
      syslog (LOG_NOTICE,SPOOLING,host->params->peerName) ;
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
  char msgstr[SMBUF] ;

  if (host->spoolTime == 0 && host->connectTime == 0)
    return ;        /* host has never connected and never started spooling*/

  startPeriod = (host->spoolTime != 0 ? &host->spoolTime : &host->connectTime);

  if (now - *startPeriod >= statsResetPeriod)
    final = true ;
  
  if (host->spoolTime != 0)
    syslog (LOG_NOTICE, HOST_SPOOL_STATS, host->params->peerName,
            (final ? "final" : "checkpoint"),
            (long) (now - host->spoolTime), host->artsToTape,
            host->artsHostClose, host->artsHostSleep) ;
  else {
    sprintf(msgstr, "accsize %.0f rejsize %.0f", host->artsSizeAccepted, host->artsSizeRejected);
    syslog (LOG_NOTICE, HOST_STATS_MSG, host->params->peerName, 
            (final ? "final" : "checkpoint"),
            (long) (now - host->connectTime),
            host->artsOffered, host->artsAccepted,
            host->artsNotWanted, host->artsRejected,
            host->artsMissing, msgstr,
            host->artsToTape,
            host->artsHostClose, host->artsFromTape,
            host->artsDeferred, (double)host->dlAccum/cnt,
            host->artsCxnDrop,
            (double)host->blAccum/cnt, hostHighwater,
            (100.0*host->blNone)/cnt,
            (100.0*host->blQuartile[0])/cnt, (100.0*host->blQuartile[1])/cnt,
            (100.0*host->blQuartile[2])/cnt, (100.0*host->blQuartile[3])/cnt,
            (100.0*host->blFull)/cnt) ;
  }

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
      host->artsSizeAccepted = 0 ;
      host->artsSizeRejected = 0 ;
      
      *startPeriod = theTime () ; /* in of case STATS_RESET_PERIOD */
    }

    /* reset these each log period */
    host->blNone = 0 ;
    host->blFull = 0 ;
    host->blQuartile[0] = host->blQuartile[1] = host->blQuartile[2] =
                          host->blQuartile[3] = 0;
    host->dlAccum = 0;
    host->blAccum = 0;
    host->blCount = 0;

#if 0
  /* XXX turn this section on to get a snapshot at each log period. */
  if (gPrintInfo != NULL)
    gPrintInfo () ;
#endif
}








static double
convsize(double size, char **tsize)
{
    double dsize;
    static char tTB[]="TB";
    static char tGB[]="GB";
    static char tMB[]="MB";
    static char tKB[]="KB";
    static char tB []="B";

    if (size/((double)1024*1024*1024*1000)>=1.) {
	dsize=size/((double)1024*1024*1024*1024);
	*tsize=tTB;
    } else if (size/(1024*1024*1000)>=1.) {
	dsize=size/(1024*1024*1024);
	*tsize=tGB;
    } else if (size/(1024*1000)>=1.) {
	dsize=size/(1024*1024);
	*tsize=tMB;
    } else if (size/1000>=1.) {
	dsize=size/1024;
	*tsize=tKB;
    } else {
	dsize=size;
	*tsize=tB;
    }
    return dsize;
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
  
  TMRstart(TMR_STATUSFILE);
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
      long sec ;
      long offered ;        
      double size, totalsize;
      char *tsize;

      flogged = false ;
      
      now = time (NULL) ;
      sec = (long) (now - start) ;
      strcpy (timeString,ctime (&now)) ;

      if (genHtml)
        {
          fprintf (fp, "<HTML>\n"
		       "<HEAD>\n"
		       "<META HTTP-EQUIV=\"Refresh\" CONTENT=\"300;\">\n"
		       "</HEAD>\n"
		       "<BODY>\n") ;
	  fprintf (fp, "\n");
	  fprintf (fp, "<PRE>\n");
	}

      fprintf (fp,"%s\npid %d started %s\nUpdated: %s",
               versionInfo,(int) myPid,startTime,timeString) ;
      fprintf (fp,"(peers: %d active-cxns: %d sleeping-cxns: %d idle-cxns: %d)\n\n",
               peerNum, actConn, slpConn,(maxcon - (actConn + slpConn))) ;

      fprintf (fp,"Configuration file: %s\n\n",configFile) ;
      
      if (genHtml)
      {
        fprintf (fp, "</PRE>\n");
        fprintf (fp,"<UL>\n");
        for (h = gHostList ; h != NULL ; h = h->next)
          fprintf (fp,"<LI><A href=\"#%s\">%s</A></LI>\n",
                   h->params->peerName, h->params->peerName);
        fprintf (fp,"</UL>\n\n");
        fprintf (fp,"<PRE>\n");
      }

      mainLogStatus (fp) ;
      listenerLogStatus (fp) ;

/*
Default peer configuration parameters:
    article timeout: 600       initial connections: 1
   response timeout: 300           max connections: 5
       close period: 6000               max checks: 25
     want streaming: true           dynamic method: 1
        no-check on: 95.0%     dynamic backlog low: 25%
       no-check off: 90.0%    dynamic backlog high: 50%
    no-check filter: 50.0   dynamic backlog filter: 0.7
  backlog low limit: 1024                 port num: 119
 backlog high limit: 1280
     backlog factor: 1.1
*/
      fprintf(fp,"%sDefault peer configuration parameters:%s\n",
              genHtml ? "<B>" : "", genHtml ? "</B>" : "") ;
      fprintf(fp,"    article timeout: %-5d     initial connections: %d\n",
	    defaultParams->articleTimeout,
	    defaultParams->initialConnections) ;
      fprintf(fp,"   response timeout: %-5d         max connections: %d\n",
	    defaultParams->responseTimeout,
	    defaultParams->absMaxConnections) ;
      fprintf(fp,"       close period: %-5d              max checks: %d\n",
	    defaultParams->closePeriod,
	    defaultParams->maxChecks) ;
      fprintf(fp,"     want streaming: %-5s          dynamic method: %d\n",
	    defaultParams->wantStreaming ? "true " : "false",
	    defaultParams->dynamicMethod) ;
      fprintf(fp,"        no-check on: %-2.1f%%     dynamic backlog low: %-2.1f%%\n",
	    defaultParams->lowPassHigh,
	    defaultParams->dynBacklogLowWaterMark) ;
      fprintf(fp,"       no-check off: %-2.1f%%    dynamic backlog high: %-2.1f%%\n",
	    defaultParams->lowPassLow,
	    defaultParams->dynBacklogHighWaterMark) ;
      fprintf(fp,"    no-check filter: %-2.1f   dynamic backlog filter: %-2.1f\n",
	    defaultParams->lowPassFilter,
	    defaultParams->dynBacklogFilter) ;
      fprintf(fp,"  backlog limit low: %-7d         drop-deferred: %s\n",
	    defaultParams->backlogLimit,
	    defaultParams->dropDeferred ? "true " : "false");
      fprintf(fp," backlog limit high: %-7d         min-queue-cxn: %s\n",
	    defaultParams->backlogLimitHigh,
	    defaultParams->minQueueCxn ? "true " : "false");
      fprintf(fp,"     backlog factor: %1.1f\n\n",
	    defaultParams->backlogFactor);

      tapeLogGlobalStatus (fp) ;

      fprintf (fp,"\n") ;
      fprintf(fp,"%sglobal (process)%s\n",
              genHtml ? "<B>" : "", genHtml ? "</B>" : "") ;
      
      fprintf (fp, "   seconds: %ld\n", sec) ;
      if (sec == 0) sec = 1 ;
      offered = procArtsOffered ? procArtsOffered : 1 ;
      totalsize = procArtsSizeAccepted+procArtsSizeRejected ;
      if (totalsize == 0) totalsize = 1. ;

      fprintf (fp, "   offered: %-5ld\t%6.2f art/s\n",
		procArtsOffered,
		(double)procArtsOffered/sec) ;
      fprintf (fp, "  accepted: %-5ld\t%6.2f art/s\t%5.1f%%\n",
		procArtsAccepted,
		(double)procArtsAccepted/sec,
		(double)procArtsAccepted*100./offered) ;
      fprintf (fp, "   refused: %-5ld\t%6.2f art/s\t%5.1f%%\n",
		procArtsNotWanted,
		(double)procArtsNotWanted/sec,
		(double)procArtsNotWanted*100./offered) ;
      fprintf (fp, "  rejected: %-5ld\t%6.2f art/s\t%5.1f%%\n",
		procArtsRejected,
		(double)procArtsRejected/sec,
		(double)procArtsRejected*100./offered) ;
      fprintf (fp, "   missing: %-5ld\t%6.2f art/s\t%5.1f%%\n",
		procArtsMissing,
		(double)procArtsMissing/sec,
		(double)procArtsMissing*100./offered) ;
      fprintf (fp, "  deferred: %-5ld\t%6.2f art/s\t%5.1f%%\n",
		procArtsDeferred,
		(double)procArtsDeferred/sec,
		(double)procArtsDeferred*100./offered) ;

      size=convsize(procArtsSizeAccepted, &tsize);
      fprintf (fp, "accpt size: %.3g %s", size, tsize) ;
      size=convsize(procArtsSizeAccepted/sec, &tsize);
      fprintf (fp, " \t%6.3g %s/s\t%5.1f%%\n",
		size, tsize,
		procArtsSizeAccepted*100./totalsize) ;

      size=convsize(procArtsSizeRejected, &tsize);
      fprintf (fp, "rejct size: %.3g %s", size, tsize) ;
      size=convsize(procArtsSizeRejected/sec, &tsize);
      fprintf (fp, " \t%6.3g %s/s\t%5.1f%%\n",
		size, tsize,
		procArtsSizeRejected*100./totalsize) ;

      fprintf (fp, "\n");

      for (h = gHostList ; h != NULL ; h = h->next)
        hostPrintStatus (h,fp) ;

      if (genHtml) 
	{
          fprintf (fp,"</PRE>\n") ;
          fprintf (fp,"</BODY>\n") ;
          fprintf (fp,"</HTML>\n") ;
	}
      
      fclose (fp) ;
    }
    TMRstop(TMR_STATUSFILE);
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
 *   missing: 0          no-check on: 95.0%      idle cxns: 4
 *  deferred: 0         no-check off: 95.0%       max cxns: 8/10
 *  requeued: 0        no-check fltr: 50.0    queue length: 0.0/200
 *   spooled: 0       dynamic method: 0              empty: 100.0%
 *[overflow]: 0        dyn b'log low: 25%          >0%-25%: 0.0%
 *[on_close]: 0       dyn b'log high: 50%          25%-50%: 0.0%
 *[sleeping]: 0       dyn b'log stat: 37%          50%-75%: 0.0%
 * unspooled: 0       dyn b'log fltr: 0.7        75%-<100%: 0.0%
 *  no queue: 1234    avr.cxns queue: 0.0             full: 0.0%
 *accpt size: 121.1 MB drop-deferred: false   defer length: 0
 *rejct size: 27.1 MB  min-queue-cxn: false
 *                 backlog low limit: 1000000
 *                backlog high limit: 2000000     (factor 2.0)
 *                 backlog shrinkage: 0 bytes (from current file)
 *   offered:  1.13 art/s   accepted:  0.69 art/s (101.71 KB/s)
 *   refused:  0.01 art/s   rejected:  0.42 art/s (145.11 KB/s)
 *   missing 0 spooled 0
 *
 */
static void hostPrintStatus (Host host, FILE *fp)
{
  time_t now = theTime() ;
  double cnt = (host->blCount) ? (host->blCount) : 1.0;
  double size;
  char *tsize;
  char buf[]="1234.1 MB";

  ASSERT (host != NULL) ;
  ASSERT (fp != NULL) ;

  if (genHtml)
    fprintf (fp,"<A name=\"%s\"><B>%s</B></A>",host->params->peerName,
             host->params->peerName);
  else
    fprintf (fp,"%s",host->params->peerName);

  if (host->blockedReason != NULL)
    fprintf (fp,"  (remote status: ``%s'')",host->blockedReason) ;

  fputc ('\n',fp) ;

  fprintf (fp, "   seconds: %-7ld   art. timeout: %-5d        ip name: %s\n",
	   host->firstConnectTime > 0 ? (long)(now - host->firstConnectTime) : 0,
	   host->params->articleTimeout, host->params->ipName) ;
           
  fprintf (fp, "   offered: %-7ld  resp. timeout: %-5d           port: %d\n",
	   (long) host->gArtsOffered, host->params->responseTimeout,
	   host->params->portNum);

  fprintf (fp, "  accepted: %-7ld want streaming: %s      active cxns: %d\n",
	   (long) host->gArtsAccepted, 
           (host->params->wantStreaming ? "yes" : "no "),
	   host->activeCxns) ;

  fprintf (fp, "   refused: %-7ld   is streaming: %s    sleeping cxns: %d\n",
	   (long) host->gArtsNotWanted,
           (host->remoteStreams ? "yes" : "no "),
	   host->sleepingCxns) ;

  fprintf (fp, "  rejected: %-7ld     max checks: %-5d   initial cxns: %d\n",
	   (long) host->gArtsRejected, host->params->maxChecks,
	   host->params->initialConnections) ;

  fprintf (fp, "   missing: %-7ld    no-check on: %-3.1f%%      idle cxns: %d\n",
	   (long) host->gArtsMissing, host->params->lowPassHigh,
           host->maxConnections - (host->activeCxns + host->sleepingCxns)) ;

  fprintf (fp, "  deferred: %-7ld   no-check off: %-3.1f%%       max cxns: %d/%d\n",
	   (long) host->gArtsDeferred, host->params->lowPassLow,
	   host->maxConnections, host->params->absMaxConnections) ;

  fprintf (fp, "  requeued: %-7ld  no-check fltr: %-3.1f    queue length: %-3.1f/%d\n",
	   (long) host->gArtsCxnDrop, host->params->lowPassFilter,
	   (double)host->blAccum / cnt, hostHighwater) ;

  fprintf (fp, "   spooled: %-7ld dynamic method: %-5d          empty: %-3.1f%%\n",
	   (long) host->gArtsToTape,
	   host->params->dynamicMethod,
	   100.0 * host->blNone / cnt) ;

  fprintf (fp, "[overflow]: %-7ld  dyn b'log low: %-3.1f%%        >0%%-25%%: %-3.1f%%\n",
	   (long) host->gArtsQueueOverflow, 
	   host->params->dynBacklogLowWaterMark,
	   100.0 * host->blQuartile[0] / cnt) ;

  fprintf (fp, "[on_close]: %-7ld dyn b'log high: %-3.1f%%        25%%-50%%: %-3.1f%%\n",
	   (long) host->gArtsHostClose,
	   host->params->dynBacklogHighWaterMark,
	   100.0 * host->blQuartile[1] / cnt) ;

  fprintf (fp, "[sleeping]: %-7ld dyn b'log stat: %-3.1f%%        50%%-75%%: %-3.1f%%\n",
	   (long) host->gArtsHostSleep,
	   host->backlogFilter*100.0*(1.0-host->params->dynBacklogFilter),
	   100.0 * host->blQuartile[2] / cnt) ;

  fprintf (fp, " unspooled: %-7ld dyn b'log fltr: %-3.1f       75%%-<100%%: %-3.1f%%\n",
	   (long) host->gArtsFromTape,
	   host->params->dynBacklogLowWaterMark,
	   100.0 * host->blQuartile[3] / cnt) ;

  fprintf (fp, "  no queue: %-7ld avr.cxns queue: %-3.1f             full: %-3.1f%%\n",
	   (long) host->gNoQueue,
	   (double) host->gCxnQueue / (host->gArtsOffered ? host->gArtsOffered :1) ,
	   100.0 * host->blFull / cnt) ;
  size=convsize(host->gArtsSizeAccepted, &tsize);
  sprintf(buf,"%.3g %s", size, tsize);
  fprintf (fp, "accpt size: %-8s drop-deferred: %-5s   defer length: %-3.1f\n",
	   buf, host->params->dropDeferred ? "true " : "false",
           (double)host->dlAccum / cnt) ;
  size=convsize(host->gArtsSizeRejected, &tsize);
  sprintf(buf,"%.3g %s", size, tsize);
  fprintf (fp, "rejct size: %-8s min-queue-cxn: %s\n",
	   buf, host->params->minQueueCxn ? "true " : "false");

  tapeLogStatus (host->myTape,fp) ;

  {
  time_t      sec = (time_t) (now - host->connectTime);
  double      or, ar, rr, jr;
  double      ars, jrs;
  char       *tars, *tjrs;
  if (sec != 0) {
      or = (double) host->artsOffered / (double) sec;
      ar = (double) host->artsAccepted / (double) sec;
      rr = (double) host->artsNotWanted / (double) sec;
      jr = (double) host->artsRejected / (double) sec;
      ars = convsize (host->artsSizeAccepted/sec, &tars);
      jrs = convsize (host->artsSizeRejected/sec, &tjrs);
      fprintf(fp, "   offered: %5.2f art/s   accepted: %5.2f art/s, %.3g %s/s\n",
	      or, ar, ars, tars);
      fprintf(fp, "   refused: %5.2f art/s   rejected: %5.2f art/s, %.3g %s/s\n",
	      rr, jr, jrs, tjrs);
  }
  fprintf(fp, "   missing %d spooled %d\n",
	  host->artsMissing, host->artsToTape);
  }

#ifdef        XXX_STATSHACK
  {
  time_t      now = time(NULL), sec = (long) (now - host->connectTime);
  float               or, ar, rr, jr;

  if (sec != 0) {
      or = (float) host->artsOffered / (float) sec;
      ar = (float) host->artsAccepted / (float) sec;
      rr = (float) host->artsNotWanted / (float) sec;
      jr = (float) host->artsRejected / (float) sec;
      fprintf(fp, "\t\tor %02.2f ar %02.2f rr %02.2f jr %02.2f\n",
              or, ar, rr, jr);
  }
  fprintf(fp, "\tmissing %d spooled %d\n",
      host->artsMissing,host->backlogSpooled);
  }
#endif        /* XXX_STATSHACK */
  
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


/*
 * The callback function for the deferred article timer to call.
 */
static void hostDeferredArtCbk (TimeoutId tid, void *data)
{
  Host host = (Host) data ;
  time_t now = theTime () ;
  Article article ;

  (void) tid ;                  /* keep lint happy */

  ASSERT (tid == host->deferredId) ;

  while (host->deferred && host->deferred->whenToRequeue <= now)
    {
      article = remHead (&host->deferred,&host->deferredTail) ;
      host->deferLen-- ;
      hostSendArticle (host, article) ; /* requeue it */
    }

  if (host->deferred)
    host->deferredId = prepareSleep (hostDeferredArtCbk,
                                    host->deferred->whenToRequeue - now,
                                    host) ;
  else
    host->deferredId = 0;
}


/* if the host has too many unprocessed articles so we send some to the tape. */
static void backlogToTape (Host host)
{
  Article article ;

  while ((host->backlog + host->deferLen) > hostHighwater)
    {
      if (!host->loggedBacklog)
	{
#if 0               /* this message is pretty useless and confuses people */
	  syslog (LOG_NOTICE,BACKLOG_TO_TAPE,host->params->peerName /* ,host->backlog */) ;
#endif
	  host->loggedBacklog = true ;
	}
  
      if (host->deferred != NULL)
       {
         article = remHead (&host->deferred,&host->deferredTail) ;
          host->deferLen--;
       }
      else
       {
         article = remHead (&host->queued,&host->queuedTail) ;
          host->backlog--;
       }

      ASSERT(article != NULL);

      host->artsQueueOverflow++ ;
      host->gArtsQueueOverflow++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      procArtsToTape++ ;
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
      procArtsToTape++ ;
      tapeTakeArticle (host->myTape,art) ;
    }
  
  while ((art = remHead (&host->queued,&host->queuedTail)) != NULL)
    {
      host->backlog-- ;
      host->artsHostClose++ ;
      host->gArtsHostClose++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      procArtsToTape++ ;
      tapeTakeArticle (host->myTape,art) ;
    }

  while ((art = remHead (&host->deferred,&host->deferredTail)) != NULL)
    {
      host->deferLen-- ;
      host->artsHostClose++ ;
      host->gArtsHostClose++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      procArtsToTape++ ;
      tapeTakeArticle (host->myTape,art) ;
    }

  while ((art = remHead (&host->deferred,&host->deferredTail)) != NULL)
    {
      host->deferLen-- ;
      host->artsHostClose++ ;
      host->gArtsHostClose++ ;
      host->artsToTape++ ;
      host->gArtsToTape++ ;
      procArtsToTape++ ;
      tapeTakeArticle (host->myTape,art) ;
    }
}







#define QUEUE_ELEM_POOL_SIZE ((4096 - 2 * (sizeof (void *))) / (sizeof (struct proc_q_elem)))

static ProcQElem queueElemPool ;

/*
 * Add an article to the given queue.
 */
static void queueArticle (Article article, ProcQElem *head, ProcQElem *tail,
                         time_t when)
{
  ProcQElem elem ;

  if (queueElemPool == NULL)
    {
      unsigned int i ;

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
  elem->whenToRequeue = when ;
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
                     long low, long high, int required, long setval,
		     scope * sc, u_int inh)
{
  int rval = VALUE_OK ;
  value *v ;
  scope *s ;
  char *p = strrchr (name,':') ;
  
  v = findValue (sc,name,inh) ;
  if (v == NULL && required != NOTREQNOADD)
    {
      s = findScope (sc,name,0) ;
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
                         double high, int required, double setval,
			 scope * sc, u_int inh)
{
  int rval = VALUE_OK ;
  value *v ;
  scope *s ;
  char *p = strrchr (name,':') ;
  
  v = findValue (sc,name,inh) ;
  if (v == NULL && required != NOTREQNOADD)
    {
      s = findScope (sc,name,0) ;
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



static int validateBool (FILE *fp, const char *name, int required, bool setval,
			 scope * sc, u_int inh)
{
  int rval = VALUE_OK ;
  value *v ;
  scope *s ;
  char *p = strrchr (name,':') ;
  
  v = findValue (sc,name,inh) ;
  if (v == NULL && required != NOTREQNOADD)
    {
      s = findScope (sc,name,0) ;
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
      h->dlAccum += h->deferLen ;
      h->blAccum += h->backlog ;
      if (h->backlog == 0)
	   h->blNone++ ;
      else if (h->backlog >= (hostHighwater - h->deferLen))
	   h->blFull++ ;
      else
	   h->blQuartile[(4*h->backlog) / (hostHighwater - h->deferLen)]++ ;
      h->blCount++ ;
    }
}
static void hostCleanup (void)
{
  if (statusFile != NULL)
    FREE (statusFile) ;
}

