/*  $Id$
**
**  The implementation of the innfeed EndPoint object class.
**
**  Written by James Brister <brister@vix.com>
**  Copyright 1996 by the Internet Software Consortium
**
**  For license terms, see the end of this file.
**
**  The EndPoint class is what gives the illusion (sort of, kind of) of
**  threading.  Basically it controls a select loop and a set of EndPoint
**  objects.  Each EndPoint has a file descriptor it is interested in.  The
**  users of the EndPoint tell the EndPoints to notify them when a read or
**  write has been completed (or simple if the file descriptor is read or
**  write ready).
*/

#include "innfeed.h"
#include "config.h"
#include "clibrary.h"
#include "portable/socket.h"
#include "portable/time.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <syslog.h>

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#include "inn/innconf.h"
#include "inn/messages.h"
#include "libinn.h"

#include "buffer.h"
#include "configfile.h"
#include "endpoint.h"
#include "host.h"

static const char *const timer_name[] = {
  "idle", "blstats", "stsfile", "newart", "readart", "prepart", "read",
  "write", "cb"
};

#if ! defined (NSIG)
#define NSIG 32
#endif


  /* This is the structure that is the EndPoint */
struct endpoint_s 
{
      /* fields for managing multiple reads into the inBuffer. */
    Buffer *inBuffer ;          /* list of buffers to read into */
    unsigned int inBufferIdx ;         /* where is list we're at. */
    size_t inIndex ;            /* where in current read we're at */
    size_t inMinLen ;           /* minimum amount to read */
    size_t inAmtRead ;          /* amount read so far */
    EndpRWCB inCbk ;            /* callback for when read complete */
    void *inClientData ;        /* callback data */
    
      /* fields for managing multiple writes from the outBuffer */
    Buffer *outBuffer ;         /* list of buffers to write */
    unsigned int outBufferIdx ;        /* index into buffer list to start write */
    size_t outIndex ;           /* where in current buffer we write from */
    size_t outSize ;            /* total of all the buffers */
    size_t outAmtWritten ;      /* amount written so far */
    EndpRWCB outProgressCbk ;   /* callback when done */
    EndpRWCB outDoneCbk ;       /* callback when done */
    void *outClientData ;       /* callback data */

    EndpWorkCbk workCbk ;       /* callback to run if no I/O to do */
    void *workData ;            /* data for callback */

    int myFd ;                  /* the file descriptor we're handling */
    int myErrno ;               /* the errno when I/O fails */
    
    double selectHits ;		/* indicates how often it's ready */
};



  /* A private structure. These hold the information on the timer callbacks. */
typedef struct timerqelem_s
{
    TimeoutId id ;              /* the id we gave out */
    time_t when ;               /* The time the timer should go off */
    EndpTCB func ;              /* the function to call */
    void *data ;                /* the client callback data */
    struct timerqelem_s *next ; /* next in the queue */
} *TimerElem, TimerElemStruct ;



  /* set to 1 elsewhere if you want stderr to get what's also being written
     in doWrite. */
int debugWrites ;

extern const char *InputFile ;

static EndPoint mainEndPoint ;
static bool mainEpIsReg = false ;
static unsigned int stdioFdMax = MAX_STDIO_FD ;

time_t  PrivateTime;


typedef void (*sigfn) (int) ;
static sigfn *sigHandlers ;

static volatile sig_atomic_t *sigFlags ;



  /* private functions */
static IoStatus doRead (EndPoint endp) ;
static IoStatus doWrite (EndPoint endp) ;
static IoStatus doExcept (EndPoint endp) ;
static void pipeHandler (int s) ;
static void signalHandler (int s) ;
static int hitCompare (const void *v1, const void *v2) ;
static void reorderPriorityList (void) ;
static TimerElem newTimerElem (TimeoutId i, time_t w, EndpTCB f, void *d) ;
static TimeoutId timerElemAdd (time_t when, EndpTCB func, void *data) ;
static struct timeval *getTimeout (struct timeval *tout) ;
static void doTimeout (void) ;
static void handleSignals (void) ;

#if 0
static int ff_set (fd_set *set, unsigned int start) ;
static int ff_free (fd_set *set, unsigned int start) ;
#endif
static void endpointCleanup (void) ;


  /* Private data */
static size_t maxEndPoints ;

static EndPoint *endPoints ;    /* endpoints indexed on fd */
static EndPoint *priorityList ; /* endpoints indexed on priority */

static int absHighestFd = 0 ;       /* never goes down */
static int highestFd = -1 ;
static unsigned int endPointCount = 0 ;
static unsigned int priorityCount = 0 ;

static fd_set rdSet ;
static fd_set wrSet ;
static fd_set exSet ;

static int keepSelecting ;

static TimerElem timeoutQueue ;
static TimerElem timeoutPool ;
static TimeoutId nextId ;
static int timeoutQueueLength ;




  /* Create a new EndPoint and hook it to the give file descriptor. All
     fields are initialized to appropriate values.  On the first time this
     function is called the global data structs that manages lists of
     endpoints are intialized. */
static bool inited = false ;

EndPoint newEndPoint (int fd) 
{
  EndPoint ep ;

  if (!inited)
    {
      inited = true ;
      atexit (endpointCleanup) ;
    }
  
  if (fd < 0)
    return NULL ;

  /* try to dup the fd to a larger number to leave lower values free for
     broken stdio implementations. */
  if (stdioFdMax > 0 && ((unsigned int) fd) <= stdioFdMax)
    {
      int newfd = fcntl(fd, F_DUPFD, stdioFdMax + 1);
      if (newfd >= 0)
        {
          d_printf (1,"Dupped fd %d to %d\n",fd,newfd) ;
          if (close (fd) != 0)
            syswarn ("ME oserr close (%d)", fd) ;
        }
      else
        {
          d_printf (1,"Couldn't dup fd %d to above %d\n",fd,stdioFdMax) ;
          newfd = fd ;
        }

      fd = newfd ;
    }

  if ((unsigned int) fd >= maxEndPoints)
    {
      unsigned int i = maxEndPoints ;
      
      maxEndPoints = (((fd + 256) / 256) * 256); /* round up to nearest 256 */ 
      if (endPoints == NULL)
        {
          endPoints = xmalloc (sizeof(EndPoint) * maxEndPoints) ;
          priorityList = xmalloc (sizeof(EndPoint) * maxEndPoints) ;
        }
      else
        {
          endPoints = xrealloc (endPoints,sizeof(EndPoint) * maxEndPoints) ;
          priorityList = xrealloc (priorityList,
                                   sizeof(EndPoint) * maxEndPoints) ;
        }

      for ( ; i < maxEndPoints ; i++)
        endPoints [i] = priorityList [i] = NULL ;
    }
  
  ASSERT (endPoints [fd] == NULL) ;

  if (fd > absHighestFd)
    {
      static bool sizelogged = false ;
          
#if defined (FD_SETSIZE)
      if (fd >= FD_SETSIZE)
        {
          sizelogged = true ;
          warn ("ME fd (%d) looks too big (%d -- FD_SETSIZE)", fd,
                FD_SETSIZE) ;
          return NULL ;
        }
#else
      if (fd > (sizeof (fd_set) * CHAR_BIT))
        {
          sizelogged = true ;
          warn ("ME fd (%d) looks too big (%d -- sizeof (fd_set) * CHAR_BIT)",
                fd, (sizeof (fd_set) * CHAR_BIT)) ;
          return NULL ;
        }
#endif

      absHighestFd = fd ;
    }
      
  ep = xcalloc (1, sizeof(struct endpoint_s)) ;

  ep->inBuffer = NULL ;
  ep->inBufferIdx = 0 ;
  ep->inIndex = 0 ;
  ep->inMinLen = 0 ;
  ep->inAmtRead = 0 ;
  ep->inCbk = NULL ;
  ep->inClientData = NULL ;

  ep->outBuffer = 0 ;
  ep->outBufferIdx = 0 ;
  ep->outIndex = 0 ;
  ep->outSize = 0 ;
  ep->outProgressCbk = NULL ;
  ep->outDoneCbk = NULL ;
  ep->outClientData = NULL ;
  ep->outAmtWritten = 0 ;

  ep->workCbk = NULL ;
  ep->workData = NULL ;
  
  ep->myFd = fd ;
  ep->myErrno = 0 ;

  ep->selectHits = 0.0 ;

  endPoints [fd] = ep ;
  priorityList [priorityCount++] = ep ;
  endPointCount++ ;

  highestFd = (fd > highestFd ? fd : highestFd) ;

  return ep ;
}



/* Delete the given endpoint. The files descriptor is closed and the two
   Buffer arrays are released. */

void delEndPoint (EndPoint ep) 
{
  unsigned int idx ;

  if (ep == NULL)
    return ;

  ASSERT (endPoints [ep->myFd] == ep) ;

  if (mainEndPoint == ep)
    mainEndPoint = NULL ;
  
  if (ep->inBuffer != NULL)
    freeBufferArray (ep->inBuffer) ;
  
  if (ep->outBuffer != NULL)
    freeBufferArray (ep->outBuffer) ;

  close (ep->myFd) ;

  /* remove from selectable bits */
  FD_CLR (ep->myFd,&rdSet) ;
  FD_CLR (ep->myFd,&wrSet) ;
  FD_CLR (ep->myFd,&exSet) ;

  /* Adjust the global arrays to account for deleted endpoint. */
  endPoints [ep->myFd] = NULL ;
  if (ep->myFd == highestFd)
    while (endPoints [highestFd] == NULL && highestFd >= 0)
      highestFd-- ;

  for (idx = 0 ; idx < priorityCount ; idx++)
    if (priorityList [idx] == ep)
      break ;

  ASSERT (idx < priorityCount) ; /* i.e. was found */
  ASSERT (priorityList [idx] == ep) ; /* redundant */

  /* this hole will removed in the reorder routine */
  priorityList [idx] = NULL ;

  endPointCount-- ;

  free (ep) ;
}

int endPointFd (EndPoint endp)
{
  ASSERT (endp != NULL) ;

  return endp->myFd ;
}




/* Request a read to be done next time there's data. The endpoint
 * ENDP is what will do the read. BUFF is the Buffer the data should
 * go into. FUNC is the callback function to call when the read is
 * complete. CLIENTDATA is the client data to pass back into the
 * callback function. MINLEN is the minimum amount of data to
 * read. If MINLEN is 0 then then BUFF must be filled, otherwise at
 * least MINLEN bytes must be read.
 *
 * BUFF can be null, in which case no read is actually done, but the
 * callback function will be called still. This is useful for
 * listening sockets.
 *
 * Returns 0 if the read couln't be prepared (for example if a read
 * is already outstanding).
 */

int prepareRead (EndPoint endp,
                 Buffer *buffers,
                 EndpRWCB func,
                 void *clientData,
                 int minlen) 
{
  int bufferSizeTotal = 0 ;
  int idx ;
  
  ASSERT (endp != NULL) ;
  
  if (endp->inBuffer != NULL || FD_ISSET (endp->myFd,&rdSet)) 
    return 0 ;                  /* something already there */

  for (idx = 0 ; buffers != NULL && buffers [idx] != NULL ; idx++)
    {
      size_t bs = bufferSize (buffers [idx]) ;
      size_t bds = bufferDataSize (buffers [idx]) ;
      
      bufferSizeTotal += (bs - bds) ;
    }
  
  endp->inBuffer = buffers ;
  endp->inBufferIdx = 0 ;
  endp->inIndex = 0 ;
  endp->inMinLen = (minlen > 0 ? minlen : bufferSizeTotal) ;
  endp->inCbk = func ;
  endp->inAmtRead = 0 ;
  endp->inClientData = clientData ;

  FD_SET (endp->myFd, &rdSet) ;
  if ( InputFile == NULL )
    FD_SET (endp->myFd, &exSet) ;

  return 1 ;
}



/* Request a write to be done at a later point. ENDP is the EndPoint
 * to do the write. BUFF is the Buffer to write from. FUNC is the
 * function to call when the write is complete. CLIENTDATA is some
 * data to hand back to the callback function.
 *
 * If BUFF is null, then no write will actually by done, but the
 * callback function will still be called. This is useful for
 * connecting sockets.
 *
 * Returns 0 if the write couldn't be prepared (like if a write is
 * still in process.
 */
int prepareWrite (EndPoint endp,
                  Buffer *buffers,
                  EndpRWCB progress,
                  EndpRWCB done,
                  void *clientData) 
{
  int bufferSizeTotal = 0 ;
  int idx ;

  ASSERT (endp != NULL) ;
  
  if (endp->outBuffer != NULL || FD_ISSET (endp->myFd,&wrSet))
    return 0 ;                  /* something already there */

  for (idx = 0 ; buffers != NULL && buffers [idx] != NULL ; idx++)
    bufferSizeTotal += bufferDataSize (buffers [idx]) ;
  
  endp->outBuffer = buffers ;
  endp->outBufferIdx = 0 ;
  endp->outIndex = 0 ;
  endp->outProgressCbk = progress ;
  endp->outDoneCbk = done ;
  endp->outClientData = clientData ;
  endp->outSize = bufferSizeTotal ;
  endp->outAmtWritten = 0 ;

  FD_SET (endp->myFd, &wrSet) ;
  FD_SET (endp->myFd, &exSet) ;

  return 1 ;
}


/* Cancel the pending read. */
void cancelRead (EndPoint endp)
{
  FD_CLR (endp->myFd,&rdSet) ;
  if (!FD_ISSET (endp->myFd, &wrSet))
    FD_CLR (endp->myFd,&exSet) ;

  freeBufferArray (endp->inBuffer) ;
  
  endp->inBuffer = NULL ;
  endp->inBufferIdx = 0 ;
  endp->inIndex = 0 ;
  endp->inMinLen = 0 ;
  endp->inAmtRead = 0 ;
  endp->inCbk = NULL ;
  endp->inClientData = NULL ;
}


/* cancel all pending writes. The first len bytes of the queued write are
  copied to buffer. The number of bytes copied (if it is less than *len) is
  copied to len. If no write was outstanding then len will have 0 stored in
  it. */
void cancelWrite (EndPoint endp, char *buffer UNUSED, size_t *len UNUSED)
{
  FD_CLR (endp->myFd, &wrSet) ;
  if (!FD_ISSET (endp->myFd, &rdSet))
    FD_CLR (endp->myFd, &exSet) ;

#if 0
#error XXX need to copy data to buffer and *len
#endif
  
  freeBufferArray (endp->outBuffer) ;
  
  endp->outBuffer = NULL ;
  endp->outBufferIdx = 0 ;
  endp->outIndex = 0 ;
  endp->outProgressCbk = NULL ;
  endp->outDoneCbk = NULL ;
  endp->outClientData = NULL ;
  endp->outSize = 0 ;
  endp->outAmtWritten = 0 ;
}

/* queue up a new timeout request. to go off at a specific time. */
TimeoutId prepareWake (EndpTCB func, time_t timeToWake, void *clientData) 
{
  TimeoutId id ;
  time_t now = theTime() ;

  if (now > timeToWake)
    return 0 ;
  
  id = timerElemAdd (timeToWake,func,clientData) ;

#if 0
  d_printf (1,"Preparing wake %d at date %ld for %d seconds\n",
           (int) id, (long) now, timeToWake - now) ;
#endif

  return id ;
}


/* queue up a new timeout request to off TIMETOSLEEP seconds from now */
TimeoutId prepareSleep (EndpTCB func, int timeToSleep, void *clientData) 
{
  time_t now = theTime() ;
  TimeoutId id ;
  
  id = timerElemAdd (now + timeToSleep,func,clientData) ;

#if 0
  d_printf (1,"Preparing sleep %d at date %ld for %d seconds\n",
           (int) id, (long) now, timeToSleep) ;
#endif

  return id ;
}


/* Updates a an existing timeout request to go off TIMETOSLEEP seconds from
   now, or queues a new request.  Always returns a new ID. */
TimeoutId updateSleep (TimeoutId tid, EndpTCB func, int timeToSleep,
                       void *clientData) 
{
  if (tid == 0)
    return prepareSleep (func, timeToSleep, clientData) ;
  else
    {
      /* XXX - quick and dirty but CPU wasteful implementation */
      removeTimeout (tid) ;
      return prepareSleep (func, timeToSleep, clientData) ;
    }
}


/* Remove a timeout that was previously prepared. 0 is a legal value that
   is just ignored. */
bool removeTimeout (TimeoutId tid)
{
  TimerElem n = timeoutQueue ;
  TimerElem p = NULL ;

  if (tid == 0)
    return true ;
  
  while (n != NULL && n->id != tid)
    {
      p = n ;
      n = n->next ;
    }

  if (n == NULL)
    return false ;

  if (p == NULL)                /* at the head. */
    timeoutQueue = n->next ;
  else
    p->next = n->next ;

  n->next = timeoutPool ;
  timeoutPool = n ;

  timeoutQueueLength-- ;
  
  return true ;
}


/* The main routine. This is a near-infinite loop that drives the whole
   program. */
void Run (void) 
{
  fd_set rSet ;
  fd_set wSet ;
  fd_set eSet ;
  unsigned long last_summary = 0 ;

  keepSelecting = 1 ;
  xsignal (SIGPIPE, pipeHandler) ;

  while (keepSelecting)
    {
      struct timeval timeout ;
      struct timeval *twait ;
      int sval ;
      unsigned int idx ;
      bool modifiedTime = false ;
      
      twait = getTimeout (&timeout) ;

      memcpy (&rSet,&rdSet,sizeof (rdSet)) ;
      memcpy (&wSet,&wrSet,sizeof (wrSet)) ;
      memcpy (&eSet,&exSet,sizeof (exSet)) ;

      if (highestFd < 0 && twait == NULL) /* no fds and no timeout */
        break ;
      else if (twait != NULL && (twait->tv_sec != 0 || twait->tv_usec != 0))
        {
            /* if we have any workprocs registered we poll rather than
               block on the fds */
          for (idx = 0 ; idx < priorityCount ; idx++)
            if (priorityList [idx] != NULL &&
                priorityList [idx]->workCbk != NULL)
              {
                modifiedTime = true ;
                twait->tv_sec = 0 ;
                twait->tv_usec = 0 ;

                break ;
              }
        }

      /* calculate host backlog statistics */
      TMRstart(TMR_BACKLOGSTATS);
      gCalcHostBlStat ();
      TMRstop(TMR_BACKLOGSTATS);

      TMRstart(TMR_IDLE);
      sval = select (highestFd + 1, &rSet, &wSet, &eSet, twait) ;
      TMRstop(TMR_IDLE);

      timePasses () ;
      if (innconf->timer)
        {
	  unsigned long now = TMRnow () ;
	  if (last_summary == 0 
	      || (long) (now - last_summary) > (innconf->timer * 1000))
	    {
	      TMRsummary ("ME", timer_name) ;
	      last_summary = now;
	    }
	}
      
      if (sval == 0 && twait == NULL)
        die ("No fd's ready and no timeouts") ;
      else if (sval < 0 && errno == EINTR)
        {
	  handleSignals () ;
        }
      else if (sval < 0) 
        {
          syswarn ("ME exception: select failed: %d", sval) ;
          stopRun () ;
        }
      else if (sval > 0)
        {
          IoStatus rval ;
          int readyCount = sval ;
          int endpointsServiced = 1 ;
          
          handleSignals() ;
          
          for (idx = 0 ; idx < priorityCount ; idx++)
            {
              EndPoint ep = priorityList [idx] ;
              bool specialCheck = false ;

              if (readyCount > 0 && ep != NULL) 
                {
                  int fd = ep->myFd ;
                  int selectHit = 0, readMiss = 0, writeMiss = 0 ;

                  /* Every SELECT_RATIO times we service an endpoint in this
                     loop we check to see if the mainEndPoint fd is ready to
                     read or write. If so we process it and do the current
                     endpoint next time around. */
                  if (((endpointsServiced % (SELECT_RATIO + 1)) == 0) &&
                      ep != mainEndPoint && mainEndPoint != NULL &&
                      !mainEpIsReg)
                    {
                      fd_set trSet, twSet ;
                      struct timeval tw ;
                      int checkRead = FD_ISSET (mainEndPoint->myFd,&rdSet) ;
                      int checkWrite = FD_ISSET (mainEndPoint->myFd,&wrSet) ;

                      endpointsServiced++;

                      if (checkRead || checkWrite) 
                        {
                          fd = mainEndPoint->myFd ;

                          tw.tv_sec = tw.tv_usec = 0 ;
                          memset (&trSet,0,sizeof (trSet)) ;
                          memset (&twSet,0,sizeof (twSet)) ;
                      
                          if (checkRead)
                            FD_SET (fd,&trSet) ;
                          if (checkWrite)
                            FD_SET (fd,&twSet) ;

                          sval = select (fd + 1,&trSet,&twSet,0,&tw) ;

                          if (sval > 0)
                            {
                              idx-- ;
                              ep = mainEndPoint ;
                              specialCheck = true ;
                              if (checkRead && FD_ISSET (fd,&trSet))
                                {
                                  FD_SET (fd,&rSet) ;
                                  readyCount++ ;
                                }
                              if (checkWrite && FD_ISSET (fd,&twSet))
                                {
                                  FD_SET (fd,&wSet) ;
                                  readyCount++ ;
                                }
                            }
                          else if (sval < 0)
                            {
                              syswarn ("ME exception: select failed: %d",
                                       sval) ;
                              stopRun () ;
                              return ;
                            }
                          else
                            fd = ep->myFd ; /* back to original fd. */
                        }
                    }

                  FD_CLR (fd, &exSet) ;

                  if (FD_ISSET (fd,&rSet))
                    {
                      readyCount-- ;
                      endpointsServiced++ ;
                      selectHit = 1 ;
                      
                      if ((rval = doRead (ep)) != IoIncomplete)
                        {
                          Buffer *buff = ep->inBuffer ;

                          FD_CLR (fd, &rdSet) ;

                          /* incase callback wants to issue read */
                          ep->inBuffer = NULL ; 
                          
                          if (ep->inCbk != NULL)
                            (*ep->inCbk) (ep,rval,buff,ep->inClientData) ;
                          else
                            freeBufferArray (buff) ;
                        }
                      else
                        {
                          if ( InputFile == NULL )
                            FD_SET (ep->myFd, &exSet) ;
                        }
                    }
                  else if (FD_ISSET(fd,&rdSet))
                    readMiss = 1;

                  /* get it again as the read callback may have deleted the */
                  /* endpoint */
                  if (specialCheck)
                    ep = mainEndPoint ;
                  else
                    ep = priorityList [idx] ;
                  
                  if (readyCount > 0 && ep != NULL && FD_ISSET (fd,&wSet))
                    {
                      readyCount-- ;
                      endpointsServiced++ ;
                      selectHit = 1 ;
                      
                      if ((rval = doWrite (ep)) != IoIncomplete &&
			  rval != IoProgress)
                        {
                          Buffer *buff = ep->outBuffer ;

                          FD_CLR (fd, &wrSet) ;

                          /* incase callback wants to issue a write */
                          ep->outBuffer = NULL ;        
                          
                          if (ep->outDoneCbk != NULL)
                            (*ep->outDoneCbk) (ep,rval,buff,ep->outClientData) ;
                          else
                            freeBufferArray (buff) ;
                        }
                      else if (rval == IoProgress)
                        {
                          Buffer *buff = ep->outBuffer ;

                          if (ep->outProgressCbk != NULL)
                            (*ep->outProgressCbk) (ep,rval,buff,ep->outClientData) ;
                        }
                      else
                        {
                          FD_SET (ep->myFd, &exSet) ;
                        }
                    }
                  else if (FD_ISSET(fd,&wrSet))
                    writeMiss = 1;

                  /* get it again as the write callback may have deleted the */
                  /* endpoint */
                  if (specialCheck)
                    ep = mainEndPoint ;
                  else
                    ep = priorityList [idx] ;

                  if (ep != NULL)
                    {
                      ep->selectHits *= 0.9 ;
                      if (selectHit)
                        ep->selectHits += 1.0 ;
                      else if (readMiss && writeMiss)
                        ep->selectHits -= 1.0 ;
                    }
                    
                  if (readyCount > 0 && ep != NULL && FD_ISSET (fd,&eSet))
                    doExcept (ep) ;
                }
            }
          
          reorderPriorityList () ;
        }
      else if (sval == 0 && !modifiedTime)
        doTimeout () ;

        /* now we're done processing all read fds and/or the
           timeout(s). Next we do the work callbacks for all the endpoints
           whose fds weren't ready. */
      for (idx = 0 ; idx < priorityCount ; idx++)
        {
          EndPoint ep = priorityList [idx] ;

          if (ep != NULL)
            {
              int fd = ep->myFd ;
              
              if ( !FD_ISSET (fd,&rSet) && !FD_ISSET (fd,&wSet) )
                if (ep->workCbk != NULL)
                  {
                    EndpWorkCbk func = ep->workCbk ;
                    void *data = ep->workData ;

                    ep->workCbk = NULL ;
                    ep->workData = NULL ;
                    TMRstart(TMR_CALLBACK);
                    func (ep,data) ;
                    TMRstop(TMR_CALLBACK);
                  }
              
            }
        }
    }
}

void *addWorkCallback (EndPoint endp, EndpWorkCbk cbk, void *data)
{
  void *oldBk = endp->workData ;
  
  endp->workCbk = cbk ;
  endp->workData = data ;

  return oldBk ;
}

/* Tell the Run routine to stop next time around. */
void stopRun (void) 
{
  keepSelecting = 0 ;
}


int endPointErrno (EndPoint endp)
{
  return endp->myErrno ;
}

bool readIsPending (EndPoint endp) 
{
  return (endp->inBuffer != NULL ? true : false) ;
}

bool writeIsPending (EndPoint endp)
{
  return (endp->outBuffer != NULL ? true : false) ;
}

void setMainEndPoint (EndPoint endp)
{
  struct stat buf ;

  mainEndPoint = endp ;
  if (endp->myFd >= 0 && fstat (endp->myFd,&buf) < 0)
    syslog (LOG_ERR,"Can't fstat mainEndPoint fd (%d): %m", endp->myFd) ;
  else if (endp->myFd < 0)
    syslog (LOG_ERR,"Negative fd for mainEndPoint???") ;
  else
    mainEpIsReg = (S_ISREG(buf.st_mode) ? true : false) ;
}

int getMainEndPointFd (void)
{
  return(mainEndPoint->myFd) ;
}

void freeTimeoutQueue (void)
{
  TimerElem p, n ;

  p = timeoutQueue ;
  while (p)
    {
      n = p->next ;
      p->next = timeoutPool ;
      timeoutPool = p;
      p = n ;
      timeoutQueueLength-- ;
    }
}


/***********************************************************************/
/*                      STATIC FUNCTIONS BELOW HERE                    */
/***********************************************************************/


/*
 * called when the file descriptor on this endpoint is read ready.
 */
static IoStatus doRead (EndPoint endp) 
{
  int i = 0 ;
  unsigned int idx ;
  unsigned int bCount = 0 ;
  struct iovec *vp = NULL ;
  Buffer *buffers = endp->inBuffer ;
  unsigned int currIdx = endp->inBufferIdx ;
  size_t amt = 0 ;
  IoStatus rval = IoIncomplete ;

  TMRstart(TMR_READ);
  for (i = currIdx ; buffers && buffers [i] != NULL ; i++)
    bCount++ ;

  bCount = (bCount > IOV_MAX ? IOV_MAX : bCount) ;

  i = 0 ;

  /* now set up the iovecs for the readv */
  if (bCount > 0)
    {
      char *bbase ;
      size_t bds, bs ;

      vp = xcalloc (bCount, sizeof(struct iovec)) ;

      bbase = bufferBase (buffers[currIdx]) ;
      bds = bufferDataSize (buffers[currIdx]) ;
      bs = bufferSize (buffers [currIdx]) ;

      /* inIndex is an index in the virtual array of the read, not directly
         into the buffer. */
      vp[0].iov_base = bbase + bds + endp->inIndex ;
      vp[0].iov_len = bs - bds - endp->inIndex ;

      amt = vp[0].iov_len ;
      
      for (idx = currIdx + 1 ; idx < bCount ; idx++)
        {
          bbase = bufferBase (buffers[idx]) ;
          bds = bufferDataSize (buffers[idx]) ;
          bs = bufferSize (buffers [idx]) ;
      
          vp [idx].iov_base = bbase ;
          vp [idx].iov_len = bs - bds ;
          amt += (bs - bds) ;
        }

      i = readv (endp->myFd,vp,(int) bCount) ;

      if (i > 0)
        {
          size_t readAmt = (size_t) i ;
            
          endp->inAmtRead += readAmt ;
          
          /* check if we filled the first buffer */
          if (readAmt >= (size_t) vp[0].iov_len)
            {                   /* we did */
              bufferIncrDataSize (buffers[currIdx], vp[0].iov_len) ;
              readAmt -= vp [0].iov_len ;
              endp->inBufferIdx++ ;
            }
          else
            {
              endp->inIndex += readAmt ;
              bufferIncrDataSize (buffers[currIdx], readAmt) ;
              readAmt = 0 ;
            }
          
          /* now check the rest of the buffers */
          for (idx = 1 ; readAmt > 0 ; idx++)
            {
              ASSERT (idx < bCount) ;

              bs = bufferSize (buffers [currIdx + idx]) ;
              bbase = bufferBase (buffers [currIdx + idx]) ;
              bds = bufferDataSize (buffers [currIdx + idx]) ;
              
              if (readAmt >= (bs - bds))
                {
                  bufferSetDataSize (buffers [currIdx + idx],bs) ;
                  readAmt -= bs ;
                  endp->inBufferIdx++ ;
                }
              else
                {
                  endp->inIndex = readAmt ;
                  bufferIncrDataSize (buffers [currIdx + idx], readAmt) ;
                  memset (bbase + bds + readAmt, 0, bs - bds - readAmt) ;
                  readAmt = 0 ;
                }
            }

          if (endp->inAmtRead >= endp->inMinLen)
            {
              endp->inIndex = 0 ;
              rval = IoDone ;
            }
        }
      else if (i < 0 && errno != EINTR && errno != EAGAIN)
        {
          endp->myErrno = errno ;
          rval = IoFailed ;
        }
      else if (i < 0 && errno == EINTR)
        {
	  handleSignals () ;
        }
      else if (i == 0)
        rval = IoEOF ;
      else                   /* i < 0 && errno == EAGAIN */
        rval = IoIncomplete ;
      
      free (vp) ;
    }
  else
    rval = IoDone ;
  TMRstop(TMR_READ);
  return rval ;
}

/* called when the file descriptor on the endpoint is write ready. */
static IoStatus doWrite (EndPoint endp)
{
  unsigned int idx ;
  int i = 0 ;
  size_t amt = 0 ;
  unsigned int bCount = 0 ;
  struct iovec *vp = NULL ;
  Buffer *buffers = endp->outBuffer ;
  unsigned int currIdx = endp->outBufferIdx ;
  IoStatus rval = IoIncomplete ;
  
  TMRstart(TMR_WRITE);
  for (i = currIdx ; buffers && buffers [i] != NULL ; i++)
    bCount++ ;

  bCount = (bCount > IOV_MAX ? IOV_MAX : bCount) ;

  i = 0 ;
  
  if (bCount > 0)
    {
      vp = xcalloc (bCount, sizeof(struct iovec)) ;

      vp[0].iov_base = bufferBase (buffers [currIdx]) ;
      vp[0].iov_base = (char *) vp[0].iov_base + endp->outIndex ;
      vp[0].iov_len = bufferDataSize (buffers [currIdx]) - endp->outIndex ;

      amt = vp[0].iov_len ;
      
      for (idx = 1 ; idx < bCount ; idx++)
        {
          vp [idx].iov_base = bufferBase (buffers [idx + currIdx]) ;
          vp [idx].iov_len = bufferDataSize (buffers [idx + currIdx]) ;
          amt += vp[idx].iov_len ;
        }

#if 1
      if (debugWrites) 
        {
          /* nasty mixing, but stderr is unbuffered usually. It's debugging only */
          d_printf (5,"About to write this:================================\n") ;
          writev (2,vp,bCount) ;
          d_printf (5,"end=================================================\n") ;
        }
      
#endif

      ASSERT (endp->myFd >= 0) ;
      ASSERT (vp != 0) ;
      ASSERT (bCount > 0) ;
      
      i = writev (endp->myFd,vp,(int) bCount) ;

      if (i > 0)
        {
          size_t writeAmt = (size_t) i ;
          
          endp->outAmtWritten += writeAmt ;

          /* now figure out which buffers got completely written */
          for (idx = 0 ; writeAmt > 0 ; idx++)
            {
              if (writeAmt >= (size_t) vp[idx].iov_len)
                {
                  endp->outBufferIdx++ ;
                  endp->outIndex = 0 ;
                  writeAmt -= vp [idx].iov_len ;
                }
              else
                {
                  /* this buffer was not completly written */
                  endp->outIndex += writeAmt ;
                  writeAmt = 0 ;
                }
            }

          if (endp->outAmtWritten == endp->outSize)
            rval = IoDone ;
	  else
            rval = IoProgress ;
        }
      else if (i < 0 && errno == EINTR)
        {
	  handleSignals () ;
        }
      else if (i < 0 && errno == EAGAIN)
        {
          rval = IoIncomplete ;
        }
      else if (i < 0)
        {
          endp->myErrno = errno ;
          rval = IoFailed ;
        }
      else
        d_printf (1,"Wrote 0 bytes in doWrite()?\n") ;

      free (vp) ;
    }
  else
    rval = IoDone ;

  TMRstop(TMR_WRITE);
  return rval ;
}


static IoStatus doExcept (EndPoint endp)
{
  int optval;
  socklen_t size ;
  int fd = endPointFd (endp) ;

  if (getsockopt (fd, SOL_SOCKET, SO_ERROR,
                  (char *) &optval, &size) != 0)
    syswarn ("ME exception: getsockopt (%d)", fd) ;
  else if (optval != 0)
    {
      errno = optval ;
      syswarn ("ME exception: fd %d", fd) ;
    }
  else
    syswarn ("ME exception: fd %d: Unknown error", fd) ;

#if 0
  sleep (5) ;
  abort () ;
#endif

  /* Not reached */
  return IoFailed ;
}

#if 0
static void endPointPrint (EndPoint ep, FILE *fp)
{
  fprintf (fp,"EndPoint [%p]: fd [%d]\n",(void *) ep, ep->myFd) ;
}
#endif

static void signalHandler (int s)
{
  sigFlags[s] = 1 ;
#ifndef HAVE_SIGACTION
  xsignal (s, signalHandler) ;
#endif
}


static void pipeHandler (int s)
{
  xsignal (s, pipeHandler) ;
}


/* compare the hit ratio of two endpoint for qsort. We're sorting the
   endpoints on their relative activity */
static int hitCompare (const void *v1, const void *v2)
{
  const struct endpoint_s *e1 = *((const struct endpoint_s * const *) v1) ;
  const struct endpoint_s *e2 = *((const struct endpoint_s * const *) v2) ;
  double e1Hit = e1->selectHits ;
  double e2Hit = e2->selectHits ;

  if (e1 == mainEndPoint)
    return -1 ;
  else if (e2 == mainEndPoint)
    return 1 ;
  else if (e1Hit < e2Hit)
    return 1 ;
  else if (e1Hit > e2Hit)
    return -1 ;

  return 0 ;
}



/* We maintain the endpoints in order of the percent times they're ready
   for read/write when they've been selected. This helps us favour the more
   active endpoints. */
static void reorderPriorityList (void)
{
  unsigned int i, j ;
  static int thisTime = 4;

  /* only sort every 4th time since it's so expensive */
  if (--thisTime > 0)
    return ;

  thisTime = 4;

  for (i = j = 0; i < priorityCount; i++)
    if (priorityList [i] != NULL)
      {
        if (i != j)
          priorityList [j] = priorityList [i] ;
        j++ ;
      }

  for (i = j; i < priorityCount; i++)
    priorityList [ i ] = NULL;

  priorityCount = j;

  qsort (priorityList, (size_t)priorityCount, sizeof (EndPoint), &hitCompare);
}


#define TIMEOUT_POOL_SIZE ((4096 - 2 * (sizeof (void *))) / (sizeof (TimerElemStruct)))

/* create a new timeout data structure properly initialized. */
static TimerElem newTimerElem (TimeoutId i, time_t w, EndpTCB f, void *d)
{
  TimerElem p ;

  if (timeoutPool == NULL)
    {
      unsigned int j ;

      timeoutPool = xmalloc (sizeof(TimerElemStruct) * TIMEOUT_POOL_SIZE) ;

      for (j = 0; j < TIMEOUT_POOL_SIZE - 1; j++)
        timeoutPool[j] . next = &(timeoutPool [j + 1]) ;
      timeoutPool [TIMEOUT_POOL_SIZE-1] . next = NULL ;
    }

  p = timeoutPool ;
  timeoutPool = timeoutPool->next ;

  ASSERT (p != NULL) ;
  
  p->id = i ;
  p->when = w ;
  p->func = f ;
  p->data = d ;
  p->next = NULL ;

  return p ;
}



/* add a new timeout structure to the global list. */
static TimeoutId timerElemAdd (time_t when, EndpTCB func, void *data)
{
  TimerElem p = newTimerElem (++nextId ? nextId : ++nextId,when,func,data) ;
  TimerElem n = timeoutQueue ;
  TimerElem q = NULL ;
  
  while (n != NULL && n->when <= when)
    {
      q = n ;
      n = n->next ;
    }

  if (n == NULL && q == NULL)   /* empty list so put at head */
    timeoutQueue = p ;
  else if (q == NULL)           /* put at head of list */
    {
      p->next = timeoutQueue ;
      timeoutQueue = p ;
    }
  else if (n == NULL)           /* put at end of list */
    q->next = p ;
  else                          /* in middle of list */
    {
      p->next = q->next ;
      q->next = p ;
    }

  timeoutQueueLength++ ;
  
  return p->id ;
}


/* Fills in TOUT with the timeout to use on the next call to
 * select. Returns TOUT. If there is no timeout, then returns NULL.  If the
 * timeout has already passed, then it calls the timeout handling routine
 * first.
 */
static struct timeval *getTimeout (struct timeval *tout)
{
  struct timeval *rval = NULL ;
  
  if (timeoutQueue != NULL)
    {
      time_t now = theTime() ;

      while (timeoutQueue && now > timeoutQueue->when)
        doTimeout () ;
          
      if (timeoutQueue != NULL && now == timeoutQueue->when)
        {
          tout->tv_sec = 0 ;
          tout->tv_usec = 0 ;
          rval = tout ;
        }
      else if (timeoutQueue != NULL)
        {
          tout->tv_sec = timeoutQueue->when - now ;
          tout->tv_usec = 0 ;
          rval = tout ;
        }
    }

  return rval ;
}

      
  


  
static void doTimeout (void)
{
  EndpTCB cbk = timeoutQueue->func ;
  void *data = timeoutQueue->data ;
  TimerElem p = timeoutQueue ;
  TimeoutId tid = timeoutQueue->id ;

  timeoutQueue = timeoutQueue->next ;

  p->next = timeoutPool ;
  timeoutPool = p ;

  timeoutQueueLength-- ;
  
  if (cbk)
    (*cbk) (tid, data) ;        /* call the callback function */
}





#if defined (WANT_MAIN)


#define BUFF_SIZE 100


void timerCallback (void *cd) ;
void timerCallback (void *cd)
{
  d_printf (1,"Callback \n") ;
}

  
void lineIsWritten (EndPoint ep, IoStatus status, Buffer *buffer, void *data);
void lineIsWritten (EndPoint ep, IoStatus status, Buffer *buffer, void *data)
{
  int i ;
  
  if (status == IoDone)
    d_printf (1,"LINE was written\n") ;
  else
    {
      int oldErrno = errno ;
      
      errno = endPointErrno (ep) ;
      perror ("write failed") ;
      errno = oldErrno ;
    }

  for (i = 0 ; buffer && buffer [i] ; i++)
    delBuffer (buffer [i]) ;
}

void lineIsRead (EndPoint myEp, IoStatus status, Buffer *buffer, void *data);
void lineIsRead (EndPoint myEp, IoStatus status, Buffer *buffer, void *d)
{
  Buffer *writeBuffers, *readBuffers ;
  Buffer newBuff1, newBuff2 ;
  Buffer newInputBuffer ;
  char *data, *p ;
  size_t len ;

  if (status == IoFailed)
    {
      int oldErrno = errno ;

      errno = endPointErrno (myEp) ;
      perror ("read failed") ;
      errno = oldErrno ;

      return ;
    }
  else if (status == IoEOF)
    {
      d_printf (1,"EOF on endpoint.\n") ;
      delEndPoint (myEp) ;

      return ;
    }
  
  
  data = bufferBase (buffer[0]) ;
  len = bufferDataSize (buffer[0]) ;
  
  if (data [len - 1] == '\r' || data [len - 1] == '\n')
    bufferDecrDataSize (buffer [0],1) ;
  if (data [len - 1] == '\r' || data [len - 1] == '\n')
    bufferDecrDataSize (buffer [0],1) ;

  data [len] = '\0' ;
  
  d_printf (1,"Got a line: %s\n", data) ;

  newBuff1 = newBuffer (len + 50) ;
  newBuff2 = newBuffer (len + 50) ;
  newInputBuffer = newBuffer (BUFF_SIZE) ;
  
  p = bufferBase (newBuff1) ; 
  strcpy (p, "Thanks for that \"") ;
  bufferSetDataSize (newBuff1,strlen (p)) ;
  
  p = bufferBase (newBuff2) ;
  strcpy (p,"\" very tasty\n") ;
  bufferSetDataSize (newBuff2,strlen (p)) ;

  writeBuffers = makeBufferArray (newBuff1,buffer[0],newBuff2,NULL) ;
  readBuffers = makeBufferArray (newInputBuffer,NULL) ;
  
  prepareWrite (myEp,writeBuffers,lineIsWritten,NULL) ;
  prepareRead (myEp,readBuffers,lineIsRead,NULL,1) ;

#if 0
  myEp->registerWake (&timerCallback,theTime() + 7,0) ;
#endif
}


static void printDate (TimeoutId tid, void *data) ;
static void printDate (TimeoutId tid, void *data)
{
  time_t t ;

  t = theTime() ;
  
  d_printf (1,"Timeout (%d) time now is %ld %s",
           (int) tid,(long) t,ctime(&t)) ;

  if (timeoutQueue == NULL) 
    {
      int ti = (rand () % 10) + 1 ;

      prepareSleep (printDate,ti,data) ;
    }
}

TimeoutId rm ;

static void Timeout (TimeoutId tid, void *data) ;
static void Timeout (TimeoutId tid, void *data)
{
  static int seeded ;
  static int howMany ;
  static int i ;
  time_t t = theTime() ;

  if ( !seeded )
    {
      srand (t) ;
      seeded = 1 ;
    }

  d_printf (1,"Timeout (%d) time now is %ld %s",
           (int) tid, (long) t,ctime(&t)) ;
  
  if (timeoutQueue != NULL && timeoutQueue->next != NULL)
    d_printf (1,"%s timeout id %d\n",
             (removeTimeout (rm) ? "REMOVED" : "FAILED TO REMOVE"), rm) ;
  rm = 0 ;
  
  howMany = (rand() % 10) + (timeoutQueue == NULL ? 1 : 0) ;

  for (i = 0 ; i < howMany ; i++ )
    {
      TimeoutId id ;
      int count = (rand () % 30) + 1 ;

      id = (i % 2 == 0 ? prepareSleep (Timeout,count,data)
            : prepareWake (Timeout,t + count,data)) ;

      if (rm == 0)
        rm = id ;
    }
}


void newConn (EndPoint ep, IoStatus status, Buffer *buffer, void *d) ;
void newConn (EndPoint ep, IoStatus status, Buffer *buffer, void *d)
{
  EndPoint newEp ;
  struct sockaddr_in in ;
  Buffer *readBuffers ;
  Buffer newBuff = newBuffer (BUFF_SIZE) ;
  int len = sizeof (in) ;
  int fd ;

  memset (&in, 0, sizeof (in)) ;
  
  fd = accept (ep->myFd, (struct sockaddr *) &in, &len) ;

  if (fd < 0)
    {
      perror ("::accept") ;
      return ;
    }
  
  newEp = newEndPoint (fd) ;

  prepareRead (ep, NULL, newConn,NULL,0) ;

  readBuffers = makeBufferArray (newBuff,NULL) ;

  prepareRead (newEp, readBuffers, lineIsRead, NULL, 1) ;

  d_printf (1,"Set up a new connection\n");
}


int main (int argc, char **argv)
{
  EndPoint accConn ;
  struct sockaddr_in accNet ;
  int accFd = socket (AF_INET,SOCK_STREAM,0) ;
  unsigned short port = atoi (argc > 1 ? argv[1] : "10000") ;
  time_t t = theTime() ;


  program = strrchr (argv[0],'/') ;

  if (!program)
    program = argv [0] ;
  else
    program++ ;

  ASSERT (accFd >= 0) ;

  memset (&accNet,0,sizeof (accNet)) ;
  accNet.sin_family = AF_INET ;
  accNet.sin_addr.s_addr = htonl (INADDR_ANY) ;
  accNet.sin_port = htons (port) ;

#ifdef LOG_PERROR
  openlog (program, LOG_PERROR | LOG_PID, LOG_NEWS) ;
#else
  openlog (program, LOG_PID, LOG_NEWS) ;
#endif
  
  if (bind (accFd, (struct sockaddr *) &accNet, sizeof (accNet)) < 0)
    {
      perror ("bind: %m") ;
      exit (1) ;
    }

  listen (accFd,5) ;
  
  accConn = newEndPoint (accFd) ;

  prepareRead (accConn,NULL,newConn,NULL,0) ;

  prepareSleep (Timeout,5,(void *) 0x10) ;

  t = theTime() ;
  d_printf (1,"Time now is %s",ctime(&t)) ;
  
  prepareWake (printDate,t + 16,NULL) ;

  Run () ;

  return 0;
}
#endif /* WANT_MAIN */

/* Probably doesn't do the right thing for SIGCHLD */
void setSigHandler (int signum, void (*ptr)(int))
{
  unsigned int i ;

  if (sigHandlers == NULL)
    {
      sigHandlers = xmalloc (sizeof(sigfn) * NSIG) ;
      sigFlags = xmalloc (sizeof(int) * NSIG) ;
      for (i = 0 ; i < NSIG ; i++)
        {
          sigHandlers [i] = NULL ;
          sigFlags [i] = 0 ;
        }
    }

  if (signum >= NSIG)
    {
      syslog (LOG_ERR,"ME signal number bigger than NSIG: %d vs %d",
              signum,NSIG) ;
      return ;
    }

  if (xsignal (signum, signalHandler) == SIG_ERR)
    die ("signal failed: %s", strerror(errno)) ;

  sigHandlers[signum] = ptr ;
}

static void handleSignals (void)
{
  int i ;
#if defined(USE_SIGVEC)
  int mask ;
#endif

  for (i = 1; i < NSIG; i++)
    {
      if (sigFlags[i])
        {
#if defined(USE_SIGACTION)
          sigset_t set, oset ;
      
          if (sigemptyset (&set) != 0 || sigaddset (&set, i) != 0)
            die ("sigemptyset or sigaddset failed") ;
          if (sigprocmask (SIG_BLOCK, &set, &oset) != 0)
            die ("sigprocmask failed: %s", strerror(errno)) ;
#elif defined(USE_SIGVEC)
# ifndef sigmask
#  define sigmask(s)    (1 << ((s) - 1))
# endif
          int mask ;
          
          mask = sigblock (sigmask(i)) ;
#elif defined(USE_SIGSET)
          if (sighold (i) != 0)
            die ("sighold failed: %s", strerror(errno)) ;
#else
      /* hope for the best */
#endif

          sigFlags[i] = 0;

          if (sigHandlers[i] != NULL &&
              sigHandlers[i] != SIG_IGN &&
              sigHandlers[i] != SIG_DFL)
            (sigHandlers[i])(i) ;
            
#if defined(USE_SIGACTION)
          if (sigprocmask (SIG_SETMASK, &oset, (sigset_t *)NULL) != 0)
            die ("sigprocmask failed: %s", strerror(errno)) ;
#elif defined(USE_SIGVEC)
          sigsetmask (mask) ;
#elif defined(USE_SIGSET)
          if (sigrelse (i) != 0)
            die ("sigrelse failed: %s", strerror(errno)) ;
#else
          /* hope for the best */
#endif
        }
    }
}


int endpointConfigLoadCbk (void *data)
{
  FILE *fp = (FILE *) data ;
  long ival ;
  int rval = 1 ;

  if (getInteger (topScope,"stdio-fdmax",&ival,NO_INHERIT))
    {
      stdioFdMax = ival ;

#if ! defined (FD_SETSIZE)

      if (stdioFdMax > 0)
        {
          logOrPrint (LOG_ERR,fp,NO_STDIO_FDMAX) ;
          stdioFdMax = 0 ;
          rval = 0 ;
        }

#else

      if (stdioFdMax > FD_SETSIZE)
        {
          logOrPrint (LOG_ERR,fp,
                      "ME config: value of %s (%ld) in %s is higher"
                      " than maximum of %ld. Using %ld","stdio-fdmax",
                      ival,"global scope",
                      (long) FD_SETSIZE, (long) FD_SETSIZE) ;
          stdioFdMax = FD_SETSIZE ;
          rval = 0 ;
        }
      
#endif
      
    }
  else
    stdioFdMax = 0 ;

  return rval ;
}



#if 0
/* definitely not the fastest, but the most portable way to find the first
  set bit in a mask  */
static int ff_set (fd_set *set,unsigned int start)
{
  unsigned int i ;

  for (i = start ; i < FD_SETSIZE ; i++)
    if (FD_ISSET (i,set))
      return (int) i ;

  return -1 ;
}


static int ff_free (fd_set *set, unsigned int start)
{
  unsigned int i ;

  for (i = start ; i < FD_SETSIZE ; i++)
    if (!FD_ISSET (i,set))
      return i ;


  return -1 ;
}
#endif


static void endpointCleanup (void)
{
  free (endPoints) ;
  free (priorityList) ;
  free (sigHandlers) ;
  endPoints = NULL ;
  priorityList = NULL ;
  sigHandlers = NULL ;
}

/*
**  Copyright 1996 by the Internet Software Consortium
**
**  Permission to use, copy, modify, and distribute this software for any
**  purpose with or without fee is hereby granted, provided that the above
**  copyright notice and this permission notice appear in all copies.
**
**  THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
**  DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
**  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
**  INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT,
**  OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
**  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
**  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
**  PERFORMANCE OF THIS SOFTWARE.
*/
