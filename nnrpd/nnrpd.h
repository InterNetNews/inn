/*  $Id$
**
**  Net News Reading Protocol server.
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
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "storage.h"
#include "inn/vector.h"
#include "inn/timer.h"

/*
**  Maximum input line length, sigh.
*/
#define ART_LINE_LENGTH		1000
#define ART_LINE_MALLOC		1024
#define ART_MAX			1024


/*
**  Some convenient shorthands.
*/
typedef struct in_addr	INADDR;


/*
**  A range of article numbers.
*/
typedef struct _ARTRANGE {
    int		Low;
    int		High;
} ARTRANGE;

/*
** access configuration for each readers
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
    long localmaxartsize;
    int readertrack;
    int strippostcc;
    int addnntppostinghost;
    int addnntppostingdate;
    char *nnrpdposthost;
    int nnrpdpostport;
    int nnrpdoverstats;
    int backoff_auth;
    char *backoff_db;
    long backoff_k;
    long backoff_postfast;
    long backoff_postslow;
    long backoff_trigger;
    int nnrpdcheckart;
    int nnrpdauthsender;
    int virtualhost;
    char *newsmaster;
    long maxbytespersecond;
} ACCESSGROUP;

/*
**  What READline returns.
*/
typedef enum _READTYPE {
    RTeof,
    RTok,
    RTlong,
    RTtimeout
} READTYPE;

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
**  the list of tags in chan.c.
*/
enum timer {
    TMR_IDLE = TMR_APPLICATION, /* Server is completely idle. */
    TMR_MAX
};

#if	defined(MAINLINE)
#define EXTERN	/* NULL */
#else
#define EXTERN	extern
#endif	/* defined(MAINLINE) */

EXTERN bool	PERMauthorized;
EXTERN bool	PERMcanpost;
EXTERN bool	PERMcanread;
EXTERN bool	PERMneedauth;
EXTERN bool	PERMspecified;
EXTERN ACCESSGROUP	*PERMaccessconf;
EXTERN bool	Tracing;
EXTERN bool 	Offlinepost;
EXTERN bool 	initialSSL;
EXTERN char	**PERMreadlist;
EXTERN char	**PERMpostlist;
EXTERN char	ClientHost[SMBUF];
EXTERN char     ServerHost[SMBUF];
EXTERN char	Username[SMBUF];
#ifdef HAVE_INET6
EXTERN char     ClientIpString[INET6_ADDRSTRLEN];
EXTERN char     ServerIpString[INET6_ADDRSTRLEN];
#else
EXTERN char     ClientIpString[20];
EXTERN char     ServerIpString[20];
#endif
EXTERN int	ClientPort;
EXTERN int	ServerPort;
EXTERN char	LogName[256] ;
#ifdef HAVE_SSL
EXTERN BOOL	ClientSSL;
#endif
extern char	*ACTIVETIMES;
extern char	*HISTORY;
extern char	*ACTIVE;
extern char	*NEWSGROUPS;
extern char	*NNRPACCESS;
extern char	NOACCESS[];
EXTERN ARTOVERFIELD	*ARTfields;
EXTERN int	ARTfieldsize;
EXTERN int	SPOOLlen;
EXTERN char	PERMpass[SMBUF];
EXTERN char	PERMuser[SMBUF];
EXTERN FILE	*locallog;
EXTERN int	ARTnumber;	/* Current article number */
EXTERN int	ARThigh;	/* Current high number for group */
EXTERN int	ARTlow;		/* Current low number for group */
EXTERN long	ARTcount;	/* Current number of articles in group */
EXTERN long	MaxBytesPerSecond; /* maximum bytes per sec a client can use, defaults to 0 */
EXTERN long	ARTget;
EXTERN long	ARTgettime;
EXTERN long	ARTgetsize;
EXTERN long	OVERcount;	/* number of XOVER commands */
EXTERN long	OVERhit;	/* number of XOVER records found in .overview */
EXTERN long	OVERmiss;	/* number of XOVER records found in articles */
EXTERN long	OVERtime;	/* number of ms spent sending XOVER data */
EXTERN long	OVERsize;	/* number of bytes of XOVER data sent	*/
EXTERN long	OVERdbz;	/* number of ms spent reading dbz data	*/
EXTERN long	OVERseek;	/* number of ms spent seeking history	*/
EXTERN long	OVERget;	/* number of ms spent reading history	*/
EXTERN long	OVERartcheck;	/* number of ms spent article check	*/
EXTERN double	IDLEtime;
EXTERN long	GRParticles;
EXTERN long	GRPcount;
EXTERN char	*GRPcur;
EXTERN long	POSTreceived;
EXTERN long	POSTrejected;

EXTERN bool     BACKOFFenabled;
EXTERN long     ClientIpAddr;
EXTERN char	*VirtualPath;
EXTERN int	VirtualPathlen;
EXTERN struct history *History;


#if	NNRP_LOADLIMIT > 0
extern int		GetLoadAverage(void);
#endif	/* NNRP_LOADLIMIT > 0 */
extern const char	*ARTpost(char *article, char *idbuff, bool ihave,
				 bool *permanent);
extern void		ARTclose(void);
extern bool		ARTreadschema(void);
extern char		*Glom(char **av);
extern int		Argify(char *line, char ***argvp);
extern void		InitBackoffConstants(void);
extern char		*PostRecFilename(char *ip, char *user);
extern int		LockPostRec(char *path);
extern int		LockPostRec(char *path);
extern void		UnlockPostRec(char *path);
extern int		RateLimit(long *sleeptime, char *path);
extern void		ExitWithStats(int x, bool readconf);
extern char		*GetHeader(const char *header, bool IsLines);
extern void		GRPreport(void);
extern bool		NGgetlist(char ***argvp, char *list);
extern bool		PERMartok(void);
extern void		PERMgetaccess(char *nnrpaccess);
extern void		PERMgetpermissions(void);
extern void		PERMlogin(char *uname, char *pass, char *errorstr);
extern bool		PERMmatch(char **Pats, char **list);
extern bool		ParseDistlist(char ***argvp, char *list);
extern READTYPE		READline(char *start, int size, int timeout);
extern char		*OVERGetHeader(char *p, int len, int field);
extern void 		SetDefaultAccess(ACCESSGROUP*);
extern void		Reply(const char *fmt, ...);

#ifdef HAVE_SSL
extern void             Printf(const char *fmt, ...);
#else
#define Printf printf
#endif

extern void		CMDauthinfo  (int ac, char** av);
extern void		CMDdate      (int ac, char** av);
extern void		CMDfetch     (int ac, char** av);
extern void		CMDgroup     (int ac, char** av);
extern void		CMDhelp      (int ac, char** av);
extern void		CMDlist      (int ac, char** av);
extern void		CMDmode      (int ac, char** av);
extern void		CMDnewgroups (int ac, char** av);
extern void		CMDnewnews   (int ac, char** av);
extern void		CMDnextlast  (int ac, char** av);
extern void		CMDpost      (int ac, char** av);
extern void		CMDxgtitle   (int ac, char** av);
extern void		CMDxover     (int ac, char** av);
extern void		CMDpat       (int ac, char** av);
extern void		CMDxpath     (int ac, char** av);
extern void		CMD_unimp    (int ac, char** av);
#ifdef HAVE_SSL
extern void		CMDstarttls  (int ac, char** av);
#endif



extern char *HandleHeaders(char *article);
extern bool ARTinstorebytoken(TOKEN token);

extern int TrackClient(char *client, char* user);

#ifdef  DO_PERL
extern void loadPerl(void);
extern void perlAccess(char *clientHost, char *clientIpString, char *serverHost, char *user, struct vector *access_vec);
extern int perlAuthenticate(char *clientHost, char *clientIpString, char *serverHost, char *user, char *passwd, char *errorstring);
extern void perlAuthInit(void);
#endif /* DO_PERL */

#ifdef	DO_PYTHON
int PY_authenticate(char *clientHost, char *clientIpString, char *serverHost, char *Username, char *Password, char *accesslist);
int PY_authorize(char *clientHost, char *clientIpString, char *ServerHost, char *Username, char *NewsGroup, int PostFlag, char **reply_message);
#endif	/* DO_PYTHON */
