/*  $Id$
**
**  Here be declarations of functions in the InterNetNews library.
*/

#ifndef LIBINN_H
#define LIBINN_H 1

#include "inn/defines.h"

/* Eventually we don't want to include this, since this will be an installed
   header and we don't want to install config.h. */
#include "config.h"

#include <stdio.h>              /* FILE */
#include <sys/types.h>          /* size_t and ssize_t */

/* Earlier versions of INN didn't have <inn/version.h> and some source is
   intended to be portable to different INN versions; it can use this macro to
   determine whether <inn/version.h> is available.  WARNING: This macro will
   be removed in a later version of INN when the library API is finalized.
   Use a configure test for inn/version.h instead. */
#define HAVE_INN_VERSION_H 1

/* Forward declarations to avoid unnecessary includes. */
struct stat;
struct iovec;
struct sockaddr;
struct sockaddr_in;
struct in_addr;

BEGIN_DECLS

/*
**  MEMORY MANAGEMENT
*/

/* The functions are actually macros so that we can pick up the file and line
   number information for debugging error messages without the user having to
   pass those in every time. */
#define xcalloc(n, size)        x_calloc((n), (size), __FILE__, __LINE__)
#define xmalloc(size)           x_malloc((size), __FILE__, __LINE__)
#define xrealloc(p, size)       x_realloc((p), (size), __FILE__, __LINE__)
#define xstrdup(p)              x_strdup((p), __FILE__, __LINE__)
#define xstrndup(p, size)       x_strndup((p), (size), __FILE__, __LINE__)

/* Last two arguments are always file and line number.  These are internal
   implementations that should not be called directly.  ISO C99 says that
   identifiers beginning with _ and a lowercase letter are reserved for
   identifiers of file scope, so while the position of libraries in the
   standard isn't clear, it's probably not entirely kosher to use _xmalloc
   here.  Use x_malloc instead. */
extern void *x_calloc(size_t, size_t, const char *, int);
extern void *x_malloc(size_t, const char *, int);
extern void *x_realloc(void *, size_t, const char *, int);
extern char *x_strdup(const char *, const char *, int);
extern char *x_strndup(const char *, size_t, const char *, int);

/* Failure handler takes the function, the size, the file, and the line. */
typedef void (*xmalloc_handler_type)(const char *, size_t, const char *, int);

/* The default error handler. */
void xmalloc_fail(const char *, size_t, const char *, int);

/* Assign to this variable to choose a handler other than the default, which
   just calls sysdie. */
extern xmalloc_handler_type xmalloc_error_handler;


/*
**  TIME AND DATE PARSING, GENERATION, AND HANDLING
*/
extern bool     makedate(time_t, bool local, char *buff, size_t buflen);
extern time_t   parsedate_nntp(const char *, const char *, bool local);
extern time_t   parsedate_rfc2822(const char *);
extern time_t   parsedate_rfc2822_lax(const char *);


/*
**  WILDMAT MATCHING
*/
enum uwildmat {
    UWILDMAT_FAIL   = 0,
    UWILDMAT_MATCH  = 1,
    UWILDMAT_POISON
};

extern bool             uwildmat(const char *text, const char *pat);
extern bool             uwildmat_simple(const char *text, const char *pat);
extern enum uwildmat    uwildmat_poison(const char *text, const char *pat);


/*
**  FILE LOCKING
*/
enum inn_locktype {
    INN_LOCK_READ,
    INN_LOCK_WRITE,
    INN_LOCK_UNLOCK
};

extern bool     inn_lock_file(int fd, enum inn_locktype type, bool block);
extern bool     inn_lock_range(int fd, enum inn_locktype type, bool block,
			       off_t offset, off_t size);


/*
**  MISCELLANEOUS UTILITY FUNCTIONS
*/
extern void     close_on_exec(int fd, bool flag);
extern char *   concat(const char *first, ...);
extern char *   concatpath(const char *base, const char *name);
extern void     daemonize(const char *path);
extern int      getfdlimit(void);
extern int      nonblocking(int fd, bool flag);
extern int      setfdlimit(unsigned int limit);
extern ssize_t  xpwrite(int fd, const void *buffer, size_t size, off_t offset);
extern void     (*xsignal(int signum, void (*sigfunc)(int)))(int);
extern void     (*xsignal_norestart(int signum, void (*sigfunc)(int)))(int);
extern ssize_t  xwrite(int fd, const void *buffer, size_t size);
extern ssize_t  xwritev(int fd, const struct iovec *iov, int iovcnt);


/* Headers. */
extern char *           GenerateMessageID(char *domain);
extern void             HeaderCleanFrom(char *from);
extern struct _DDHANDLE * DDstart(FILE *FromServer, FILE *ToServer);
extern void               DDcheck(struct _DDHANDLE *h, char *group);
extern char *             DDend(struct _DDHANDLE *h);

/* NNTP functions. */
extern int      NNTPlocalopen(FILE **FromServerp, FILE **ToServerp,
                              char *errbuff, size_t len);
extern int      NNTPremoteopen(int port, FILE **FromServerp,
                               FILE **ToServerp, char *errbuff, size_t len);
extern int      NNTPconnect(const char *host, int port, FILE **FromServerp,
                            FILE **ToServerp, char *errbuff, size_t len);
extern int      NNTPsendarticle(char *, FILE *F, bool Terminate);
extern int      NNTPsendpassword(char *server, FILE *FromServer,
                                 FILE *ToServer);

/* clientlib compatibility functions. */
extern char *   getserverbyfile(char *file);
extern int      server_init(char *host, int port);
extern int      handle_server_response(int response, char *host);
extern void     put_server(const char *text);
extern int      get_server(char *buff, int buffsize);
extern void     close_server(void);

/* Opening the active file on a client. */
extern FILE *   CAopen(FILE *FromServer, FILE *ToServer);
extern FILE *   CAlistopen(FILE *FromServer, FILE *ToServer,
			   const char *request);
extern FILE *   CA_listopen(char *pathname, FILE *FromServer, FILE *ToServer,
			    const char *request);
extern void     CAclose(void);

extern char *    GetFQDN(char *domain);
extern char *    GetModeratorAddress(FILE *FromServer, FILE *ToServer,
                                     char *group, char *moderatormailer); 

#define TEMPORARYOPEN   0
#define INND_HISTORY    1
#define INND_HISLOG     2
#define DBZ_DIR         3
#define DBZ_BASE        4

/* Hash functions */
typedef struct {
    char        hash[16];
} HASH;
extern HASH     Hash(const void *value, const size_t len);
/* Return the hash of a case mapped message-id */
extern HASH     HashMessageID(const char *MessageID);
extern bool     HashEmpty(const HASH hash);
extern void     HashClear(HASH *hash);
extern char *   HashToText(const HASH hash);
extern HASH     TextToHash(const char *text);
extern int      HashCompare(const HASH *h1, const HASH *h2);

/* Miscellaneous. */
extern int      dbzneedfilecount(void);
extern bool     MakeDirectory(char *Name, bool Recurse);
extern int      xread(int fd, char *p, off_t i);
extern int      GetResourceUsage(double *usertime, double *systime);
extern void     Radix32(unsigned long, char *buff);
extern char *   ReadInDescriptor(int fd, struct stat *Sbp);
extern char *   ReadInFile(const char *name, struct stat *Sbp);
extern FILE *   xfopena(const char *p);
extern bool     fdreserve(int fdnum);
extern FILE *   Fopen(const char *p, const char *type, int fd);
extern int      Fclose(FILE *fp);

END_DECLS

/* <ctype.h>'s isspace includes \n, which is not what we want. */
#define ISWHITE(c)              ((c) == ' ' || (c) == '\t')

#endif /* LIBINN_H */
