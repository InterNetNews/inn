/*  $Id$
**
**  The implementation of the innfeed InnListener class.
**
**  Written by James Brister <brister@vix.com>
*/

#include "innfeed.h"
#include "config.h"
#include "clibrary.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <time.h>

#include "inn/messages.h"
#include "inn/libinn.h"

#include "article.h"
#include "buffer.h"
#include "configfile.h"
#include "endpoint.h"
#include "host.h"
#include "innlistener.h"
#include "nntp.h"
#include "tape.h"

#define LISTENER_INPUT_BUFFER (1024 * 8) /* byte size of the input buffer */
#define EOF_SLEEP_TIME 1	/* seconds to sleep when EOF on InputFile */

struct innlistener_s 
{
    EndPoint myep ;

    Host *myHosts ;
    size_t hostLen ;
    Buffer inputBuffer ;
    bool dummyListener ;
    bool dynamicPeers ;
    TimeoutId inputEOFSleepId ;

    InnListener next ;
};

static unsigned int listenerCount = 0 ;
static InnListener listenerList = NULL ;

InnListener mainListener ;

static FILE *droppedFp = NULL ;
static long droppedCount = 0 ;
static int droppedFileCount = 0 ;
static char *dropArtFile = NULL ;
static bool fastExit = false ;

extern const char *pidFile ;
extern const char *InputFile ;
extern bool RollInputFile ;
extern bool genHtml ;


static void giveArticleToPeer (InnListener lis,
                               Article article, const char *peerName) ;
static void newArticleCommand (EndPoint ep, IoStatus i,
                               Buffer *buffs, void *data) ;
static void wakeUp (TimeoutId id, void *data) ;
static void writeCheckPoint (int offsetAdjust) ;
static void dropArticle (const char *peer, Article article) ;
static void listenerCleanup (void) ;

static bool inited = false ;


void listenerLogStatus (FILE *fp)
{
  fprintf (fp,"%sListener Status:%s\n",
           genHtml ? "<B>" : "", genHtml ? "</B>" : "") ;
  fprintf (fp,"    Dropped article file: %s\n",dropArtFile) ;
  fprintf (fp,"   Dropped article count: %ld\n",(long) droppedCount) ;
  fprintf (fp,"\n") ;
}

InnListener newListener (EndPoint endp, bool isDummy, bool dynamicPeers)
{
  InnListener l = xcalloc (1, sizeof(struct innlistener_s)) ;
  Buffer *readArray ;

  if (!inited)
    {
      inited = true ;
      atexit (listenerCleanup) ;
    }
  
  l->myep = endp ;

  l->hostLen = MAX_HOSTS ;
  l->myHosts = xcalloc (l->hostLen, sizeof(Host)) ;

  l->inputBuffer = newBuffer (LISTENER_INPUT_BUFFER) ;
  l->dummyListener = isDummy ;
  l->dynamicPeers = dynamicPeers ;

  addPointerFreedOnExit ((char *)bufferBase(l->inputBuffer)) ;
  addPointerFreedOnExit ((char *)l->myHosts) ;
  addPointerFreedOnExit ((char *)l) ;

  readArray = makeBufferArray (bufferTakeRef (l->inputBuffer), NULL) ;
  prepareRead (endp,readArray,newArticleCommand,l,1) ;

  l->next = listenerList ;
  listenerList = l ;

  listenerCount++ ;

  return l ;
}

void gPrintListenerInfo (FILE *fp, unsigned int indentAmt)
{
  InnListener p ;
  char indent [INDENT_BUFFER_SIZE] ;
  unsigned int i ;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sGlobal InnListener list : %p (count %d) {\n",
           indent,(void *) listenerList,listenerCount) ;
  for (p = listenerList ; p != NULL ; p = p->next)
    printListenerInfo (p,fp,indentAmt + INDENT_INCR) ;
  fprintf (fp,"%s}\n",indent) ;
}



void printListenerInfo (InnListener listener, FILE *fp, unsigned int indentAmt)
{
  char indent [INDENT_BUFFER_SIZE] ;
  unsigned int i ;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sInnListener : %p {\n",indent,(void *) listener) ;
  fprintf (fp,"%s    endpoint : %p\n", indent,(void *) listener->myep) ;
  fprintf (fp,"%s    dummy-listener : %s\n",indent,
           boolToString (listener->dummyListener)) ;
  fprintf (fp,"%s    dynamicPeers : %s\n",indent,
           boolToString (listener->dynamicPeers)) ;

  fprintf (fp,"%s    input-buffer {\n",indent) ;
  printBufferInfo (listener->inputBuffer,fp,indentAmt + INDENT_INCR) ;
  fprintf (fp,"%s    }\n",indent) ;

  fprintf (fp,"%s    hosts {\n",indent) ;
  for (i = 0 ; i < listener->hostLen ; i++)
    {
#if 0
      if (listener->myHosts [i] != NULL)
        printHostInfo (listener->myHosts [i],fp,indentAmt + INDENT_INCR) ;
#else
      fprintf (fp,"%s        %p\n",indent,(void *) listener->myHosts[i]) ;
#endif
    }
  
  fprintf (fp,"%s    }\n",indent) ;
  
  fprintf (fp,"%s}\n",indent) ;
}

  /* Unlink the pidFile if and only if it is our pidFile.
     There is still a racecondition here but very small. */
static void unlinkPidFile (void)
{
  FILE *fp;
  char buf[32];

  if ((fp = fopen(pidFile, "r")) == NULL)
    return;

  if (fgets(buf, 32, fp) != NULL && atoi(buf) == getpid())
    unlink(pidFile);
  fclose(fp);
}

  /* Close down all hosts on this listener. When they're all gone the
     Listener will be deleted. */
void shutDown (InnListener l)
{
  unsigned int i ;
  unsigned int count ;

  d_printf (1,"Shutting down the listener\n") ;

  /* When shutting down the mainListener, stop writing to the
     StatusFile and remove our pidFile. */
  if (l == mainListener)
    {
      /*hostCleanup (); should do this but .. */
      hostSetStatusFile ("/dev/null");
      unlinkPidFile();
    }

  closeDroppedArticleFile () ;
  
  if (l->myep != NULL)
    {
      if (l->inputEOFSleepId != 0)
	removeTimeout (l->inputEOFSleepId) ;
      l->inputEOFSleepId = 0 ;
      delEndPoint (l->myep) ;
    }
  l->myep = NULL ;
  
  for (i = 0, count = 0 ; i < l->hostLen ; i++)
    if (l->myHosts [i] != NULL) 
      {
        hostClose (l->myHosts[i]) ;
        count++ ;
      }

  if (count == 0 || fastExit)
    {
      time_t now = theTime () ;
      char dateString [30] ;

      gHostStats();
      strlcpy (dateString,ctime (&now),sizeof (dateString)) ;
      dateString [24] = '\0' ;

      if (fastExit)
        notice ("ME finishing (quickly) at %s", dateString) ;
      else
        notice ("ME finishing at %s", dateString) ;

      unlinkPidFile();
      exit (0) ;
    }
}


bool listenerAddPeer (InnListener listener, Host hostObj)
{
  unsigned int i ;

  d_printf (1,"Adding peer: %s\n", hostPeerName (hostObj)) ;
  
  for (i = 0 ; i < listener->hostLen ; i++)
    {
      if (listener->myHosts [i] == NULL)
        {
          listener->myHosts [i] = hostObj ;

          return true ;
        }
    }

  return false ;
}


/* return true if this listener doesn't ever generate articles. */
bool listenerIsDummy (InnListener listener)
{
  return listener->dummyListener ;
}

/* Called by the Host when it (the Host) is about to delete itself. */
unsigned int listenerHostGone (InnListener listener, Host host)
{
  unsigned int i ;
  unsigned int someThere = 0 ;

  d_printf (1,"Host is gone: %s\n", hostPeerName (host)) ;
  
  for (i = 0 ; i < listener->hostLen ; i++)
    if (listener->myHosts [i] == host)
      listener->myHosts [i] = NULL ;
    else if (listener->myHosts [i] != NULL)
      someThere++ ;

  return someThere ;
}


/* called by the Host when it has nothing to do. */
void listenerHostIsIdle (InnListener listener, Host host)
{
  ASSERT (listener != NULL) ;
  ASSERT (host != NULL) ;

  d_printf (1,"Host is idle: %s\n", hostPeerName (host)) ;
  
  if (!listener->dummyListener)
    return ;

  /* if this listener is a dummy (i.e. not generating articles cause we're
     just dealing with backlog files) then forget about the host and when
     last one is gone we exit. */

  hostClose (host) ;
}


void openInputFile (void)
{
  int fd, i, mainFd ;
  off_t offset ;
  char buf [32], *p ;

  ASSERT (InputFile && *InputFile) ;

  fd = open(InputFile, O_RDWR) ;
  if (fd < 0)
    die ("open %s: %s\n", InputFile, strerror(errno)) ;

  mainFd = getMainEndPointFd() ;
  if (fd != mainFd)
    {
      if (dup2(fd, mainFd) < 0)
	die ("dup2 %d %d: %s\n", fd, mainFd, strerror(errno)) ;
      close (fd);
    }

  i = read(mainFd, buf, sizeof (buf)) ;
  if (i < 0)
    die ("read %s: %s\n", InputFile, strerror(errno)) ;
  else if (i > 0)
    {
      p = buf;
      buf [ sizeof(buf) - 1 ] = '\0';
      offset = (off_t) strtol (p, &p, 10) ;
      if (offset > 0 && *p == '\n')
	lseek (mainFd, offset, SEEK_SET) ;
      else
	lseek (mainFd, 0, SEEK_SET) ;
    }
  syslog(LOG_NOTICE, "ME opened %s", InputFile);
}


int listenerConfigLoadCbk (void *data UNUSED)
{
  int bval ;

  if (getBool (topScope,"fast-exit",&bval,NO_INHERIT))
    fastExit = (bval ? true : false) ;

  return 1 ;
}

/**********************************************************************/
/**                     STATIC PRIVATE FUNCTIONS                     **/
/**********************************************************************/


/* EndPoint callback function for when the InnListener's fd is ready for
   reading. */
static void newArticleCommand (EndPoint ep, IoStatus i,
                               Buffer *buffs, void *data)
{
  InnListener lis = (InnListener) data ;
  char *msgid, *msgidEnd ;
  char *fileName, *fileNameEnd ;
  char *peer, *peerEnd ;
  char *cmd, *endc ;
  char *bbase = bufferBase (buffs [0]) ;
  size_t blen = bufferDataSize (buffs [0]) ;
  Buffer *readArray ;
  static int checkPointCounter ;
  char *s;

  ASSERT (ep == lis->myep) ;

  bufferAddNullByte  (buffs [0]) ;
  
  if (i == IoEOF)
    {
      if ( lis == mainListener && InputFile != NULL )
	{
	  if ( RollInputFile )
	    {
	      syslog(LOG_NOTICE, "ME reached EOF in %s", InputFile);
	      openInputFile () ;
	      RollInputFile = false ;
	      readArray = makeBufferArray (bufferTakeRef (buffs [0]),NULL) ;
	      prepareRead (ep, readArray, newArticleCommand, data, 1) ;
	    }
	  else
	    {
	      lis->inputEOFSleepId =
		prepareSleep (wakeUp, EOF_SLEEP_TIME, data) ;

	    }
	}
      else 
        {
          d_printf (1,"Got EOF on listener\n") ;
          notice ("ME source lost . Exiting");
	  shutDown (lis) ;
        }
    }
  else if (i == IoFailed)
    {
      errno = endPointErrno (ep) ;
#if HAVE_SOCKETPAIR
      if (errno != ECONNABORTED)
#endif
      syswarn ("ME source read error, exiting") ;
      d_printf (1,"Got IO Error on listener\n") ;
      shutDown (lis) ;
    }
  else if (strchr (bbase, '\n') == NULL) /* partial read */
    {
      /* check for input corrupted by NULs - if they
         precede the newline, we never get out of here */
      if (strlen(bbase) < blen)
        {
          warn ("ME source format bad, exiting: %s", bbase) ;
          shutDown (lis) ;

          return ;
        }
      if (blen == bufferSize(buffs [0])) {
        if (!expandBuffer (buffs [0], BUFFER_EXPAND_AMOUNT)) {
          warn ("ME error expanding input buffer") ;
          shutDown (lis) ;
          return ;
        }
      }
      readArray = makeBufferArray (bufferTakeRef (buffs [0]),NULL) ;
      if (!prepareRead (ep, readArray, newArticleCommand, data, 1)) {
        warn ("ME error prepare read failed") ;
        freeBufferArray (readArray) ;
        shutDown (lis) ;
        return ;
      }
    }
  else
    {
      /* now iterate over each full command we got on the input. */
      cmd = bbase ;
      while ((cmd < (bbase + blen)) && ((endc = strchr (cmd,'\n')) != NULL))
        {
          Article article ;
          char *next = endc + 1;

          if (*next == '\r')
            next++ ;

          endc-- ;
          if (*endc != '\r')
            endc++ ;

          *endc = '\0' ;

	  /* check for input corrupted by NULs - if they are preceded
	     by newline, we may skip a large chunk without noticing */
	  if (*next == '\0' && next < bbase + blen)
            {
              warn ("ME source format bad, exiting: %s", cmd) ;
              shutDown (lis) ;

              return ;
            }
          
          d_printf (2,"INN Command: %s\n", cmd) ;

          /* pick out the leading string (the filename) */
          if ((fileName = findNonBlankString (cmd,&fileNameEnd)) == NULL)
            {
              warn ("ME source format bad, exiting: %s", cmd) ;
              shutDown (lis) ;

              return ;
            }
          
          *fileNameEnd = '\0' ; /* for the benefit of newArticle() */

          /* now pick out the next string (the message id) */
          if ((msgid = findNonBlankString (fileNameEnd + 1,&msgidEnd)) == NULL)
            {
              *fileNameEnd = ' ' ; /* to make syslog work properly */
              warn ("ME source format bad, exiting: %s", cmd) ;
              shutDown (lis) ;

              return ;
            }

          *msgidEnd = '\0' ;    /* for the benefit of newArticle() */
          
          /* now create an article object and give it all the peers on the
             rest of the command line. Will return null if file is missing. */
          article = newArticle (fileName, msgid) ;
          *fileNameEnd = ' ' ;

          /* Check the message ID length */
          if (strlen(msgid) > NNTP_MSGID_MAXLEN) {
            warn ("ME message id exceeds limit of %d octets: %s",
                  NNTP_MSGID_MAXLEN, msgid) ;
            *(msgidEnd+1) = '\0' ;
          }
          *msgidEnd = ' ' ;

          /* Check if message ID starts with < and ends with > */
          if (*msgid != '<' || *(msgidEnd-1) != '>') {
            warn ("ME source format bad, exiting: %s", cmd) ;
            *(msgidEnd+1) = '\0';
          }

          /* now get all the peernames off the rest of the command lines */
          peerEnd = msgidEnd ;
          do 
            {
              *peerEnd = ' ' ;

              /* pick out the next peer name */
              if ((peer = findNonBlankString (peerEnd + 1,&peerEnd))==NULL)
                break ;     /* even no peer names is OK. */ /* XXX REALLY? */

              *peerEnd = '\0' ;
              
              /* See if this is a valid peername */
              for(s = peer; *s; s++)
                if (!CTYPE(isalnum, *s) && *s != '.' && *s != '-' && *s != '_')
                  break;
              if (*s != 0) {
                  warn ("ME invalid peername %s", peer) ;
                  continue;
              }
              if (article != NULL)
                giveArticleToPeer (lis,article,peer) ;
            }
          while (peerEnd < endc) ;

          delArticle (article) ;
          
          cmd = next ;

	  /* write a checkpoint marker if we've done another large chunk */
	  if (InputFile && *InputFile && ++checkPointCounter == 1000)
	    {
	      /* adjust the seek pointer value by the current location
		 within the input buffer */
	      writeCheckPoint (blen - (cmd - bbase)) ;
	      checkPointCounter = 0 ;
	    }

        }

      if (*cmd != '\0')         /* partial command left in buffer */
        {
          Buffer *bArr ;
          unsigned int leftAmt = blen - (cmd - bbase) ;

          ASSERT (cmd != bbase) ;
          /* first we shift whats left in the buffer down to the bottom */
          memmove (bbase,cmd,leftAmt) ;
          bufferSetDataSize (buffs [0],leftAmt) ;
      
          bArr = makeBufferArray (bufferTakeRef (buffs [0]),NULL) ;
      
          if ( !prepareRead (lis->myep, bArr, newArticleCommand, lis, 1) )
            {
              warn ("ME error prepare read failed") ;

              freeBufferArray (bArr) ;
              
              shutDown (lis) ;

              return ;
            }
        }
      else if ( !readIsPending (lis->myep) ) 
        {                       /* XXX read should never be pending here */
          Buffer *bArr = makeBufferArray (bufferTakeRef (buffs [0]),NULL) ;
      
          bufferSetDataSize (buffs [0],0) ;
      
          if ( !prepareRead (lis->myep, bArr, newArticleCommand, lis, 1) )
            {
              warn ("ME error prepare read failed") ;

              shutDown (lis) ;

              return ;
            }
        }
    }

  freeBufferArray (buffs) ;
}

/* EndPoint callback function for when the sleep due to 
   having reached EOF on InputFile is done. */
static void wakeUp (TimeoutId id, void *data)
{
  InnListener lis = (InnListener) data ;
  Buffer *readArray ;

  ASSERT (id == lis->inputEOFSleepId) ;

  lis->inputEOFSleepId = 0 ;
  readArray = makeBufferArray (bufferTakeRef (lis->inputBuffer), NULL) ;
  prepareRead (lis->myep,readArray,newArticleCommand,lis,1) ;
}


/* Find the Host object for the peer and hand off a reference to the
   article for it to transmit. */
static void giveArticleToPeer (InnListener lis,
                               Article article, const char *peerName)
{
  unsigned int i ;

  for (i = 0 ; i < lis->hostLen ; i++)
    if (lis->myHosts[i] != NULL)
      if (strcmp (peerName,hostPeerName (lis->myHosts [i])) == 0)
        {
          d_printf (1,"Giving article to peer: %s\n", peerName) ;
          hostSendArticle (lis->myHosts [i],artTakeRef (article)) ;
          break ;
        }

  if (i == lis->hostLen)
    {
      d_printf (1,"Failed to give article to peer: -%s-\n", peerName) ;
      
      if (lis->dynamicPeers)
	{
	  Host newHostObj;

          d_printf (1, "Adding peer dynamically\n") ;
          
          newHostObj = newDefaultHost (lis, peerName);

          if (newHostObj == NULL)
            {
              /* Most likely we couldn't get the lock, i.e. the
		 peer is blocked.
	       */
              dropArticle (peerName,article) ;
            }
          else if ( !listenerAddPeer (lis, newHostObj) )
            {
              /* XXX need to remember we've gone over the limit and not try
                 to add any more. */
              warn ("ME internal too many hosts. (max is %lu)",
                    (unsigned long) lis->hostLen) ;
              dropArticle (peerName,article) ;
            }
          else
	    {
	      d_printf (1,"Giving article to peer: %s\n", peerName) ;
	      hostSendArticle (newHostObj,artTakeRef (article)) ;
	    }
        }
      else
        {
          dropArticle (peerName,article) ;
        }
    }
}


static void writeCheckPoint (int offsetAdjust)
{
  char offsetString[16], *writePointer ;
  off_t offset ;
  int writeBytes, writeReturn, mainFd ;
	      
  mainFd = getMainEndPointFd() ;
  offset = lseek (mainFd, 0, SEEK_CUR) ;
  if (offset < 0)
    syslog (LOG_ERR, "ME tell(mainFd): %m") ;
  else
    {
      snprintf (offsetString, sizeof(offsetString), "%ld\n",
		      (long)(offset - offsetAdjust) ) ;
      if ( lseek (mainFd, 0, SEEK_SET) != 0 )
	syslog (LOG_ERR, "ME seek(mainFd, 0, 0): %m") ;
      else
	{
	  writeBytes = strlen (offsetString) ;
	  writePointer = offsetString ;
	  do
	    { 
	      writeReturn = write (mainFd, writePointer, writeBytes) ;
	      if (writeReturn < 0)
		{
		  syslog (LOG_ERR,"ME write input checkpoint: %m") ;
		  break ;
		}
	      writePointer += writeReturn ;
	      writeBytes -= writeReturn ;
	    } while (writeBytes) ;
	  if ( lseek (mainFd, offset, SEEK_SET) != offset )
	    die ("ME seek(mainFd, %ld, SEEK_SET): %s\n", (long)offset,
		 strerror(errno) ) ;
	}
    }
}


void openDroppedArticleFile (void) 
{
  pid_t myPid = getpid () ;
  const char *tapeDir = getTapeDirectory() ;

  if (dropArtFile != NULL)
    free (dropArtFile) ;

  xasprintf (&dropArtFile, "%s/innfeed-dropped.%c%06d",
            tapeDir, droppedFileCount + 'A', (int) myPid) ;

  if ((droppedFp = fopen (dropArtFile,"w")) == NULL)
    {
      syswarn ("ME cant open %s: loosing articles", dropArtFile) ;

      free (dropArtFile) ;
      dropArtFile = NULL ;
      
      if ((droppedFp = fopen ("/dev/null","w")) == NULL)
        {
          die ("ME error opening /dev/null") ;
        }
    }

  
}

void closeDroppedArticleFile (void)
{
  off_t pos ;

  if (droppedFp == NULL)
    return ;

  fflush (droppedFp) ;
  pos = ftello (droppedFp) ;

  fclose (droppedFp) ;
  droppedFp = NULL ;

  if (pos == 0 && dropArtFile != NULL)
    unlink (dropArtFile) ;
  else if (pos != 0 && dropArtFile == NULL)
    warn ("ME lost %ld articles", droppedCount) ;
  else if (pos != 0)
    notice ("ME dropped %ld articles", droppedCount) ;
     
  droppedFileCount = (droppedFileCount + 1) % 26 ;
  droppedCount = 0 ;
}

static void dropArticle (const char *peerName, Article article)
{
  static bool logged = false ;

  if (!logged)
    {
      warn ("ME dropping articles into %s", dropArtFile) ;
      logged = true ;
    }
  
  droppedCount++ ;
  fprintf (droppedFp,"%s %s %s\n",artFileName (article),
           artMsgId (article), peerName) ;
}


static void listenerCleanup (void)
{
  free (dropArtFile) ;
  dropArtFile = NULL ;
}
