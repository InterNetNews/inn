/*  $Id$
**
**  Net News Reading Protocol server.
*/

#include "config.h"
#include "portable/time.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "storage.h"


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
EXTERN char     ClientIp[20];
EXTERN char     ServerIp[20];
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
EXTERN double	IDLEtime;
EXTERN long	GRParticles;
EXTERN long	GRPcount;
EXTERN char	*GRPcur;
EXTERN long	POSTreceived;
EXTERN long	POSTrejected;

EXTERN bool     BACKOFFenabled;
EXTERN long     ClientIP;                                 
EXTERN char	*VirtualPath;
EXTERN int	VirtualPathlen;


#if	NNRP_LOADLIMIT > 0
extern int		GetLoadAverage();
#endif	/* NNRP_LOADLIMIT > 0 */
extern const char	*ARTpost();
extern void		ARTclose();
extern bool		ARTreadschema();
extern char		*Glom();
extern int		Argify();
extern void		ExitWithStats(int x, bool readconf);
extern bool		GetGroupList();
extern char		*GetHeader();
extern void		GRPreport();
extern void		HIScheck();
extern char		*HISgetent();
extern long		LOCALtoGMT();
extern bool		NGgetlist();
extern long		NNTPtoGMT();
extern bool		PERMartok();
extern bool		PERMmatch();
extern bool		ParseDistlist();
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
bool ARTinstorebytoken(TOKEN token);

#ifdef	DO_PYTHON
int PY_authenticate(char *ClientHost, char *ClientIP, char *ServerHost, char *Username, char *Password, char *accesslist);
int PY_authorize(char *ClientHost, char *ClientIP, char *ServerHost, char *Username, char *NewsGroup, int PostFlag, char **reply_message);
#endif	/* DO_PYTHON */
