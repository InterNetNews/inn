/* -*- c -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Thu Dec 28 13:15:04 1995
 * Project:     INN (innfeed)
 * File:        innlistener.c
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
 * Description: Implementation of the InnListener class.
 * 
 */

#if ! defined (lint)
static const char *rcsid = "$Id$" ;
static void use_rcsid (const char *rid) {   /* Never called */
  use_rcsid (rcsid) ; use_rcsid (rid) ;
}
#endif

#include "config.h"


#if defined (DO_HAVE_UNISTD)
#include <unistd.h>
#endif

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>

#include "innlistener.h"
#include "endpoint.h"
#include "host.h"
#include "buffer.h"
#include "article.h"
#include "msgs.h"
#include "tape.h"
#include "configfile.h"

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

static u_int listenerCount = 0 ;
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


static void giveArticleToPeer (InnListener lis,
                               Article article, const char *peerName) ;
static void newArticleCommand (EndPoint ep, IoStatus i,
                               Buffer *buffs, void *data) ;
static void wakeUp (TimeoutId id, void *data) ;
static void logBadPeer (const char *peer) ;
static bool isBadPeer (const char *peer) ;
static void rememberBadPeer (const char *peer) ;
static void writeCheckPoint (int offsetAdjust) ;
static void dropArticle (const char *peer, Article article) ;
static void listenerCleanup (void) ;

static bool inited = false ;


void listenerLogStatus (FILE *fp)
{
  fprintf (fp,"Listener Status:\n") ;
  fprintf (fp,"    Dropped article file: %s\n",dropArtFile) ;
  fprintf (fp,"   Dropped article count: %ld\n",(long) droppedCount) ;
  fprintf (fp,"\n") ;
}

InnListener newListener (EndPoint endp, bool isDummy, bool dynamicPeers)
{
  InnListener l = CALLOC (struct innlistener_s, 1) ;
  Buffer *readArray ;

  if (!inited)
    {
      inited = true ;
      atexit (listenerCleanup) ;
    }
  
  ASSERT (l != NULL) ;

  l->myep = endp ;

  l->hostLen = MAX_HOSTS ;
  l->myHosts = CALLOC (Host, l->hostLen) ;
  ASSERT (l->myHosts != NULL) ;

  l->inputBuffer = newBuffer (LISTENER_INPUT_BUFFER) ;
  l->dummyListener = isDummy ;
  l->dynamicPeers = dynamicPeers ;

  addPointerFreedOnExit ((char *)bufferBase(l->inputBuffer)) ;
  addPointerFreedOnExit ((char *)l->inputBuffer) ;
  addPointerFreedOnExit ((char *)l->myHosts) ;
  addPointerFreedOnExit ((char *)l) ;

  readArray = makeBufferArray (bufferTakeRef (l->inputBuffer), NULL) ;
  prepareRead (endp,readArray,newArticleCommand,l,1) ;

  l->next = listenerList ;
  listenerList = l ;

  listenerCount++ ;

  return l ;
}

void gPrintListenerInfo (FILE *fp, u_int indentAmt)
{
  InnListener p ;
  char indent [INDENT_BUFFER_SIZE] ;
  u_int i ;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sGlobal InnListener list : %p (count %d) {\n",
           indent,listenerList,listenerCount) ;
  for (p = listenerList ; p != NULL ; p = p->next)
    printListenerInfo (p,fp,indentAmt + INDENT_INCR) ;
  fprintf (fp,"%s}\n",indent) ;
}



void printListenerInfo (InnListener listener, FILE *fp, u_int indentAmt)
{
  char indent [INDENT_BUFFER_SIZE] ;
  u_int i ;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sInnListener : %p {\n",indent,listener) ;
  fprintf (fp,"%s    endpoint : %p\n", indent, listener->myep) ;
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
      fprintf (fp,"%s        %p\n",indent,listener->myHosts[i]) ;
#endif
    }
  
  fprintf (fp,"%s    }\n",indent) ;
  
  fprintf (fp,"%s}\n",indent) ;
}


  /* Close down all hosts on this listener. When they're all gone the
     Listener will be deleted. */
void shutDown (InnListener l)
{
  u_int i ;
  u_int count ;

  dprintf (1,"Shutting down the listener\n") ;

  closeDroppedArticleFile () ;
  unlink (pidFile) ;
  
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

      strcpy (dateString,ctime (&now)) ;
      dateString [24] = '\0' ;

      if (fastExit)
        syslog (LOG_NOTICE,FAST_EXIT_PROGRAM,dateString) ;
      else
        syslog (LOG_NOTICE,STOPPING_PROGRAM,dateString) ;

      exit (0) ;
    }
}


bool listenerAddPeer (InnListener listener, Host hostObj)
{
  u_int i ;

  dprintf (1,"Adding peer: %s\n", hostPeerName (hostObj)) ;
  
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
u_int listenerHostGone (InnListener listener, Host host)
{
  u_int i ;
  u_int someThere = 0 ;

  dprintf (1,"Host is gone: %s\n", hostPeerName (host)) ;
  
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

  dprintf (1,"Host is idle: %s\n", hostPeerName (host)) ;
  
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
      (void) close (fd);
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


int listenerConfigLoadCbk (void *data)
{
  int bval ;

  (void) data ;

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

  ASSERT (ep = lis->myep) ;

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
          dprintf (1,"Got EOF on listener\n") ;
          syslog (LOG_NOTICE,INN_GONE) ;
	  shutDown (lis) ;
        }
    }
  else if (i == IoFailed)
    {
      errno = endPointErrno (ep) ;
      syslog (LOG_ERR,INN_IO_ERROR) ;
      dprintf (1,"Got IO Error on listener\n") ;
      shutDown (lis) ;
    }
  else if (strchr (bbase, '\n') == NULL) /* partial read */
    {
      expandBuffer (buffs [0], BUFFER_EXPAND_AMOUNT) ;
      readArray = makeBufferArray (bufferTakeRef (buffs [0]),NULL) ;
      prepareRead (ep, readArray, newArticleCommand, data, 1) ;
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
          
          dprintf (2,"INN Command: %s\n", cmd) ;

          /* pick out the leading string (the filename) */
          if ((fileName = findNonBlankString (cmd,&fileNameEnd)) == NULL)
            {
              syslog (LOG_ERR,INN_BAD_CMD,cmd) ;
              shutDown (lis) ;

              return ;
            }
          
          *fileNameEnd = '\0' ; /* for the benefit of newArticle() */

          /* now pick out the next string (the message id) */
          if ((msgid = findNonBlankString (fileNameEnd + 1,&msgidEnd)) == NULL)
            {
              *fileNameEnd = ' ' ; /* to make syslog work properly */
              syslog (LOG_ERR,INN_BAD_CMD,cmd) ;
              shutDown (lis) ;

              return ;
            }

          *msgidEnd = '\0' ;    /* for the benefit of newArticle() */
          
          /* now create an article object and give it all the peers on the
             rest of the command line. Will return null if file is missing. */
          article = newArticle (fileName, msgid) ;
          *fileNameEnd = ' ' ;
          *msgidEnd = ' ' ;

          /* now get all the peernames off the rest of the command lines */
          peerEnd = msgidEnd ;
          do 
            {
              *peerEnd = ' ' ;

              /* pick out the next peer name */
              if ((peer = findNonBlankString (peerEnd + 1,&peerEnd))==NULL)
                break ;     /* even no peer names is OK. */ /* XXX REALLY? */

              *peerEnd = '\0' ;
              
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
          u_int leftAmt = blen - (cmd - bbase) ;

          /* first we shift whats left in the buffer down to the bottom */
          if (cmd != bbase)
            {
              memcpy (bbase,cmd,leftAmt) ;
              bufferSetDataSize (buffs [0],leftAmt) ;
            }
          else if ( !expandBuffer (buffs[0],BUFFER_EXPAND_AMOUNT) )
            {
              syslog (LOG_ERR,L_BUFFER_EXPAND_ERROR);

              shutDown (lis) ;

              return ;
            }
      
          bArr = makeBufferArray (bufferTakeRef (buffs [0]),NULL) ;
      
          if ( !prepareRead (lis->myep, bArr, newArticleCommand, lis, 1) )
            {
              syslog (LOG_ERR,L_PREPARE_READ_FAILED) ;

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
              syslog (LOG_ERR,L_PREPARE_READ_FAILED) ;

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

  ASSERT (id = lis->inputEOFSleepId) ;

  lis->inputEOFSleepId = 0 ;
  readArray = makeBufferArray (bufferTakeRef (lis->inputBuffer), NULL) ;
  prepareRead (lis->myep,readArray,newArticleCommand,lis,1) ;
}

/*
 * Log the name of the bad peer. One time only.
 */
static char **peers ;
static u_int peerLen ;
static u_int peerIdx ;

static void rememberBadPeer (const char *peer)
{
  if (isBadPeer (peer))         /* check if we already logged it. */
    return ;
  
  if (peerIdx == peerLen)
    {
      peerLen += 10 ;
      if (peers != NULL)
        peers = REALLOC (peers, char *, peerLen) ;
      else
        peers = ALLOC (char *, peerLen) ;
    }

  peers [peerIdx++] = strdup (peer) ;
}

static void logBadPeer (const char *peer)
{
  if (isBadPeer (peer))
    return ;
  
  rememberBadPeer (peer) ;
  syslog (LOG_ERR,UNKNOWN_PEER,peer) ;
}


static bool isBadPeer (const char *peer)
{
  u_int i ;
  
  for (i = 0 ; i < peerIdx ; i++)
    if (strcmp (peer,peers [i]) == 0)
      return true ;

  return false ;
}


/* Find the Host object for the peer and hand off a reference to the
   article for it to transmit. */
static void giveArticleToPeer (InnListener lis,
                               Article article, const char *peerName)
{
  u_int i ;

  for (i = 0 ; i < lis->hostLen ; i++)
    if (lis->myHosts[i] != NULL)
      if (strcmp (peerName,hostPeerName (lis->myHosts [i])) == 0)
        {
          dprintf (1,"Giving article to peer: %s\n", peerName) ;
          hostSendArticle (lis->myHosts [i],artTakeRef (article)) ;
          break ;
        }

  if (i == lis->hostLen)
    {
      dprintf (1,"Failed to give article to peer: -%s-\n", peerName) ;
      
      if (lis->dynamicPeers && !isBadPeer (peerName))
        {
          u_int articleTout ;
          u_int respTout ;
          u_int initialCxns ;
          u_int maxCxns ;
          u_int maxChecks ;
          bool streaming ;
          double lowFilter ;
          double highFilter ;
          u_short portNum ;
          Host newHostObj ;

          dprintf (1, "Adding peer dynamically\n") ;
          
          syslog (LOG_NOTICE,DYNAMIC_PEER,peerName) ;

          getHostDefaults (&articleTout, &respTout, &initialCxns,
                           &maxCxns, &maxChecks, &streaming,
                           &lowFilter, &highFilter, &portNum) ;

          newHostObj = newHost (lis, peerName, peerName,
                                articleTout, respTout, initialCxns,
                                maxCxns, maxChecks, portNum, CLOSE_PERIOD,
                                streaming, lowFilter, highFilter);

          if (newHostObj == NULL)
            {
              /* XXX I need to handle this better. */
              rememberBadPeer (peerName) ;
              syslog (LOG_ERR,NO_HOST,peerName) ;
              dropArticle (peerName,article) ;
            }
          else if ( !listenerAddPeer (lis, newHostObj) )
            {
              /* XXX need to remember we've gone over the limit and not try
                 to add any more. */
              rememberBadPeer (peerName) ;
              syslog (LOG_ERR, TOO_MANY_HOSTS, lis->hostLen) ;
              dropArticle (peerName,article) ;
            }
          else
            hostSendArticle (newHostObj,artTakeRef (article)) ;
        }
      else
        {
          logBadPeer (peerName) ;
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
  offset = lseek (mainFd, 0L, SEEK_CUR) ;
  if (offset < 0)
    syslog (LOG_ERR, "ME tell(mainFd): %m") ;
  else
    {
      (void) sprintf (offsetString, "%ld\n",
		      (long)(offset - offsetAdjust) ) ;
      if ( lseek (mainFd, 0L, SEEK_SET) != 0 )
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
    FREE (dropArtFile) ;
  
  dropArtFile = malloc (pathMax(tapeDir) + 1) ;
  
  sprintf (dropArtFile,"%s/innfeed-dropped.%c%06d",
           tapeDir, droppedFileCount + 'A', (int) myPid) ;

  if ((droppedFp = fopen (dropArtFile,"w")) == NULL)
    {
      syslog (LOG_ERR,NO_DROPPED_FILE,dropArtFile) ;

      FREE (dropArtFile) ;
      dropArtFile = NULL ;
      
      if ((droppedFp = fopen ("/dev/null","w")) == NULL)
        {
          syslog (LOG_ERR,NO_NULL_FILE) ;
          die ("Error opening /dev/null") ;
        }
    }

  
}

void closeDroppedArticleFile (void)
{
  long pos ;

  if (droppedFp == NULL)
    return ;

  fflush (droppedFp) ;
  pos = ftell (droppedFp) ;

  fclose (droppedFp) ;
  droppedFp = NULL ;

  if (pos == 0 && dropArtFile != NULL)
    unlink (dropArtFile) ;
  else if (pos != 0 && dropArtFile == NULL)
    syslog (LOG_WARNING,LOST_ARTICLE_COUNT,droppedCount) ;
  else if (pos != 0)
    syslog (LOG_NOTICE,DROPPED_ARTICLE_COUNT,droppedCount) ;
     
  droppedFileCount = (droppedFileCount + 1) % 26 ;
  droppedCount = 0 ;
}

static void dropArticle (const char *peerName, Article article)
{
  static bool logged = false ;

  if (!logged)
    {
      syslog (LOG_WARNING,DROPPED_LOCATION,dropArtFile) ;
      logged = true ;
    }
  
  droppedCount++ ;
  fprintf (droppedFp,"%s %s %s\n",artFileName (article),
           artMsgId (article), peerName) ;
}


static void listenerCleanup (void)
{
  FREE (dropArtFile) ;
}
