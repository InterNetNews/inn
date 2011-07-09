/*  $Id$
**
**  Helper routines for the innfeed program.
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
#include <netdb.h>
#include <netinet/in.h>
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

#include "inn/messages.h"
#include "inn/libinn.h"

#include "endpoint.h"
#include "misc.h"
#include "tape.h"

unsigned int openfds ;
int debuggingOutput ;
unsigned int loggingLevel ;
char **PointersFreedOnExit ;

char *timeToStringFormat = 0;
bool debuggingDump = true ;
extern void (*gPrintInfo) (void) ;
void (*gCleanUp) (void) = 0 ;


/* Log a message to stderr, called from warn or die.  Mostly the same as the
   standard message_log_stderr, but prepends the date to each line. */
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
            (message_program_name ? message_program_name : "UNKNOWN"));
    vfprintf(stderr, fmt, args);
    if (err) fprintf(stderr, ": %s", strerror(err));
    fprintf(stderr, "\n");
}

/* If desired, print out the state of innfeed, call a cleanup function, and
   then dump core.  Used as an exit handler for die. */
void
dump_core(void)
{
#if SNAPSHOT_ON_DIE
    if (debuggingDump && gPrintInfo != NULL)
        (*gPrintInfo)();
#endif

    if (gCleanUp != NULL)
        (*gCleanUp)();
  
    chdir(getTapeDirectory());
  
    sleep(5);
    abort();
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
    message_log_syslog_err(length, format, args, 0);
    va_end(args);
    exit(status);
}



void d_printf (unsigned int level, const char *fmt, ...) 
{
  static pid_t myPid ;
  char timeString [30] ;
  time_t now ;
  struct tm *tm ;
  va_list ap ;
    
  if (myPid == 0)
    myPid = getpid ()  ;
  
  if (loggingLevel < level)
    return ;
  
  now = theTime() ;
  tm = localtime (&now);
  strftime (timeString, sizeof(timeString), "%b %d %H:%M:%S", tm);

  va_start (ap, fmt) ;
  fprintf (stderr, "%s %s[%ld]: ",timeString,
           (message_program_name ? message_program_name : "UNKNOWN"),
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
      syslog (level,"%s",buffer) ;
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
  
  while (*p && isspace((unsigned char) *p))
    p++ ;

  while (*p && isdigit((unsigned char) *p))
    {
      digits++ ;
      cd = (cd * 10) + (*p - '0') ;
      p++ ;
    }

  if (digits != 3)
    return false ;
  
  if (*p == '-')
    p++ ;
  
  while (*p && isspace((unsigned char) *p))
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
  
  while (*p && isspace((unsigned char) *p)) p++ ;
  while (*p && !isspace((unsigned char) *p)) p++ ; /* skip response code */
  while (*p && isspace((unsigned char) *p)) p++ ;

  if ( *p == '\0' )
    return NULL ;

  q = p ;
  while ( *q && !isspace((unsigned char) *q) )
    q++ ;

  rval = xstrndup (p, q - p) ;

  return rval ;
}




char *findNonBlankString (char *ptr, char **tail)
{
  char *p, *q ;

  for (p = ptr ; *p && isspace((unsigned char) *p) ; p++)
    /* nada */ ;
  if ( ! *p )
    return NULL ;

  for (q = p ; *q && !isspace((unsigned char) *q) ; q++)
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
  unsigned int len ;

  assert (string != NULL) ;

  len = strlen (string) ;
  if (len == 0)
    return ;
  
  for (p = string + len - 1 ; p >= string && isspace((unsigned char) *p) ; p--)
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

  strlcpy (realName,fileName,sizeof (realName)) ;
  if ((p = strrchr (realName, '/')) != NULL)
    {
      *p = '\0' ;
      snprintf (tmpName, sizeof(tmpName), "%s/lockf%ld", realName,
                (long) pid) ;
      *p = '/' ;
    }
  else
    snprintf (tmpName, sizeof(tmpName), "lockf%ld", (long) pid) ;
  
  /* Create the temporary name for the lock file. */
  while ((fd = open (tmpName, O_RDWR | O_CREAT | O_EXCL, 0644)) < 0)
    {
      switch (errno)
        {
          default:
            unlink (tmpName) ;
            syswarn ("ME lock file open: %s", tmpName) ;
            return false ;

          case EEXIST:
            if (unlink (tmpName) < 0)
              {
                syswarn ("ME lock file unlink: %s", tmpName) ;
                return false ;
              }
            break;
        }
    }

  /* stick our pid in the temp file. */
  snprintf (buff,sizeof(buff),"%ld\n",(long) pid) ;
  if (write (fd,buff,(size_t) strlen (buff)) != (int) strlen (buff))
    {
      syswarn ("ME lock file pid-write") ;
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
            syswarn ("ME lock file link: %s", realName) ;
            unlink (tmpName) ;
            return false ;

          case EEXIST:          
            /* the real lock file exists. So pull out the pid in there and
               see if that process is still alive. */
            if ((fd = open (realName,O_RDONLY)) < 0)
              {
                syswarn ("ME lock file open: %s", realName) ;
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
                warn ("ME lock bad-pid info in %s: %s", realName, buff) ;
                unlink (tmpName) ;
                return false ;
              }

            /* now send a null signal to the process named inside to see if
               it's still alive. */
            if (kill (pid,0) == 0)
              {
                warn ("ME lock in-use already: %s by pid %ld", realName,
                      (unsigned long) pid);
                unlink (tmpName) ;
                return false ;    /* process is still alive */
              }

            /* process that took out the lock is gone */
            if (unlink (realName) < 0)
              {
                syswarn ("ME lock file unlink: %s", realName) ;
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
  unlink (lockfile) ;
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
  free (charp) ;
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

char* timeToString(time_t t, char* buffer, size_t size)
{
  static const char defaultFormat[] = "%a %b %d %H:%M:%S %Y" ;
  const struct tm *const tm = localtime(&t);
  strftime (buffer, size,
    timeToStringFormat == 0 ? defaultFormat : timeToStringFormat, tm);
  return buffer;
}

void addPointerFreedOnExit (char *pointerToFree)
{
  static int totalPointers = 0 ;
  static int nextPointer = 0 ;

  if (nextPointer == 0 || nextPointer == totalPointers - 1)
    {
      int i;

      totalPointers += 16 ;
      if (PointersFreedOnExit == NULL)
	PointersFreedOnExit = xmalloc (sizeof(char *) * totalPointers) ;
      else
	PointersFreedOnExit =
	  xrealloc (PointersFreedOnExit, sizeof(char *) * totalPointers) ;

      for (i = nextPointer; i < totalPointers; i++)
	PointersFreedOnExit [i] = NULL;
    }
  PointersFreedOnExit [nextPointer++] = pointerToFree ;
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
      syswarn ("ME error creating temp shrink file for %s", name) ;
      free (tmpname) ;
      return false ;
    }

  if ((tmpFp = fdopen (fd,"w")) == NULL)
    {
      syswarn ("ME error opening temp shrink file %s", tmpname) ;
      free (tmpname) ;
      return false ;
    }

  if (fseeko (fp,currlen - size,SEEK_SET) != 0)
    {
      fclose (tmpFp) ;
      warn ("ME error seeking to point %ld in %s", currlen - size, name) ;
      free (tmpname) ;
      return false ;
    }

  /* find the end of the next line in the shrinking file. */
  while ((c = fgetc (fp)) != '\n')
    if (c == EOF)
      {
        warn ("ME no newline in shrinking file %s", name) ;
        fclose (tmpFp) ;
        fseeko (fp,currlen,SEEK_SET) ;
        free (tmpname) ;
        return false ;
    }

  /* copy the tail of the shrinking file to the temp file. */
  while ((i = fread (buffer,1,sizeof (buffer),fp)) > 0)
    {
      if (fwrite (buffer,1,i,tmpFp) != (size_t) i)
        {
          fclose (tmpFp) ;
          syswarn ("ME fwrite failed to temp shrink file %s", tmpname) ;
          fseeko (fp,currlen, SEEK_SET) ;
          free (tmpname) ;
          return false ;
        }
    }

  if (i < 0)
    logAndExit (1,"ME fread failed on file %s: %s",name, strerror (errno)) ;

  fclose (tmpFp) ;

  if (unlink (name) != 0)
    logAndExit (1,"ME oserr unlink %s: %s",name, strerror (errno)) ;

  /* we're in the same directory so this is ok. */
  if (rename (tmpname,name) != 0)
    logAndExit (1,"ME oserr rename %s, %s: %s", tmpname, name,
                strerror (errno)) ;
  
  if (freopen (name,mode,fp) != fp)
    logAndExit (1,"ME freopen on shrink file failed %s: %s", name,
                strerror (errno)) ;

  fseeko (fp,0,SEEK_END) ;
  size = ftello (fp) ;

  notice ("ME file %s shrunk from %ld to %ld", name, currlen, size) ;

  free (tmpname) ;
  
  return true ;
}



long pathMax (const char *pathname UNUSED)
{
  static long rval = 0 ;

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
