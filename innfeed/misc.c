/* -*- c -*-
 *
 * Author:      James Brister <brister@vix.com> -- berkeley-unix --
 * Start Date:  Wed Dec 27 17:10:18 1995
 * Project:     INN (innfeed)
 * File:        misc.c
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
 * Description: 
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

#include <arpa/nameser.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <signal.h>
#include <syslog.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>

/* FIXME: Default to a max length of 256 characters for path names if the
   host headers doesn't give better information.  Should be replaced by the
   code from Stevens. */
#ifndef PATH_MAX
# define PATH_MAX 256
#endif

#include "libinn.h"

#include "endpoint.h"
#include "misc.h"
#include "msgs.h"
#include "tape.h"

u_int openfds ;
int debuggingOutput ;
u_int loggingLevel ;
char **PointersFreedOnExit ;

bool debuggingDump = true ;
extern void (*gPrintInfo) (void) ;
void (*gCleanUp) (void) = 0 ;


/* Log a message to stderr, called from warn or die.  Mostly the same as the
   standard error_log_stderr, but prepends the date to each line. */
void
error_log_stderr_date(int len UNUSED, const char *fmt, va_list args, int err)
{
    char timebuff[30];
    time_t now;
    struct tm *tm;

    now = time(NULL);
    tm = localtime(&now);
    strftime(timebuff, sizeof(timebuff), "%Y-%m-%d %H:%M:%S", tm);
    fprintf(stderr, "%s %s: ", timebuff,
            (error_program_name ? error_program_name : "UNKNOWN"));
    vfprintf(stderr, fmt, args);
    if (err) fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
}

/* If desired, print out the state of innfeed, call a cleanup function, and
   then dump core.  Used as an exit handler for die. */
int
dump_core(void)
{
#if SNAPSHOT_ON_DIE
    if (debuggingDump && gPrintInfo != NULL)
        (*gPrintInfo)();
#endif

    if (gCleanUp != NULL)
        (*gCleanUp)();
  
    if (CORE_DIRECTORY != NULL)
        chdir(CORE_DIRECTORY);
    else
        chdir(getTapeDirectory());
  
    sleep(5);
    abort();

    /* Not reached. */
    return 1;
}

/* An alternate version of die, used when we don't want to dump core.  This
   should somehow eventually be phased out to simplify things; it's
   basically a copy of die() from lib/error.c that ignores the cleanup
   handler and has innfeed's handlers hard-coded (ugh). */
void
logAndExit(int status, const char *format, ...)
{
    va_list args;
    int length;

    va_start(args, format);
    length = vsnprintf(NULL, 0, format, args);
    va_end(args);
    va_start(args, format);
    error_log_stderr_date(length, format, args, 0);
    va_end(args);
    va_start(args, format);
    error_log_syslog_err(length, format, args, 0);
    va_end(args);
    exit(status);
}



void d_printf (u_int level, const char *fmt, ...) 
{
  static pid_t myPid ;
  char timeString [30] ;
  time_t now ;
  va_list ap ;
    
  if (myPid == 0)
    myPid = getpid ()  ;
  
  if (loggingLevel < level)
    return ;
  
  now = theTime() ;
  strcpy (timeString, ctime (&now) + 4) ; /* strip off leading day name */
  timeString [15] = '\0' ;      /* strip off trailing year and newline */

  va_start (ap, fmt) ;
  fprintf (stderr, "%s %s[%ld]: ",timeString,
           (error_program_name ? error_program_name : "UNKNOWN"),
           (long) myPid) ;
  vfprintf (stderr, fmt, ap) ;
  va_end (ap) ;
}

void logOrPrint (int level, FILE *fp, const char *fmt, ...)
{
  va_list ap ;

  va_start (ap,fmt) ;
  if (fp != NULL)
    {
      vfprintf (fp,fmt,ap) ;
      fputc ('\n',fp) ;
    }
  else
    {
      char buffer [512] ;      /* gag me */

      vsnprintf (buffer,sizeof (buffer),fmt,ap) ;
      syslog (level,buffer) ;
    }
  va_end (ap) ;
}



/* return true if the file exists and is a regular file. */
bool fileExistsP (const char *filename)
{
  struct stat buf ;

  if (stat (filename,&buf) < 0)
    return false ;

  return (S_ISREG (buf.st_mode) ? true : false) ;
}


bool isDirectory (const char *filename)
{
  struct stat buf ;

  if (stat (filename,&buf) < 0)
    return false ;

  return (S_ISDIR (buf.st_mode) ? true : false) ;
}



bool getNntpResponse (char *p, int *code, char **rest)
{
  bool rval = true ;
  int cd = 0 ;
  int digits = 0 ;

  if (rest)
    *rest = 0 ;
  *code = 0 ;

  if (p == NULL)
    return false ;
  
  while (*p && CTYPE (isspace, *p))
    p++ ;

  while (*p && CTYPE (isdigit, *p))
    {
      digits++ ;
      cd = (cd * 10) + (*p - '0') ;
      p++ ;
    }

  if (digits != 3)
    return false ;
  
  if (*p == '-')
    p++ ;
  
  while (*p && CTYPE (isspace, *p))
    p++ ;
      
  if (rest)
    *rest = p ;

  *code = cd ;
  
  return rval ;
}



/* Pull out a message id from a response on to a streaming command */
char *getMsgId (const char *p)
{
  const char *q ;
  char *rval ;
  
  while (*p && CTYPE (isspace, *p)) p++ ;
  while (*p && !CTYPE (isspace, *p)) p++ ; /* skip response code */
  while (*p && CTYPE (isspace, *p)) p++ ;

  if ( *p == '\0' )
    return NULL ;

  q = p ;
  while ( *q && !CTYPE (isspace, *q) )
    q++ ;
  
  rval = MALLOC ((size_t) (q - p + 1)) ;
  assert (rval != NULL) ;

  strncpy (rval,p,(size_t) (q - p)) ;
  rval [q - p] = '\0' ;
  
  return rval ;
}




char *findNonBlankString (char *ptr, char **tail)
{
  char *p, *q ;

  for (p = ptr ; *p && CTYPE (isspace, *p) ; p++)
    /* nada */ ;
  if ( ! *p )
    return NULL ;

  for (q = p ; *q && !CTYPE (isspace, *q) ; q++)
    /* nada */ ;

  *tail = q ;

  return p ;
}


/* strtok can't handle zero length tokens. */
char *mystrtok (char *line, const char *sep)
{
  static char *newPoint ;
  char *oldline ;
  
  if (line == NULL && newPoint == NULL)
    return NULL ;

  if (line != NULL)
    {
      oldline = line ;
      while (*line != '\0' && strchr (sep,*line) == NULL)
        line++ ;

      if (*line == '\0')
        newPoint = NULL ;
      else
        {
          newPoint = line + 1 ;
          *line = '\0' ;
        }
    }
  else
    {
      if (newPoint == NULL)
        return NULL ;
      
      oldline = newPoint ;
      line = oldline ;
      
      while (*line != '\0' && strchr (sep,*line) == NULL)
        line++ ;

      if (*line == '\0')
        newPoint = NULL ;
      else
        {
          newPoint = line + 1 ;
          *line = '\0' ;
        }
    }

  return oldline ;
}



void trim_ws (char *string)
{
  char *p ;
  u_int len ;

  assert (string != NULL) ;

  len = strlen (string) ;
  if (len == 0)
    return ;
  
  for (p = string + len - 1 ; p >= string && CTYPE (isspace, *p) ; p--)
    /* nada */ ;
  *++p = '\0' ;
}


#if 0
/* Scribble on top of memory we're about to free. */
void deadBeef (void *base, size_t byteCount)
{
  unsigned char *b = (unsigned char *) base ;
  int i ;

#if 0

  memset (base, 0, byteCount) ;

#else

  assert (b != NULL) ;

  for (i = 0 ; i < ((int) byteCount) - 4 ; i += 4)
    {
#if 0
      *((int *) (b + i)) = 0xdeadbeef ;
#else
      b [i + 0] = (unsigned char) 0xde ;
      b [i + 1] = (unsigned char) 0xad ;
      b [i + 2] = (unsigned char) 0xbe ;
      b [i + 3] = (unsigned char) 0xef ;
#endif
    }
  
  switch (byteCount % 4)
    {
      case 0:
        *(b + i + 3) = (unsigned char) 0xef ;
        
      case 3:
        *(b + i + 2) = (unsigned char) 0xbe ;

      case 2:
        *(b + i + 1) = (unsigned char) 0xad ;

      case 1:
        *b = (unsigned char) 0xde ;
    }

#endif
}
#endif 

/* Not using plain flock or lockf 'cause I don't want to waste file
   descriptors. This routine is based on the file shlock.c from INN. */
bool lockFile (const char *fileName)
{
  char buff [20] ;
  char tmpName [PATH_MAX], realName [PATH_MAX] ;
  char *p ;
  int fd, i ;
  pid_t pid = getpid () ;

  strcpy (realName,fileName) ;
  if ((p = strrchr (realName, '/')) != NULL)
    {
      *p = '\0' ;
      sprintf (tmpName, "%s/lockf%ld", realName, (long) pid) ;
      *p = '/' ;
    }
  else
    sprintf (tmpName, "lockf%ld", (long) pid) ;
  
  /* Create the temporary name for the lock file. */
  while ((fd = open (tmpName, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0)
    {
      switch (errno)
        {
          default:
            unlink (tmpName) ;
            syslog (LOG_ERR,NO_OPEN_LOCK,tmpName) ;
            return false ;

          case EEXIST:
            if (unlink (tmpName) < 0)
              {
                syslog (LOG_ERR,NO_UNLINK_LOCK,tmpName) ;
                return false ;
              }
            break;
        }
    }

  /* stick our pid in the temp file. */
  sprintf (buff,"%ld\n",(long) pid) ;
  if (write (fd,buff,(size_t) strlen (buff)) != (int) strlen (buff))
    {
      syslog (LOG_ERR,NO_WRITE_LOCK_PID) ;
      close (fd) ;
      unlink (tmpName) ;
      return false ;
    }
  close (fd) ;

  /* now link the real name to the temp file. */
  while (link (tmpName,realName) < 0)
    {
      switch (errno) 
        {
          default:              /* opps. bailing out. */
            syslog (LOG_ERR,NO_LINK_LOCK,realName) ;
            unlink (tmpName) ;
            return false ;

          case EEXIST:          
            /* the real lock file exists. So pull out the pid in there and
               see if that process is still alive. */
            if ((fd = open (realName,O_RDONLY)) < 0)
              {
                syslog (LOG_ERR,NO_OPEN_LOCK,realName) ;
                unlink (tmpName) ;
                return false ;
              }

            if ((i = read (fd,buff,sizeof (buff) - 1)) <= 0)
              {
                close (fd) ;
                unlink (tmpName) ;
                return false ;
              }
            close (fd) ;
            
            buff [i] = '\0' ;
            pid = (pid_t) atol (buff) ;
            if (pid <= 0)
              {
                syslog (LOG_ERR,BAD_PID,realName,buff) ;
                unlink (tmpName) ;
                return false ;
              }

            /* now send a null signal to the process named inside to see if
               it's still alive. */
            if (kill (pid,0) == 0)
              {
                syslog (LOG_ERR,LOCK_EXISTS,realName,(int) pid) ;
                unlink (tmpName) ;
                return false ;    /* process is still alive */
              }

            /* process that took out the lock is gone */
            if (unlink (realName) < 0)
              {
                syslog (LOG_ERR,NO_UNLINK_LOCK,realName) ;
                unlink (tmpName) ;
                return false ;
              }
        }
    }

  unlink (tmpName) ;

  return true ;
}


void unlockFile (const char *lockfile)
{
  (void) unlink (lockfile) ;
}


bool endsIn (const char *string, const char *tail)
{
  size_t len = strlen (tail) ;
  size_t slen = strlen (string) ;

  if (slen < len)
    return false ;
  else if (strcmp (string + slen - len, tail) == 0)
    return true ;
  else
    return false ;
}

      
/* append the contents of src to dest. src is removed if append if
   successful */
bool appendFile (const char *dest, const char *src)
{
  FILE *inTmp, *outTmp ;
  char buff [BUFSIZ] ;
  size_t rval ;

      /* append the outputFilename file to the inputFilename file */
  if ((outTmp = fopen (dest, "a")) == NULL)
    die ("fopen (%s): %s",dest, strerror (errno)) ;
  if ((inTmp = fopen (src, "r")) == NULL)
    die ("fopen (%s): %s",src, strerror (errno)) ;

  while ((rval = fread (buff,sizeof (char),BUFSIZ,inTmp)) > 0)
    {
      if (fwrite (buff,sizeof (char), rval, outTmp) != rval)
        die ("fwrite: %s", strerror (errno)) ;
    }

  if (ferror (inTmp))
    die ("Error on inTmp in newTape") ;
  if (ferror (outTmp))
    die ("Error on outTmp in newTape") ;

  if (fclose (inTmp) != 0)
    die ("fclose (inTmp): appendFile (%s,%s): %s",dest,src,strerror (errno)) ;
      
  if (fclose (outTmp) != 0)
    die ("fclose (outTmp): appendFile (%s,%s): %s",dest,src,strerror (errno)) ;

  if (unlink (src) != 0)
    die ("unlink (%s): %s", src, strerror (errno)) ;

  return true ;
}


/* return true if file1 is older than file2 */
bool isOlder (const char *file1, const char *file2)
{
  struct stat buf1 ;
  struct stat buf2 ;

  if (stat (file1,&buf1) < 0)
    return false ;

  if (stat (file2,&buf2) < 0)
    return false ;

  return ((buf1.st_mtime < buf2.st_mtime) ? true : false) ;
}


void freeCharP (char *charp)
{
  FREE (charp) ;
}


/* return the length of the file reference by the given file descriptor */
long fileLength (int fd)
{
  struct stat buf ;

  if (fstat (fd,&buf) < 0)
    return false ;

  return ((long) buf.st_size) ;
}



const char *boolToString (bool val)
{
  return val ? "true" : "false" ;
}

void addPointerFreedOnExit (char *pointerToFree)
{
  static int totalPointers = 0 ;
  static int nextPointer = 0 ;

  if (nextPointer == 0 || nextPointer == totalPointers - 1)
    {
      register int i;

      totalPointers += 16 ;
      if (PointersFreedOnExit == NULL)
	PointersFreedOnExit = ALLOC (char *, totalPointers) ;
      else
	PointersFreedOnExit =
	  REALLOC (PointersFreedOnExit, char *, totalPointers) ;

      ASSERT (PointersFreedOnExit != NULL) ;

      for (i = nextPointer; i < totalPointers; i++)
	PointersFreedOnExit [i] = NULL;
    }
  PointersFreedOnExit [nextPointer++] = pointerToFree ;
}

/* malloc a buffer and build the filename in it. */
char *buildFilename (const char *directory, const char *fname)
{
  int len = 0 ;
  char *p = NULL ;

  if (fname == NULL)
    return NULL ;

  if (directory == NULL)
    directory = "." ;
  
  len = strlen (directory) + strlen (fname) + 2 ;

  if (len < pathMax(directory) - 2)
    {
      p = malloc (len + 1) ;
      p [0] = '\0' ;
      if (fname [0] != '/')
        {
          strcat (p,directory) ;
          if (p [strlen(p) - 1] != '/')
            strcat (p,"/") ;
        }
      strcat (p,fname) ;
    }

  return p ;
}



/* borrows heavily from the shrinkfile program by chongo. */
bool shrinkfile (FILE *fp, long size, char *name, const char *mode)
{
  long currlen = ftello (fp) ;
  char *tmpname ;
  char buffer [BUFSIZ] ;
  FILE *tmpFp ;
  int c ;
  int i ;
  int fd ;

  if (currlen <= size)
    {
      d_printf (1,"No need to shrink file (%s %ld vs %ld\n",
               name,size,currlen) ;
      return true ;
    }

  /* create a temp file. */
  tmpname = concat (name,".XXXXXX",(char *)0) ;
  fd = mkstemp (tmpname) ;

  if (fd < 0)
    {
      syslog (LOG_ERR,SHRINK_TEMP_CREATE,name) ;
      FREE (tmpname) ;
      return false ;
    }

  if ((tmpFp = fdopen (fd,"w")) == NULL)
    {
      syslog (LOG_ERR,SHRINK_TEMP_OPEN,tmpname) ;
      FREE (tmpname) ;
      return false ;
    }

  if (fseeko (fp,currlen - size,SEEK_SET) != 0)
    {
      fclose (tmpFp) ;
      syslog (LOG_ERR,SHRINK_SEEK,currlen - size,name) ;
      FREE(tmpname) ;
      return false ;
    }

  /* find the end of the next line in the shrinking file. */
  while ((c = fgetc (fp)) != '\n')
    if (c == EOF)
      {
        syslog (LOG_WARNING,SHRINK_NONL,name) ;
        fclose (tmpFp) ;
        fseeko (fp,currlen,SEEK_SET) ;
        FREE(tmpname) ;
        return false ;
    }

  /* copy the tail of the shrinking file to the temp file. */
  while ((i = fread (buffer,1,sizeof (buffer),fp)) > 0)
    {
      if (fwrite (buffer,1,i,tmpFp) != (size_t) i)
        {
          fclose (tmpFp) ;
          syslog (LOG_ERR,SHRINK_WRITETMP,tmpname) ;
          fseeko (fp,currlen, SEEK_SET) ;
          FREE(tmpname) ;
          return false ;
        }
    }

  if (i < 0)
    logAndExit (1,SHRINK_READ,name) ;

  fclose (tmpFp) ;

  if (unlink (name) != 0)
    logAndExit (1,UNLINK_FAILED,name) ;

  /* we're in the same directory so this is ok. */
  if (rename (tmpname,name) != 0)
    logAndExit (1,RENAME_FAILED,tmpname,name) ;
  
  if (freopen (name,mode,fp) != fp)
    logAndExit (1,SHRINK_FREOPEN,name) ;

  fseeko (fp,0,SEEK_END) ;
  size = ftello (fp) ;
  
  syslog (LOG_WARNING,FILE_SHRUNK,name,currlen,size) ;

  FREE(tmpname) ;
  
  return true ;
}



long pathMax (const char *pathname)
{
  static long rval = 0 ;

  (void) pathname ;
  
  if (rval > 0)
    return rval ;
  
#if defined (PATH_MAX)

  rval = PATH_MAX ;

#elif defined (_POSIX_PATH_MAX)

  rval = _POSIX_PATH_MAX ;

#elif defined (DO_HAVE_PATHCONF) && defined (_PC_PATH_MAX)

  if (pathname == NULL)
    pathname = "/tmp" ;
  
  rval = pathconf (pathname,_PC_PATH_MAX) ;

#else

  rval = 255 ;
  if (!logged) 
    {
      syslog (LOG_ERR,NO_PATH_MAX,rval) ;
      logged = true ;
    }
  
#endif

  return rval ;
}
