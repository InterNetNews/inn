/*  $Id$
**
**  Miscellaneous utility functions for innfeed.
**
**  Written by James Brister <brister@vix.com>
*/

#if ! defined ( misc_h__ )
#define misc_h__

#include "config.h"

#include <stdarg.h>
#include <sys/types.h>

/* These typedefs are all here because C is too stupid to let me multiply
   define typedefs to the same things (as C++ will). Hence I can't redeclare
   the typedefs to get around recursive header file includes (like host.h and
   connection.h would need if they contained their own typedefs). */

typedef struct article_s *Article ;             /* see article.h */
typedef struct buffer_s *Buffer ;               /* see buffer.h */
typedef struct commander_s *Commander ;         /* see commander.h */
typedef struct config_s *Config ;               /* see config.h */
typedef struct connection_s *Connection ;       /* see connection.h */
typedef struct endpoint_s *EndPoint ;           /* see endpoint.h */
typedef struct host_s *Host ;                   /* see host.h */
typedef struct innlistener_s *InnListener ;     /* see innlistener.h */
typedef struct tape_s *Tape ;                   /* see tape.h */

typedef int TimeoutId ;                         /* see endpoint.h */
typedef enum {                                  /* see endpoint.h */
  IoDone, IoIncomplete, IoFailed, IoEOF, IoProgress
} IoStatus ; 

typedef void (*EndpRWCB) (EndPoint e,           /* see endpoint.h */
                          IoStatus i, Buffer *b, void *d) ;
typedef void (*EndpTCB) (TimeoutId tid, void *d) ; /* see endpoint.h */
typedef void (*EndpWorkCbk) (EndPoint ep, void *data) ;


/* debugging information */
extern unsigned int loggingLevel ;     /* if 0 then d_printf is a no-op */

/* used by timeToString with strftime(3) */
extern char *timeToStringFormat ;

/* the current count of file desccriptors */
extern unsigned int openfds ;

/* if level <= loggingLevel then print */
void d_printf (unsigned int level, const char *fmt, ...)
    __attribute__((__format__ (printf, 2, 3)));

/* for the gethostbyname() error code */
const char *host_err_str (void) ;

/* parse a reponse line into it's code and body. *rest will end up pointing
   into the middle of p */
bool getNntpResponse (char *p, int *code, char **rest) ;

/* parse out the first field of a nntp response code as a message-id. Caller
   must free the return value when done. */
char *getMsgId (const char *p) ;

/* pick out the next non-blank string inside PTR. TAIL is set to point at
   the first blank (or NULL) after the string. Returns a pointer into PTR */
char *findNonBlankString (char *ptr, char **tail) ;

/* if fp is not NULL then print to it, otherwise syslog at the level. */
void logOrPrint (int level, FILE *fp, const char *fmt, ...)
    __attribute__((__format__(printf, 3, 4)));

/* Error handling functions for use with warn and die. */
void error_log_stderr_date(int len, const char *fmt, va_list args, int err);

/* Do cleanup and then abort, for use with die. */
int dump_core(void);

/* Alternate die that doesn't invoke an error handler. */
void logAndExit (int exitVal, const char *fmt, ...)
    __attribute__((__format__(printf, 2, 3)));

/* return true of the file exists and is a regular file */
bool fileExistsP (const char *filename) ;

/* return true if file exists and is a directory */
bool isDirectory (const char *filename) ;

char *mystrtok (char *string, const char *sep) ;

/* remove trailing whitespace */
void trim_ws (char *string) ;

/* locks the peer and returns true or returns false */
bool lockPeer (const char *peerName) ;

/* return true if the end of string matches tail. */
bool endsIn (const char *string, const char *tail) ;

/* scribble over then free up the null-terminated string */
void freeCharP (char *charp) ;

/* append the contents of src to dest. src is removed if append if
   successful */
bool appendFile (const char *dest, const char *src) ;

/* return the length of the file reference by the given file descriptor */
long fileLength (int fd) ;

bool lockFile (const char *fileName) ;
void unlockFile (const char *lockfile) ;


/* return true if file1 is older than file2 */
bool isOlder (const char *file1, const char *file2) ;

/* converts val into a printable string */
const char *boolToString (bool val) ;

/* strftime with "%a %b %d %H:%M:%S %Y" (like ctime without linefeed) */
char* timeToString (time_t time, char* buffer, size_t size) ;

/* memory leak checker helper. */
void addPointerFreedOnExit (char *pointerToFree) ;

/* string the file opened by FP to the give SIZE. The NEWNAME is the name
   of the file to have after truncation. FP will be reopened for writing on
   the new file and will be positioned at the end */
bool shrinkfile (FILE *fp, long size, char *name, const char *mode) ;

/* max length of pathname */
long pathMax (const char *pathname) ;

#define ASSERT(x) do{if (!(x))die("assertion -- %s -- failed in file %s line %d",#x,__FILE__,__LINE__);}while(0)

#define INDENT_INCR 5
#define INDENT_BUFFER_SIZE 80
#if ! defined (MIN)
#define MIN(A,B) ((A) < (B) ? (A) : (B))
#endif

#endif /* misc_h__ */
