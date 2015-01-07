/*  $Id$
**
**  Here be declarations of functions in the InterNetNews library.
*/

#ifndef INN_LIBINN_H
#define INN_LIBINN_H 1

#include <inn/defines.h>
#include "inn/xmalloc.h"
#include "inn/xwrite.h"

#include <stdarg.h>             /* va_list */
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
struct sockaddr;
struct sockaddr_in;
struct in_addr;

BEGIN_DECLS

/*
**  TIME AND DATE PARSING, GENERATION, AND HANDLING
*/
extern int      Argify(char *line, char ***argvp);
extern int      nArgify(char *line, char ***argvp, int n);
extern int      reArgify(char *p, char **argv, int n, bool stripspaces);
extern char *   Glom(char **av);
extern bool     makedate(time_t, bool local, char *buff, size_t buflen);
extern time_t   parsedate_nntp(const char *, const char *, bool local);
extern time_t   parsedate_rfc5322(const char *);
extern time_t   parsedate_rfc5322_lax(const char *);



/*
**  WILDMAT MATCHING
*/
enum uwildmat {
    UWILDMAT_FAIL   = 0,
    UWILDMAT_MATCH  = 1,
    UWILDMAT_POISON
};

extern bool             is_valid_utf8(const char *start);
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
extern char *   concat(const char *first, ...);
extern char *   concatpath(const char *base, const char *name);
extern void     daemonize(const char *path);
extern int      getfdlimit(void);
extern int      setfdlimit(unsigned int limit);
extern void     (*xsignal(int signum, void (*sigfunc)(int)))(int);
extern void     (*xsignal_norestart(int signum, void (*sigfunc)(int)))(int);


/* Headers. */
extern char *           GenerateMessageID(char *domain);
extern void             InitializeMessageIDcclass(void);
extern bool             IsValidMessageID(const char *string, bool stripspaces);
extern bool             IsValidHeaderName(const char *string);
extern const char *     skip_cfws(const char *p);
extern const char *     skip_fws(const char *p);
extern void             HeaderCleanFrom(char *from);
extern struct _DDHANDLE * DDstart(FILE *FromServer, FILE *ToServer);
extern void               DDcheck(struct _DDHANDLE *h, char *group);
extern char *             DDend(struct _DDHANDLE *h);


/* Various checks. */
extern bool             IsValidArticleNumber(const char *string);
extern bool             IsValidKeyword(const char *string)
    __attribute__ ((__pure__));
extern bool             IsValidRange(char *string);


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

#endif /* INN_LIBINN_H */
