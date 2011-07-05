/*  $Id$
**
**  NetNews Reading Protocol server.
*/

#include "config.h"
#include "portable/socket.h"
#include "portable/time.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>

#include "inn/qio.h"
#include "inn/libinn.h"
#include "inn/nntp.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/vector.h"
#include "inn/timer.h"

#ifdef HAVE_SASL
#include <sasl/sasl.h>
#include <sasl/saslutil.h>
#endif

/*
**  A range of article numbers.
*/
typedef struct _ARTRANGE {
    ARTNUM	Low;
    ARTNUM	High;
} ARTRANGE;

/*
**  Access configuration for each reader.
*/
typedef struct _ACCESSGROUP {
    char *name;
    char *key;
    char *read;
    char *post;
    char *users;
    char *rejectwith;
    int allownewnews;
    bool allowihave;
    int locpost;
    int allowapproved;
    int used;
    int localtime;
    int strippath;
    int nnrpdperlfilter;
    int nnrpdpythonfilter;
    char *fromhost;
    char *pathhost;
    char *organization;
    char *moderatormailer;
    char *domain;
    char *complaints;
    int spoolfirst;
    int checkincludedtext;
    int clienttimeout;
    unsigned long localmaxartsize;
    int readertrack;
    int strippostcc;
    int addinjectiondate;
    int addinjectionpostinghost;
    char *nnrpdposthost;
    unsigned long nnrpdpostport;
    int nnrpdoverstats;
    int backoff_auth;
    char *backoff_db;
    unsigned long backoff_k;
    unsigned long backoff_postfast;
    unsigned long backoff_postslow;
    unsigned long backoff_trigger;
    int nnrpdcheckart;
    int nnrpdauthsender;
    int virtualhost;
    char *newsmaster;
    long maxbytespersecond;
} ACCESSGROUP;

/*
**  What line_read returns.
*/
typedef enum _READTYPE {
    RTeof,
    RTok,
    RTlong,
    RTtimeout
} READTYPE;


/*
**  Structure used by line_read to keep track of what has been read.
*/
struct line {
    char *start;
    char *where;
    size_t remaining;
    size_t allocated;
};


/*
**  Information about the currently connected client.  Eventually, this struct
**  may become the repository for all nnrpd global state, or at least the
**  state that relates to a particular client connection.
*/
struct client {
    char host[SMBUF];
    char ip[INET6_ADDRSTRLEN];
    unsigned short port;
    char serverhost[SMBUF];
    char serverip[INET6_ADDRSTRLEN];
    unsigned short serverport;
};


/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char	*Header;
    int		Length;
    bool	NeedsHeader;
} ARTOVERFIELD;

/*
**  Supported timers.  If you add new timers to this list, also add them to
**  the list of tags in nnrpd.c.
*/
enum timer {
    TMR_IDLE = TMR_APPLICATION, /* Server is completely idle. */
    TMR_NEWNEWS,                /* Executing NEWNEWS command. */
    TMR_READART,                /* Reading an article (SMretrieve). */
    TMR_CHECKART,               /* Checking an article (ARTinstorebytoken). */
    TMR_NNTPREAD,               /* Reading from the peer. */
    TMR_NNTPWRITE,              /* Writing to the peer. */
    TMR_MAX
};

#if	defined(MAINLINE)
#define EXTERN	/* NULL */
#else
#define EXTERN	extern
#endif	/* defined(MAINLINE) */

EXTERN bool	PERMauthorized;
EXTERN bool     PERMcanauthenticate;
#ifdef HAVE_SSL
EXTERN bool     PERMcanauthenticatewithoutSSL;
#endif
EXTERN bool	PERMcanpost;
EXTERN bool     PERMcanpostgreeting;
EXTERN bool	PERMcanread;
EXTERN bool	PERMneedauth;
EXTERN bool	PERMspecified;
EXTERN bool     PERMgroupmadeinvalid;
EXTERN ACCESSGROUP	*PERMaccessconf;
EXTERN bool	Tracing;
EXTERN bool 	Offlinepost;
EXTERN bool 	initialSSL;
EXTERN char	**PERMreadlist;
EXTERN char	**PERMpostlist;
EXTERN struct client Client;
EXTERN char	Username[SMBUF];
extern char	*ACTIVETIMES;
extern char	*HISTORY;
extern char	*ACTIVE;
extern char	*NEWSGROUPS;
extern char	*NNRPACCESS;
EXTERN char	PERMuser[SMBUF];
EXTERN FILE	*locallog;
EXTERN ARTNUM	ARTnumber;	/* Current article number. */
EXTERN ARTNUM	ARThigh;	/* Current high number for group. */
EXTERN ARTNUM	ARTlow;		/* Current low number for group. */
EXTERN unsigned long	ARTcount;	/* Number of articles in group. */
EXTERN long	MaxBytesPerSecond; /* Maximum bytes per sec a client can use, defaults to 0. */
EXTERN long	ARTget;
EXTERN long	ARTgettime;
EXTERN long	ARTgetsize;
EXTERN long	OVERcount;	/* Number of (X)OVER commands. */
EXTERN long	OVERhit;	/* Number of (X)OVER records found in overview. */
EXTERN long	OVERmiss;	/* Number of (X)OVER records found in articles. */
EXTERN long	OVERtime;	/* Number of ms spent sending (X)OVER data. */
EXTERN long	OVERsize;	/* Number of bytes of (X)OVER data sent. */
EXTERN long	OVERdbz;	/* Number of ms spent reading dbz data. */
EXTERN long	OVERseek;	/* Number of ms spent seeking history. */
EXTERN long	OVERget;	/* Number of ms spent reading history. */
EXTERN long	OVERartcheck;	/* Number of ms spent article check. */
EXTERN double	IDLEtime;
EXTERN unsigned long	GRParticles;
EXTERN long	GRPcount;
EXTERN char	*GRPcur;
EXTERN long	POSTreceived;
EXTERN long	POSTrejected;

EXTERN bool     BACKOFFenabled;
EXTERN char	*VirtualPath;
EXTERN int	VirtualPathlen;
EXTERN struct history *History;
EXTERN struct line NNTPline;
EXTERN struct vector *OVextra;
EXTERN int	overhdr_xref;
EXTERN bool     LLOGenable;

extern const char	*ARTpost(char *article, char *idbuff, bool *permanent);
extern void		ARTclose(void);
extern int		TrimSpaces(char *line);
extern void		InitBackoffConstants(void);
extern char		*PostRecFilename(char *ip, char *user);
extern int		LockPostRec(char *path);
extern int		LockPostRec(char *path);
extern void		UnlockPostRec(char *path);
extern int		RateLimit(long *sleeptime, char *path);
extern void		ExitWithStats(int x, bool readconf) __attribute__ ((noreturn));
extern char		*GetHeader(const char *header, bool stripspaces);
extern void		GRPreport(void);
extern bool		NGgetlist(char ***argvp, char *list);
extern bool		PERMartok(void);
extern void		PERMgetaccess(char *nnrpaccess);
extern void		PERMgetpermissions(void);
extern void		PERMlogin(char *uname, char *pass, int* code, char *errorstr);
extern bool		PERMmatch(char **Pats, char **list);
extern bool		ParseDistlist(char ***argvp, char *list);
extern void 		SetDefaultAccess(ACCESSGROUP*);
extern void		Reply(const char *fmt, ...)
    __attribute__((__format__(printf, 1, 2)));
extern void             Printf(const char *fmt, ...)
    __attribute__((__format__(printf, 1, 2)));

extern void             CMDauthinfo     (int ac, char** av);
extern void             CMDcapabilities (int ac, char** av);
extern void             CMDdate         (int ac, char** av);
extern void             CMDfetch        (int ac, char** av);
extern void             CMDgroup        (int ac, char** av);
extern void             CMDhelp         (int ac, char** av);
extern void             CMDlist         (int ac, char** av);
extern void             CMDmode         (int ac, char** av);
extern void             CMDnewgroups    (int ac, char** av);
extern void             CMDnewnews      (int ac, char** av);
extern void             CMDnextlast     (int ac, char** av);
extern void             CMDover         (int ac, char** av);
extern void             CMDpost         (int ac, char** av);
extern void             CMDquit         (int ac, char** av) __attribute__ ((noreturn));
extern void             CMDxgtitle      (int ac, char** av);
extern void             CMDpat          (int ac, char** av);
extern void             CMD_unimp       (int ac, char** av);
#ifdef HAVE_SSL
extern void             CMDstarttls     (int ac, char** av);
#endif

extern bool CMDgetrange(int ac, char *av[], ARTRANGE *rp, bool *DidReply);

/*
**  Run a resolver or authenticator.  The directory is where to look for the
**  command if not fully qualified.  username and password may be NULL to run
**  resolvers.
*/
char *auth_external(struct client *, const char *command,
                    const char *directory, const char *username,
                    const char *password);

void write_buffer(const char *buff, ssize_t len);

extern char *HandleHeaders(char *article);
extern bool ARTinstorebytoken(TOKEN token);

extern int TrackClient(char *client, char* user, size_t len);

#ifdef  DO_PERL
extern void loadPerl(void);
extern void perlAccess(char *user, struct vector *access_vec);
extern void perlAuthenticate(char *user, char *passwd, int *code, char *errorstring, char*newUser);
extern void perlAuthInit(void);
#endif /* DO_PERL */

#ifdef	DO_PYTHON
extern bool PY_use_dynamic;

void PY_authenticate(char *path, char *Username, char *Password, int *code, char *errorstring, char *newUser);
void PY_access(char* path, struct vector *access_vec, char *Username);
void PY_close_python(void);
int PY_dynamic(char *Username, char *NewsGroup, int PostFlag, char **reply_message);
void PY_dynamic_init (char* file);
#endif	/* DO_PYTHON */

void line_free(struct line *);
void line_init(struct line *);
void line_reset(struct line *);
READTYPE line_read(struct line *, int, const char **, size_t *, size_t *);

#ifdef HAVE_SASL
extern sasl_conn_t *sasl_conn;
extern int sasl_ssf, sasl_maxout;
extern sasl_callback_t sasl_callbacks[];

void SASLauth(int ac, char *av[]);
void SASLnewserver(void);
#endif /* HAVE_SASL */
