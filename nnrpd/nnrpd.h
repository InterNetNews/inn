/*  $Revision$
**
**  Net News Reading Protocol server.
*/
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <sys/file.h>
#if	defined(VAR_VARARGS)
#include <varargs.h>
#endif	/* defined(VAR_VARARGS) */
#if	defined(VAR_STDARGS)
#include <stdarg.h>
#endif	/* defined(VAR_STDARGS) */
#include <syslog.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#if defined(OVER_MMAP) || defined(ART_MMAP)
#include <sys/mman.h>
#endif
#include "paths.h"
#include "nntp.h"
#include "logging.h"
#include "libinn.h"
#include "qio.h"
#include "macros.h"


/*
**  Maximum input line length, sigh.
*/
#define ART_LINE_LENGTH		1000
#define ART_LINE_MALLOC		1024


/*
**  Some convenient shorthands.
*/
typedef struct in_addr	INADDR;
#define Printf		(void)printf
#if	defined(VAR_NONE)
#define Reply		(void)printf
#endif	/* defined(VAR_NONE) */


/*
**  A group entry.
*/
typedef struct _GROUPENTRY {
    char	*Name;
    ARTNUM	High;
    ARTNUM	Low;
    char	Flag;
    char	*Alias;
} GROUPENTRY;


/*
**  A range of article numbers.
*/
typedef struct _ARTRANGE {
    int		Low;
    int		High;
} ARTRANGE;


/*
**  What READline returns.
*/
typedef enum _READTYPE {
    RTeof,
    RTok,
    RTlong,
    RTtimeout
} READTYPE;


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
EXTERN BOOL	Tracing;
EXTERN char	**PERMlist;
EXTERN STRING	MyHostName;
extern char	ACTIVE[];
EXTERN char	ClientHost[SMBUF];
EXTERN char	LogName[256] ;
extern char	ACTIVETIMES[];
extern char	HISTORY[];
extern char	NEWSGROUPS[];
extern char	NOACCESS[];
EXTERN char	PERMpass[20];
EXTERN char	PERMuser[20];
EXTERN char	*RemoteMaster;
EXTERN ARTNUM	*ARTcache;
EXTERN ARTNUM	*ARTnumbers;
EXTERN int	ARTindex;
EXTERN int	ARTsize;
extern int	PERMdefault;
EXTERN long	ARTcount;
EXTERN long	ARTget;
EXTERN long	ARTgettime;
EXTERN long	ARTgetsize;
EXTERN long	OVERcount;	/* number of XOVER commands			*/
EXTERN long	OVERhit;	/* number of XOVER records found in .overview	*/
EXTERN long	OVERmiss;	/* number of XOVER records found in articles	*/
EXTERN long	OVERtime;	/* number of ms spent sending XOVER data	*/
EXTERN long	OVERread;	/* number of bytes of XOVER data read		*/
EXTERN long	OVERsize;	/* number of bytes of XOVER data sent		*/
EXTERN long	GRParticles;
EXTERN long	GRPcount;
EXTERN char	GRPlast[SPOOLNAMEBUFF];
EXTERN long	POSTreceived;
EXTERN long	POSTrejected;


#if	NNRP_LOADLIMIT > 0
extern int		GetLoadAverage();
#endif	/* NNRP_LOADLIMIT > 0 */
extern STRING		ARTpost();
extern void		ARTclose();
extern void		ARTreadschema();
extern char		*Glom();
extern int		Argify();
extern NORETURN		ExitWithStats();
extern BOOL		GetGroupList();
extern char		*GetHeader();
extern void		GRPreport();
extern GROUPENTRY	*GRPfind();
extern char		*HISgetent();
extern long		LOCALtoGMT();
extern BOOL		NGgetlist();
extern long		NNTPtoGMT();
extern BOOL		PERMartok();
extern BOOL		PERMinfile();
extern BOOL		PERMmatch();
extern BOOL		ParseDistlist();
extern READTYPE		READline();
extern void		OVERclose();
#if	defined(VAR_STDARGS)
extern void		Reply(char *, ...);
#endif	/* defined(VAR_STDARGS) */
#if	defined(VAR_VARARGS)
extern void		Reply();
#endif	/* defined(VAR_VARARGS) */
