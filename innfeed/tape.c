/*  $Id$
**
**  The implementation of the innfeed Tape class.
**
**  Written by James Brister <brister@vix.com>
**
**  The implementation of the Tape class. Tapes are read-only or write-only
**  files that are accessed sequentially. Their basic unit of i/o is an
**  Article. Tapes work out of a single directory and manage all file names
**  themselves.
**
**  Tapes will checkpoint themselves periodically so that when innfeed exits
**  or crashes things can restart close to where they were last. The period
**  checkpointing is handled entirely by the Tape class, but the checkpoint
**  period needs to be set by some external user before the first tape is
**  created.
*/

#include "innfeed.h"
#include "config.h"
#include "clibrary.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <syslog.h>

#include <dirent.h>
typedef struct dirent DIRENTRY ;

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/libinn.h"

#include "article.h"
#include "configfile.h"
#include "endpoint.h"
#include "tape.h"

extern char *dflTapeDir;
extern bool genHtml ;
extern unsigned int hostHighwater;

#if 0
/* a structure for temporary storage of articles. */
typedef struct q_e_s
{
    Article article ;
    struct q_e_s *next ;
} *QueueElem ;
#endif

/* The Tape class type. */
struct tape_s
{
    /* the pathname of the file the administrator can drop in by hand. */
    char *handFilename ;

    /* the pathname of the file the Tape will read from */
    char *inputFilename ;

    /* the pathname of the file the Tape will write to. */
    char *outputFilename ;

    /* the pathname of the file used in locking */
    char *lockFilename ;

    /* the peer we're doing this for. */
    char *peerName ;
    
    FILE *inFp ;                /* input FILE */
    FILE *outFp ;               /* output FILE */

    time_t lastRotated ;        /* time files last got switched */
    bool checkNew ;             /* set bool when we need to check for
                                   hand-crafted file. */
    
#if 0
    /* the tape holds a small output queue in memory to avoid thrashing. */
    QueueElem head ;            
    QueueElem tail ;
    unsigned int qLength ;             /* amount on the queue */
#endif
    
    long outputSize ;           /* the current size of the output file. */
    long lossage ;

    /* the number of bytes we try to keep the output under. We actually
       wait for the outputSize to get 10% greater than this amount before
       shrinking the file down again. A value of zero means no limit. */
    long outputLowLimit ;
    long outputHighLimit ;
    double backlogFactor ;
    
    bool scribbled ;            /* have we scribbled a checkpoint value in
                                   the file. */
    long tellpos ;              /* for input file checkpointing. */
    bool changed ;              /* true if tape was read since last
                                   checkpoint or start. */

    /* true if articles that are output are NOT later input. */
    bool noRotate ;     

    /* true if no articles should ever be spooled */
    bool noBacklog ;
};


static void checkpointTape (Tape tape) ;
static void removeTapeGlobally (Tape tape) ;
static void addTapeGlobally (Tape tape) ;
static void prepareFiles (Tape tape) ;
static void tapeCkNewFileCbk (TimeoutId id, void *d) ;
static void tapeCheckpointCallback (TimeoutId id, void *d) ;
#if 0
static void flushTape (Tape tape) ;
#endif
static void tapesSetCheckNew (void) ;
static void initTape (Tape nt) ;
static void tapeCleanup (void) ;



/* pathname of directory we store tape files in. */
static char *tapeDirectory ;

/* the callback ID of the checkpoint timer callback. */
static TimeoutId checkPtId ;
static TimeoutId ckNewFileId ;

/* number of seconds between tape checkpoints. */
static unsigned int tapeCkPtPeriod ;
static unsigned int tapeCkNewFilePeriod ;

static time_t rotatePeriod = TAPE_ROTATE_PERIOD ;

/* global list of tapes so we can checkpoint them periodically */
static Tape *activeTapes ;

/* Size of the activeTapes array */
static size_t activeTapeSize ;

/* index of last element in activeTapes that's being used. */
static size_t activeTapeIdx ;

#if 0
/* default limit of the size of output tapes. */
static long defaultSizeLimit ;
#endif

unsigned int tapeHighwater ;

bool debugShrinking = false ;

extern bool talkToSelf ;        /* main.c */




/* callback when config file is loaded */
int tapeConfigLoadCbk (void *data)
{
  int rval = 1 ;
  long iv ;
  int bv ;
  FILE *fp = (FILE *) data ;
  char *dir, *p ;

  if (getString (topScope,"backlog-directory",&p,NO_INHERIT))
    {
      dir = concatpath(innconf->pathspool, p);
      free(p);
      if (tapeDirectory != NULL && strcmp (tapeDirectory,dir) != 0)
        {
          warn ("ME config: cannot change backlog-directory of a running"
                " process") ;
          free (dir) ;
          dir = xstrdup (tapeDirectory) ;
        }

      if (!isDirectory (dir) && isDirectory (dflTapeDir))
        {
          logOrPrint (LOG_ERR,fp,
                      "ME config: definition of backlog-directory (%s) is a"
                      " non-existant directory. Using %s",
                      dir,dflTapeDir) ;
          free (dir) ;
          dir = xstrdup (dflTapeDir) ;
        }
      else if (!isDirectory (dir))
        logAndExit (1,"ME config: no usable value for backlog-directory") ;
    }
  else if (!isDirectory (dflTapeDir))
  {
    logAndExit (1,"ME config: no usable value for backlog-directory") ;
    return -1; /* not reached */
  }
  else
    dir = xstrdup (dflTapeDir) ;
  
  if (tapeDirectory != NULL)
    free (tapeDirectory) ;
  tapeDirectory = dir ;


  
  if (getInteger (topScope,"backlog-highwater",&iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,
                      "ME config: value of %s (%ld) in %s cannot be less"
                      " than 0. Using %ld","backlog-highwater",
                      iv,"global scope",(long)TAPE_HIGHWATER);
          iv = TAPE_HIGHWATER ;
        }
    }
  else
    iv = TAPE_HIGHWATER ;
  tapeHighwater = (unsigned int) iv ;


  
  if (getInteger (topScope,"backlog-rotate-period",&iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,
                      "ME config: value of %s (%ld) in %s cannot be less"
                      " than 0. Using %ld",
                      "backlog-rotate-period",
                      iv,"global scope",(long)TAPE_ROTATE_PERIOD);
          iv = TAPE_ROTATE_PERIOD ;
        }
    }
  else
    iv = TAPE_ROTATE_PERIOD ;
  rotatePeriod = (unsigned int) iv ;


  if (getInteger (topScope,"backlog-ckpt-period",&iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,
                      "ME config: value of %s (%ld) in %s cannot be less"
                      " than 0. Using %ld",
                      "backlog-ckpt-period",iv,
                      "global scope",(long)TAPE_CHECKPOINT_PERIOD);
          iv = TAPE_CHECKPOINT_PERIOD ;
        }
    }
  else
    iv = TAPE_CHECKPOINT_PERIOD ;
  tapeCkPtPeriod = (unsigned int) iv ;


  if (getInteger (topScope,"backlog-newfile-period",&iv,NO_INHERIT))
    {
      if (iv < 0)
        {
          rval = 0 ;
          logOrPrint (LOG_ERR,fp,
                      "ME config: value of %s (%ld) in %s cannot be less"
                      " than 0. Using %ld",
                      "backlog-newfile-period",
                      iv,"global scope",(long)TAPE_NEWFILE_PERIOD);
          iv = TAPE_NEWFILE_PERIOD ;
        }
    }
  else
    iv = TAPE_NEWFILE_PERIOD ;
  tapeCkNewFilePeriod = (unsigned int) iv ;


  if (getBool (topScope,"debug-shrinking",&bv,NO_INHERIT))
    debugShrinking = (bv ? true : false) ;
  
  return rval ;
}


/* Create a new Tape object. There are three potential files involved in
   I/O. 'peerName' is what  the admin may have dropped in by
   hand. 'peerName.input' is the file that was being used as input the last
   time things were run. 'peerName.output' is the file that was being used
   as output. The file 'peerName' is appended to 'peerName.input' (or
   renamed if 'peerName.input' doesn't exist). Then 'peerName.output' is
   appeneded (or renamed) to 'peerName.input' */

static bool inited = false ;
Tape newTape (const char *peerName, bool dontRotate)
{
  Tape nt = xmalloc (sizeof(struct tape_s)) ;
  size_t pLen = strlen (peerName) ;
  size_t dLen = strlen (tapeDirectory) ;

  if (!inited)
    {
      inited = true ;
      atexit (tapeCleanup) ;
    }
  
  ASSERT (nt != NULL) ;

  if (endsIn (peerName,INPUT_TAIL))
    die ("Sorry, can't have a peer name ending in \"%s\"",INPUT_TAIL) ;

  if (endsIn (peerName,OUTPUT_TAIL))
    die ("Sorry, can't have a peer name ending in \"%s\"",OUTPUT_TAIL) ;

  if (endsIn (peerName,LOCK_TAIL))
    die ("Sorry, can't have a peer name ending in \"%s\"",LOCK_TAIL) ;

  nt->peerName = xstrdup (peerName) ;
  
  nt->handFilename = xmalloc (pLen + dLen + 2) ;
  sprintf (nt->handFilename,"%s/%s",tapeDirectory,peerName) ;

  nt->lockFilename = xmalloc (pLen + dLen + strlen(LOCK_TAIL) + 2) ;
  sprintf (nt->lockFilename,"%s/%s%s",tapeDirectory,peerName,LOCK_TAIL) ;

  nt->inputFilename = xmalloc (pLen + dLen + strlen(INPUT_TAIL) + 2) ;
  sprintf (nt->inputFilename,"%s/%s%s",tapeDirectory,peerName,INPUT_TAIL) ;

  nt->outputFilename = xmalloc (pLen + dLen + strlen(OUTPUT_TAIL) + 2) ;
  sprintf (nt->outputFilename,"%s/%s%s",tapeDirectory,peerName,OUTPUT_TAIL) ;

  if ( !lockFile (nt->lockFilename) )
    {
      warn ("ME lock failed for host: %s", nt->lockFilename) ;
      
      free (nt->handFilename) ;
      free (nt->lockFilename) ;
      free (nt->inputFilename) ;
      free (nt->outputFilename) ;
      free (nt) ;

      return NULL ;
    }

  nt->noRotate = false ;        /* for first time prepare */
  initTape (nt) ;
  nt->noRotate = dontRotate ;

  addTapeGlobally (nt) ;

  if (checkPtId == 0 && tapeCkPtPeriod > 0)     /* only done once. */
    checkPtId = prepareSleep (tapeCheckpointCallback,tapeCkPtPeriod,NULL);

  if (ckNewFileId == 0 && tapeCkNewFilePeriod > 0)
    ckNewFileId = prepareSleep (tapeCkNewFileCbk,tapeCkNewFilePeriod,NULL) ;

  return nt ;
}

static void initTape (Tape nt)
{
  value *peerVal = findPeer (nt->peerName) ;
  scope *s = (peerVal == NULL ? NULL : peerVal->v.scope_val) ;

  nt->inFp = NULL ;
  nt->outFp = NULL ;

  nt->lastRotated = 0 ;
  nt->checkNew = false ;
  
#if 0
  nt->head = NULL ;
  nt->tail = NULL ;
  nt->qLength = 0 ;
#endif

  nt->scribbled = false ;
  nt->tellpos = 0 ;

  nt->changed = false ;
  
  nt->outputSize = 0 ;
  nt->lossage = 0 ;

  nt->noBacklog = false ;
  nt->outputLowLimit = BLOGLIMIT;
  nt->outputHighLimit = BLOGLIMIT_HIGH;
  nt->backlogFactor = LIMIT_FUDGE;

  if (!talkToSelf)
    {
      int bval ;

      if (getBool (s, "no-backlog", &bval, INHERIT))
        nt->noBacklog = (bval ? true : false);
      else
        nt->noBacklog = TAPE_DISABLE;

      if (getInteger (s,"backlog-limit",&nt->outputLowLimit,INHERIT))
        {
          if (!getReal (s,"backlog-factor",&nt->backlogFactor,INHERIT))
            {
              if (!getInteger (s,"backlog-limit-highwater",
                               &nt->outputHighLimit,INHERIT))
                {
                  warn ("%s no backlog-factor or backlog-limit-highwater",
                        nt->peerName) ;
                  nt->outputLowLimit = BLOGLIMIT;
                  nt->outputHighLimit = BLOGLIMIT_HIGH;
                  nt->backlogFactor = LIMIT_FUDGE;
                }
            }
          else
            nt->outputHighLimit = (long)(nt->outputLowLimit * nt->backlogFactor);
        }
      else
        warn ("ME config: no definition for required key backlog-limit") ;
    }
  
  d_printf (1, "%s spooling: %s\n", nt->peerName, 
  	   nt->noBacklog ? "disabled" : "enabled");

  d_printf (1,"%s tape backlog limit: [%ld %ld]\n",nt->peerName,
           nt->outputLowLimit,
           nt->outputHighLimit) ;
  
  prepareFiles (nt) ;
}


void gFlushTapes (void)
{
  unsigned int i ;

  notice ("ME flushing tapes") ;
  for (i = 0 ; i < activeTapeIdx ; i++)
    tapeFlush (activeTapes [i]) ;
}



/* close the input and output tapes and reinitialize everything in the
  tape. */
void tapeFlush (Tape tape)
{
  if (tape->inFp != NULL)
    {
      checkpointTape (tape) ;
      fclose (tape->inFp) ;
    }

  if (tape->outFp != NULL)
    fclose (tape->outFp) ;

  initTape (tape) ;
}



void gPrintTapeInfo (FILE *fp, unsigned int indentAmt)
{
  char indent [INDENT_BUFFER_SIZE] ;
  unsigned int i ;
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sGlobal Tape List : (count %lu) {\n",
           indent,(unsigned long) activeTapeIdx) ;

  for (i = 0 ; i < activeTapeIdx ; i++)
    printTapeInfo (activeTapes [i],fp,indentAmt + INDENT_INCR) ;
  fprintf (fp,"%s}\n",indent) ;
}


void tapeLogGlobalStatus (FILE *fp)
{
  fprintf (fp,"%sBacklog file global values:%s\n",
           genHtml ? "<B>" : "", genHtml ? "</B>" : "") ;
  fprintf (fp,"        directory: %s\n",tapeDirectory) ;
  fprintf (fp,"    rotate period: %-3ld seconds\n",(long) rotatePeriod) ;
  fprintf (fp,"checkpoint period: %-3ld seconds\n",(long) tapeCkPtPeriod) ;
  fprintf (fp,"   newfile period: %-3ld seconds\n",(long) tapeCkNewFilePeriod);
  fprintf (fp,"backlog highwater: %u\n", tapeHighwater);
  fprintf (fp,"  highwater queue: %u\n", hostHighwater);
  fprintf (fp,"\n") ;
}


void tapeLogStatus (Tape tape, FILE *fp)
{
  if (tape == NULL)
    fprintf (fp,"(no tape)\n") ;
  else if (tape->noBacklog)
      fprintf (fp,"                 spooling: DISABLED\n");
  else if (tape->outputLowLimit == 0)
      fprintf (fp,"                 spooling: UNLIMITED\n");
  else 
    {
      fprintf (fp,"                 backlog low limit: %ld\n",
               tape->outputLowLimit) ;
      fprintf (fp,"               backlog upper limit: %ld",
               tape->outputHighLimit) ;
      if (tape->backlogFactor > 0.0)
        fprintf (fp," (factor %1.2f)",tape->backlogFactor) ;
      fputc ('\n',fp) ;
      fprintf (fp,"                 backlog shrinkage: ") ;
      fprintf (fp,"%ld bytes (from current file)\n",tape->lossage) ;
    }
}

void printTapeInfo (Tape tape, FILE *fp, unsigned int indentAmt)
{
  char indent [INDENT_BUFFER_SIZE] ;
  unsigned int i ;
#if 0
  QueueElem qe ;
#endif
  
  for (i = 0 ; i < MIN(INDENT_BUFFER_SIZE - 1,indentAmt) ; i++)
    indent [i] = ' ' ;
  indent [i] = '\0' ;

  fprintf (fp,"%sTape : %p {\n",indent,(void *) tape) ;

  if (tape == NULL)
    {
      fprintf (fp,"%s}\n",indent) ;
      return ;
    }
  
  fprintf (fp,"%s    master-file : %s\n", indent, tape->handFilename) ;
  fprintf (fp,"%s    input-file : %s\n", indent, tape->inputFilename) ;
  fprintf (fp,"%s    output-file : %s\n",indent, tape->outputFilename) ;
  fprintf (fp,"%s    lock-file : %s\n",indent, tape->lockFilename) ;
  fprintf (fp,"%s    peerName : %s\n",indent,tape->peerName) ;
  fprintf (fp,"%s    input-FILE : %p\n",indent, (void *) tape->inFp) ;
  fprintf (fp,"%s    output-FILE : %p\n",indent, (void *) tape->outFp) ;
  fprintf (fp,"%s    output-limit : %ld\n",indent, tape->outputLowLimit) ;

#if 0
  fprintf (fp,"%s    in-memory article queue (length  %d) {\n",indent,
           tape->qLength) ;

  for (qe = tape->head ; qe != NULL ; qe = qe->next)
    {
#if 0
      printArticleInfo (qe->article,fp,indentAmt + INDENT_INCR) ;
#else
      fprintf (fp,"%s    %p\n",indent,qe->article) ;
#endif
    }

  fprintf (fp,"%s    }\n",indent) ;
#endif

  fprintf (fp,"%s    tell-position : %ld\n",indent,(long) tape->tellpos) ;
  fprintf (fp,"%s    input-FILE-changed : %s\n",indent,
           boolToString (tape->changed)) ;

  fprintf (fp,"%s    no-rotate : %s\n",indent, boolToString (tape->noRotate));
  
  fprintf (fp,"%s}\n",indent) ;
}




/* delete the tape. Spools the in-memory articles to disk. */
void delTape (Tape tape)
{
  struct stat st ;
  
  if (tape == NULL)
    return ;

#if 1
  
  if (tape->outFp != NULL && fclose (tape->outFp) != 0)
    syswarn ("ME ioerr fclose %s", tape->outputFilename) ;

  if (stat(tape->outputFilename, &st) == 0 && st.st_size == 0)
    {
      d_printf (1,"removing empty output tape: %s\n",tape->outputFilename) ;
      unlink (tape->outputFilename) ;
    }
  
  tape->outFp = NULL ;
  tape->outputSize = 0 ;

#else
  
  tapeClose (tape) ;

#endif

  if (tape->inFp != NULL)
    {
      checkpointTape (tape) ;
      fclose (tape->inFp) ;
    }

  unlockFile (tape->lockFilename) ;

  freeCharP (tape->handFilename) ;
  freeCharP (tape->inputFilename) ;
  freeCharP (tape->outputFilename) ;
  freeCharP (tape->lockFilename) ;
  freeCharP (tape->peerName) ;
             
  removeTapeGlobally (tape) ;
  
  free (tape) ;
}


void tapeTakeArticle (Tape tape, Article article)
{
#if 0
  QueueElem elem ;
#endif
  const char *fname, *msgid ;

  ASSERT (tape != NULL) ;
  ASSERT (article != NULL) ;

  /* return immediately if spooling disabled - jgarzik */
  if (tape->noBacklog)
    {
      delArticle (article) ;
      return;
    }

  fname = artFileName (article) ;
  msgid = artMsgId (article) ;
  fprintf (tape->outFp,"%s %s\n", fname, msgid);
  /* I'd rather know where I am each time, and I don't trust all
   * fprintf's to give me character counts.  Therefore, do not use:
   *   tape->outputSize += (return value of the previous fprintf call);
   * nor:
   *   tape->outputSize = ftello (tape->outFp);
   */
  tape->outputSize += strlen(fname) + strlen(msgid) + 2 ; /* " " + "\n" */
  
  delArticle (article) ;

  if (debugShrinking)
    {
      struct stat sb ;

      fflush (tape->outFp) ;
      
      if (fstat (fileno (tape->outFp),&sb) != 0)
        syswarn ("ME oserr fstat %s",tape->outputFilename) ;
      else if (sb.st_size != tape->outputSize) 
        syslog (LOG_ERR,"fstat and ftello do not agree: %ld %ld for %s\n",
                (long)sb.st_size,tape->outputSize,tape->outputFilename) ;
    }
  
  if (tape->outputHighLimit > 0 && tape->outputSize >= tape->outputHighLimit)
    {
      long oldSize = tape->outputSize ;
      shrinkfile (tape->outFp,tape->outputLowLimit,tape->outputFilename,"a+");
      tape->outputSize = ftello (tape->outFp) ;
      tape->lossage += oldSize - tape->outputSize ;
    }
}


/* Pick an article off a tape and return it. NULL is returned if there
   are no more articles. */
Article getArticle (Tape tape)
{
  char line [2048] ;            /* ick. 1024 for filename + 1024 for msgid */
  char *p, *q ;
  char *msgid, *filename ;
  Article art = NULL ;
  time_t now = theTime() ;

  ASSERT (tape != NULL) ;

  if (tape->inFp == NULL && (now - tape->lastRotated) > rotatePeriod)
    prepareFiles (tape) ;       /* will flush queue too. */

  while (tape->inFp != NULL && art == NULL)
    {
      tape->changed = true ;
      
      if (fgets (line,sizeof (line), tape->inFp) == NULL)
        {
          if (ferror (tape->inFp))
            syswarn ("ME ioerr on tape file %s", tape->inputFilename) ;
          else if ( !feof (tape->inFp) )
            syswarn ("ME oserr fgets %s", tape->inputFilename) ;
          
          if (fclose (tape->inFp) != 0)
            syswarn ("ME ioerr fclose %s", tape->inputFilename) ;

          d_printf (1,"No more articles on tape %s\n",tape->inputFilename) ;

          tape->inFp = NULL ;
          tape->scribbled = false ;

          unlink (tape->inputFilename) ;

          if ((now - tape->lastRotated) > rotatePeriod)
            prepareFiles (tape) ; /* rotate files to try next. */
        }
      else
        {
          msgid = filename = NULL ;
          
          for (p = line ; *p && isspace((unsigned char) *p) ; p++) /* eat whitespace */
            /* nada */ ;

          if (*p != '\0')
            {
              q = strchr (p,' ') ;

              if (q != NULL)
                {
                  filename = p ;
                  *q = '\0' ;
      
                  for (q++ ; *q && isspace((unsigned char) *q) ; q++)
                    /* nada */ ;

                  if (*q != '\0')
                    {
                      if (((p = strchr (q, ' ')) != NULL) ||
                          ((p = strchr (q, '\n')) != NULL))
                        *p = '\0' ;

                      if (p != NULL)
                        msgid = q ;
                      else
                        filename = NULL ; /* no trailing newline or blank */
                    }
                  else
                    filename = NULL ; /* line had one field and some blanks */
                }
              else
                filename = NULL ; /* line only had one field */
            }

          /* See if message ID looks valid. */
          if (msgid) {
            for (p = msgid; *p; p++)
              ;
            if (p > msgid) p--;
            if (*msgid != '<' || *p != '>') {
              warn ("ME tape invalid messageID in %s: %s",
                    tape->inputFilename,msgid);
              msgid = NULL;
            }
          }

          if (filename != NULL && msgid != NULL)
            art = newArticle (filename, msgid) ;

          /* art may be NULL here if the file is no longer valid. */
        }
    }
  
#if 0
  /* now we either have an article or there is no more on disk */
  if (art == NULL)
    {
      if (tape->inFp != NULL && ((c = fgetc (tape->inFp)) != EOF))
        ungetc (c,tape->inFp) ; /* shouldn't happen */
      else if (tape->inFp != NULL)
        {
          /* last article read was the end of the tape. */
          if (fclose (tape->inFp) != 0)
            syswarn ("ME ioerr fclose %s", tape->inputFilename) ;
          
          tape->inFp = NULL ;
          tape->scribbled = false ;
          
          /* toss out the old input file and prepare the new one */
          unlink (tape->inputFilename) ;
          
          if (now - tape->lastRotated > rotatePeriod)
            prepareFiles (tape) ;
        }
    }
#endif
  
  if (art == NULL)
    d_printf (2,"%s All out of articles in the backlog\n", tape->peerName) ;
  else
    d_printf (2,"%s Peeled article %s from backlog\n",tape->peerName,
             artMsgId (art)) ;
      
  return art ;
}


/****************************************************/
/**                  CLASS FUNCTIONS               **/
/****************************************************/

/* Cause all the Tapes to checkpoint themselves. */
void checkPointTapes (void)
{
  unsigned int i ;

  for (i = 0 ; i < activeTapeIdx ; i++)
    checkpointTape (activeTapes [i]) ;
}


/* make all the tapes set their checkNew flag. */
static void tapesSetCheckNew (void) 
{
  unsigned int i ;

  for (i = 0 ; i < activeTapeIdx ; i++)
    activeTapes[i]->checkNew = true ;
}


#if 0
/* Set the pathname of the directory for storing tapes in. */
void setTapeDirectory (const char *newDir)
{
  /* the activeTape variable gets set when the first Tape object
     is created */
  if (activeTapes != NULL)
    {
      syslog (LOG_CRIT,"Resetting backlog directory") ;
      abort() ;
    }
  
  if (tapeDirectory != NULL)
    freeCharP (tapeDirectory) ;
  
  tapeDirectory = xstrdup (newDir) ;

  addPointerFreedOnExit (tapeDirectory) ;
}
#endif

/* Get the pathname of the directory tapes are stored in. */
const char *getTapeDirectory (void)
{
  ASSERT (tapeDirectory != NULL) ;

#if 0
  if (tapeDirectory == NULL)
    {
      tapeDirectory = xstrdup (dflTapeDir) ;
      addPointerFreedOnExit (tapeDirectory) ;
    }
#endif
  
  return tapeDirectory ;
}


#if 0
void setOutputSizeLimit (long val)
{
  defaultSizeLimit = val ;
}
#endif






/**********************************************************************/
/*                          PRIVATE FUNCTIONS                         */
/**********************************************************************/


/* Add a new tape to the class-level list of active tapes.  */
static void addTapeGlobally (Tape tape)
{
  ASSERT (tape != NULL) ;
  
  if (activeTapeSize == activeTapeIdx)
    {
      unsigned int i ;
      
      activeTapeSize += 10 ;
      if (activeTapes != NULL)
        activeTapes = xrealloc (activeTapes, sizeof(Tape) * activeTapeSize) ;
      else
        activeTapes = xmalloc (sizeof(Tape) * activeTapeSize) ;

      for (i = activeTapeIdx ; i < activeTapeSize ; i++)
        activeTapes [i] = NULL ;
    }
  activeTapes [activeTapeIdx++] = tape ;
}


/* Remove a tape for the class-level list of active tapes. */
static void removeTapeGlobally (Tape tape)
{
  unsigned int i ;

  if (tape == NULL)
    return ;
  
  ASSERT (activeTapeIdx > 0) ;
  
  for (i = 0 ; i < activeTapeIdx ; i++)
    if (activeTapes [i] == tape)
      break ;

  ASSERT (i < activeTapeIdx) ;

  for ( ; i < (activeTapeIdx - 1) ; i++)
    activeTapes [i] = activeTapes [i + 1] ;

  activeTapes [--activeTapeIdx] = NULL ;

  if (activeTapeIdx == 0)
    {
      free (activeTapes) ;
      activeTapes = NULL ;
    }
}


/* Have a tape checkpoint itself so that next process can pick up where
   this one left off. */
static void checkpointTape (Tape tape)
{
  if (tape->inFp == NULL)       /* no input file being read. */
    return ;

  if (!tape->changed)   /* haven't read since last checkpoint */
    {
      d_printf (1,"Not checkpointing unchanged tape: %s\n", tape->peerName) ;
      return ;
    }
  
  if ((tape->tellpos = ftello (tape->inFp)) < 0)
    {
      syswarn ("ME oserr ftello %s", tape->inputFilename) ;
      return ;
    }

  /* strlen of "18446744073709551616\n" (2^64) */
#define BITS64 21

  /* make sure we're not right at the beginning of the file so we can write. */
  if (tape->tellpos > BITS64)
    {
      rewind (tape->inFp) ;

      /* scribble blanks over the first lines characters */
      if (!tape->scribbled) 
        {
          int currloc = 0 ;
          int c ;

          while ((c = fgetc (tape->inFp)) != '\n' || currloc <= BITS64)
            if (c == EOF)
              return ;
            else
              currloc++ ;

          rewind (tape->inFp) ;
          
          while (currloc-- > 0)
            fputc (' ',tape->inFp) ;

          rewind (tape->inFp) ;

          fflush (tape->inFp) ;
          tape->scribbled = true ;
        }
      
      fprintf (tape->inFp,"%ld",tape->tellpos) ;

      if (fseeko (tape->inFp,tape->tellpos,SEEK_SET) != 0)
        syswarn ("ME oserr fseeko(%s,%ld,SEEK_SET)",tape->inputFilename,
                 tape->tellpos) ;
    }
  
  tape->changed = false ;
}



/* Prepare the tape file(s) for input and output */

/* For a given Tape there are
 * three possible files: PEER.input PEER and
 * PEER.output. PEER.input and PEER.output are private to
 * innfeed. PEER is where a sysadmin can drop a file that (s)he
 * wants to inject into the process. If the first line of the input file
 * contains only an integer (possibly surrounded by spaces), then this is
 * taken to be the position to seek to before reading.
 *
 * prepareFiles will process them in a manner much like the following shell
 * commands:
 *
 * if [ ! -f PEER.input ]; then
 * 	if [ -f PEER ]; then mv PEER PEER.input
 * 	elif [ -f PEER.output ]; then mv PEER.output PEER; fi
 * fi
 *
 * At this point PEER.input is opened for reading if it exists.
 *
 * The checkpoint file is left as-is unless the PEER.input file
 * happens to be newer that the checkpoint file.
 */
static void prepareFiles (Tape tape)
{
  bool inpExists ;
  bool outExists ;
  bool newExists ;

#if 0
  /* flush any in memory articles to disk */
  if (tape->head != NULL && tape->outFp != NULL)
    flushTape(tape) ;
#endif

  tape->tellpos = 0 ;

  /*  First time through, or something external has set checkNew */
  if (tape->lastRotated == 0 || tape->checkNew)
    {
      newExists = fileExistsP (tape->handFilename) ;
      if (newExists)
        notice ("%s new hand-prepared backlog file", tape->peerName) ;
    }
  else
    newExists = false ;
  
  if (tape->lastRotated == 0)    /* first time here */
    {
      inpExists = fileExistsP (tape->inputFilename) ;
      outExists = fileExistsP (tape->outputFilename) ;
    }
  else
    {
      inpExists = (tape->inFp != NULL) ? true : false ; /* can this ever be true?? */
      outExists = (tape->outFp != NULL && tape->outputSize > 0) ? true : false ;
    }
  

  /* move the hand-dropped file to the input file if needed. */
  if (newExists && !inpExists)
    {
      if (rename (tape->handFilename,tape->inputFilename) != 0)
        syswarn ("ME oserr rename %s, %s", tape->handFilename,
                 tape->inputFilename);
      else
        {
          notice ("%s grabbing external tape file", tape->peerName) ;
          inpExists = true ;
        }
    }
  
  /* now move the output file to the input file, if needed and only if in
     not in NOROTATE mode. */
  if (outExists && !inpExists && !tape->noRotate)
    {
      if (tape->outFp != NULL)
        {
          fclose (tape->outFp) ;
          tape->outFp = NULL ;
        }
      
      if (rename (tape->outputFilename,tape->inputFilename) != 0)
        syswarn ("ME oserr rename %s, %s", tape->outputFilename,
                 tape->inputFilename) ;
      else
        inpExists = true ;
    }

  /* now open up the input file and seek to the proper position. */
  if (inpExists)
    {
      int c ;
      long flength ;
      
      if ((tape->inFp = fopen (tape->inputFilename,"r+")) == NULL)
        syswarn ("ME fopen %s", tape->inputFilename) ;
      else
        {
          char buffer [64] ;

          if (fgets (buffer,sizeof (buffer) - 1, tape->inFp) == NULL)
            {
              if (feof (tape->inFp))
                {
                  d_printf (1,"Empty input file: %s\n",tape->inputFilename) ;
                  unlink (tape->inputFilename) ;
                }
              else
                syswarn ("ME oserr fgets %s", tape->inputFilename) ;
              
              fclose (tape->inFp) ;
              tape->inFp = NULL ;
              tape->scribbled = false ;
            }
          else
            {
              unsigned int len = strlen (buffer) ;
              long newPos = 0 ;

              if (len > 0 && buffer [len - 1] == '\n')
                buffer [--len] = '\0' ;

              if (len > 0 && strspn (buffer,"0123456789 \n") == len)
                {
                  if (sscanf (buffer,"%ld",&newPos) == 1)
                    {
                      tape->scribbled = true ;
                      tape->tellpos = newPos ;
                    }
                }

              if ((flength = fileLength (fileno (tape->inFp))) < tape->tellpos)
                {
                  warn ("ME tape short: %s %ld %ld", tape->inputFilename,
                        flength, tape->tellpos) ;
                  tape->tellpos = 0 ;
                }
              else if (tape->tellpos == 0)
                rewind (tape->inFp) ;
              else if (fseeko (tape->inFp,tape->tellpos - 1,SEEK_SET) != 0)
                syswarn ("ME oserr fseeko(%s,%ld,SEEK_SET)",
                         tape->inputFilename,tape->tellpos) ;
              else if ((c = fgetc (tape->inFp)) != '\n')
                {
                  while (c != EOF && c != '\n')
                    c = fgetc (tape->inFp) ;
                  
                  if (c == EOF)
                    {
                      fclose (tape->inFp) ;
                      unlink (tape->inputFilename) ;
                      tape->inFp = NULL ;
                      tape->scribbled = false ;
                      prepareFiles (tape) ;
                    }
                  else
                    {
                      long oldPos = tape->tellpos ;
                      
                      tape->changed = true ;
                      checkpointTape (tape) ;

                      warn ("ME internal checkpoint line boundary missed:"
                            " %s %ld vs. %ld",tape->inputFilename,
                              tape->tellpos, oldPos) ;
                    }
                }
            }
        }
    }
  
  tape->lastRotated = theTime() ;
  tape->checkNew = false ;

  /* now open up the output file. */
  if (tape->outFp == NULL)
    {
      if ((tape->outFp = fopen (tape->outputFilename,"a+")) == NULL)
        {
          syswarn ("ME tape open failed (a+) %s", tape->outputFilename) ;
          return ;
        }
      fseeko (tape->outFp,0,SEEK_END) ;
      tape->outputSize = ftello (tape->outFp) ;
      tape->lossage = 0 ;
    }
}


static void tapeCkNewFileCbk (TimeoutId id, void *d UNUSED)
{
  ASSERT (id == ckNewFileId) ;
  ASSERT (tapeCkNewFilePeriod > 0) ;

  tapesSetCheckNew () ;

  ckNewFileId = prepareSleep (tapeCkNewFileCbk,tapeCkNewFilePeriod,NULL) ;
}


/* The timer callback function that will checkpoint all the active tapes. */
static void tapeCheckpointCallback (TimeoutId id, void *d UNUSED)
{
  ASSERT (id == checkPtId) ;
  ASSERT (tapeCkPtPeriod > 0) ;
  
  d_printf (1,"Checkpointing tapes\n") ;
  
  checkPointTapes () ;

  checkPtId = prepareSleep (tapeCheckpointCallback,tapeCkPtPeriod,NULL) ;
}



#if 0
static void flushTape (Tape tape)
{
  QueueElem elem ;

  /* flush out queue to disk. */
  elem = tape->head ;
  while (elem != NULL)
    {
      tape->head = tape->head->next ;
      fprintf (tape->outFp,"%s %s\n", artFileName (elem->article),
               artMsgId (elem->article)) ;
      
      delArticle (elem->article) ;
      
      free (elem) ;
      elem = tape->head ;
    }
  tape->tail = NULL ;
  tape->qLength = 0;
      

  /* I'd rather know where I am each time, and I don't trust all
     fprintf's to give me character counts. */
  tape->outputSize = ftello (tape->outFp) ;
  if (debugShrinking)
    {
      struct stat buf ;
      static bool logged = false ;
      
      fflush (tape->outFp) ;
      if (fstat (fileno (tape->outFp),&buf) != 0 && !logged)
        {
          syslog (LOG_ERR,FSTAT_FAILURE,tape->outputFilename) ;
          logged = true ;
        }
      else if (buf.st_size != tape->outputSize)
        {
          warn ("ME fstat and ftello do not agree for %s",
                tape->outputFilename) ;
          logged = true ;
        }
    }
  
  if (tape->outputHighLimit > 0 && tape->outputSize > tape->outputHighLimit)
    {
      shrinkfile (tape->outFp,tape->outputLowLimit,tape->outputFilename,"a+");
      tape->outputSize = ftello (tape->outFp) ;
    }
}
#endif


static void tapeCleanup (void)
{
  free (tapeDirectory) ;
  tapeDirectory = NULL ;
}
