/*
**  Here be declarations of functions in the InterNetNews library.
*/

#ifndef INN_LIBINN_H
#define INN_LIBINN_H 1

#include "inn/concat.h"
#include "inn/macros.h"
#include "inn/portable-stdbool.h"
#include "inn/xmalloc.h"
#include "inn/xwrite.h"

#include <stdarg.h>    /* va_list */
#include <stdio.h>     /* FILE */
#include <sys/types.h> /* size_t and ssize_t */

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
**  Time and date parsing, generation and handling.
*/
extern int Argify(char *line, char ***argvp);
extern int nArgify(char *line, char ***argvp, int n);
extern int reArgify(char *p, char **argv, int n, bool stripspaces);
extern char *Glom(char **av);
extern bool makedate(time_t, bool local, char *buff, size_t buflen);
extern time_t parsedate_nntp(const char *, const char *, bool local);
extern time_t parsedate_rfc5322(const char *);
extern time_t parsedate_rfc5322_lax(const char *);


/*
**  Wildmat matching.
*/
enum uwildmat {
    UWILDMAT_FAIL = 0,
    UWILDMAT_MATCH = 1,
    UWILDMAT_POISON
};

extern bool is_valid_utf8(const char *start);
extern bool uwildmat(const char *text, const char *pat);
extern bool uwildmat_simple(const char *text, const char *pat);
extern enum uwildmat uwildmat_poison(const char *text, const char *pat);


/*
**  File locking.
*/
enum inn_locktype {
    INN_LOCK_READ,
    INN_LOCK_WRITE,
    INN_LOCK_UNLOCK
};

extern bool inn_lock_file(int fd, enum inn_locktype type, bool block);
extern bool inn_lock_range(int fd, enum inn_locktype type, bool block,
                           off_t offset, off_t size);


/*
**  Miscellaneous utility functions.
*/
extern int getfdlimit(void);
extern int setfdlimit(unsigned int limit);
extern void (*xsignal(int signum, void (*sigfunc)(int)))(int);
extern void (*xsignal_norestart(int signum, void (*sigfunc)(int)))(int);
extern void xsignal_mask(void);
extern void xsignal_unmask(void);
extern void xsignal_enable_masking(void);
extern void xsignal_forked(void);


/*
**  Headers-related routines.
**
**  Do not test HAVE_CANLOCK.  This relieves customers of /usr/include/inn from
**  the need to guess whether INN was built with Cancel-Lock support in order
**  to get a header that matches the installed libraries.
*/
extern bool gen_cancel_lock(const char *msgid, const char *username,
                            char **canbuff);
extern bool gen_cancel_key(const char *hdrcontrol, const char *hdrsupersedes,
                           const char *username, char **canbuff);
extern bool verify_cancel_key(const char *c_key_header,
                              const char *c_lock_header);
extern char *GenerateMessageID(char *domain);
extern bool IsValidMessageID(const char *string, bool stripspaces,
                             bool laxsyntax);
extern bool IsValidDomain(const char *string);
extern bool IsValidHeaderName(const char *string);
extern bool IsValidHeaderBody(const char *string);
extern bool IsValidHeaderField(const char *string);
extern const char *skip_cfws(const char *p);
extern const char *skip_fws(const char *p);
extern char *spaced_words_without_cfws(const char *p);
extern void HeaderCleanFrom(char *from);
extern struct _DDHANDLE *DDstart(FILE *FromServer, FILE *ToServer);
extern void DDcheck(struct _DDHANDLE *h, char *group);
extern char *DDend(struct _DDHANDLE *h);


/* Various checks. */
extern bool IsValidArticleNumber(const char *string);
extern bool IsValidKeyword(const char *string) __attribute__((__pure__));
extern bool IsValidRange(char *string);


/* NNTP functions. */
extern int NNTPlocalopen(FILE **FromServerp, FILE **ToServerp, char *errbuff,
                         size_t len);
extern int NNTPremoteopen(int port, FILE **FromServerp, FILE **ToServerp,
                          char *errbuff, size_t len);
extern int NNTPconnect(const char *host, int port, FILE **FromServerp,
                       FILE **ToServerp, char *errbuff, size_t len);
extern int NNTPsendarticle(char *text, FILE *ToServer, bool terminate);
extern int NNTPsendpassword(char *server, FILE *FromServer, FILE *ToServer);


/* clientlib compatibility functions. */
extern FILE *ser_rd_fp;
extern FILE *ser_wr_fp;
extern char ser_line[];

extern char *getserverbyfile(char *file);
extern int server_init(char *host, int port);
extern int handle_server_response(int response, char *host);
extern void put_server(const char *text);
extern int get_server(char *buff, int buffsize);
extern void close_server(void);


/* Opening the active file on a client. */
extern FILE *CAopen(FILE *FromServer, FILE *ToServer);
extern FILE *CAlistopen(FILE *FromServer, FILE *ToServer, const char *request);
extern FILE *CA_listopen(char *pathname, FILE *FromServer, FILE *ToServer,
                         const char *request);
extern void CAclose(void);


/* Other useful functions. */
/* Return the fully-qualified domain name of the local system in
   newly-allocated memory, or NULL if it cannot be discovered.  The caller is
   responsible for freeing.  If the host's domain cannot be found in DNS, use
   the domain argument as a fallback. */
extern char *inn_getfqdn(const char *domain);
extern char *GetModeratorAddress(FILE *FromServer, FILE *ToServer, char *group,
                                 char *moderatormailer);


/* Reserved file descriptors for use with Fopen(). */
#define TEMPORARYOPEN 0
#define INND_HISTORY  1
#define INND_HISLOG   2
#define DBZ_DIR       3
#define DBZ_BASE      4

/* Hash functions. */
typedef struct {
    char hash[16];
} HASH;

extern HASH Hash(const void *value, const size_t len);
/* Return the hash of a case mapped Message-ID. */
extern HASH HashMessageID(const char *MessageID);
extern bool HashEmpty(const HASH hash);
extern void HashClear(HASH *hash);
extern char *HashToText(const HASH hash);
extern HASH TextToHash(const char *text);
extern int HashCompare(const HASH *h1, const HASH *h2);


/* Miscellaneous. */
extern int dbzneedfilecount(void);
extern bool MakeDirectory(char *Name, bool Recurse);
extern int xread(int fd, char *p, off_t i);
extern int GetResourceUsage(double *usertime, double *systime);
extern void Radix32(unsigned long value, char *buff);
extern char *ReadInDescriptor(int fd, struct stat *Sbp);
extern char *ReadInFile(const char *name, struct stat *Sbp);
extern FILE *xfopena(const char *p);
extern bool fdreserve(int fdnum);
extern FILE *Fopen(const char *name, const char *mode, int fdindex);
extern int Fclose(FILE *fp);

END_DECLS

/* <ctype.h>'s isspace includes \n, which is not what we want. */
#define ISWHITE(c) ((c) == ' ' || (c) == '\t')

#endif /* INN_LIBINN_H */
