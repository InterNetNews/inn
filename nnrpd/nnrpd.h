/*  $Revision$
**
**  Net News Reading Protocol server.
*/
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "qio.h"


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
    int allownewnews;
    int locpost;
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
    BOOL	NeedsHeader;
} ARTOVERFIELD;

#if	defined(MAINLINE)
#define EXTERN	/* NULL */
#else
#define EXTERN	extern
#endif	/* defined(MAINLINE) */

EXTERN BOOL	PERMauthorized;
EXTERN BOOL	PERMcanpost;
EXTERN BOOL	PERMcanread;
EXTERN BOOL	PERMneedauth;
EXTERN BOOL	PERMspecified;
EXTERN ACCESSGROUP	*PERMaccessconf;
EXTERN BOOL	Tracing;
EXTERN BOOL 	Offlinepost;
EXTERN BOOL 	initialSSL;
EXTERN char	**PERMreadlist;
EXTERN char	**PERMpostlist;
EXTERN char	ClientHost[SMBUF];
EXTERN char     ServerHost[SMBUF];
EXTERN char	Username[SMBUF];
EXTERN char     ClientIp[20];
EXTERN char	LogName[256] ;
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
EXTERN long	GRParticles;
EXTERN long	GRPcount;
EXTERN char	*GRPcur;
EXTERN long	POSTreceived;
EXTERN long	POSTrejected;

EXTERN BOOL     BACKOFFenabled;
EXTERN long     ClientIP;                                 
EXTERN char	*VirtualPath;
EXTERN int	VirtualPathlen;


#if	NNRP_LOADLIMIT > 0
extern int		GetLoadAverage();
#endif	/* NNRP_LOADLIMIT > 0 */
extern STRING		ARTpost();
extern void		ARTclose();
extern BOOL		ARTreadschema();
extern char		*Glom();
extern int		Argify();
extern NORETURN		ExitWithStats(int x, BOOL readconf);
extern BOOL		GetGroupList();
extern char		*GetHeader(char *header, bool IsLineS);
extern void		GRPreport();
extern void		HIScheck();
extern char		*HISgetent();
extern long		LOCALtoGMT();
extern BOOL		NGgetlist();
extern long		NNTPtoGMT();
extern BOOL		PERMartok();
extern BOOL		PERMmatch();
extern BOOL		ParseDistlist();
extern READTYPE		READline();
extern char		*OVERGetHeader(char *p, int field);
extern void SetDefaultAccess(ACCESSGROUP*);

#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
extern void             Reply(const char *fmt, ...);
#else
# ifdef HAVE_VARARGS_H
extern void             Reply();
# else
#  define Reply printf
# endif
#endif

#ifdef HAVE_SSL
extern void             Printf(const char *fmt, ...);
#else
#define Printf printf
#endif

char *HandleHeaders(char *article);
int perlConnect(char *ClientHost, char *ClientIP, char *ServerHost, char *accesslist);
int perlAuthenticate(char *ClientHost, char *ClientIP, char *ServerHost, char *user, char *passwd, char *accesslist);
BOOL ARTinstorebytoken(TOKEN token);

#ifdef	DO_PYTHON
int PY_authenticate(char *ClientHost, char *ClientIP, char *ServerHost, char *Username, char *Password, char *accesslist);
int PY_authorize(char *ClientHost, char *ClientIP, char *ServerHost, char *Username, char *NewsGroup, int PostFlag, char **reply_message);
#endif	/* DO_PYTHON */
