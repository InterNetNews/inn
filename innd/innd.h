/*  $Revision$
**
**  Many of the data types used here have abbreviations, such as CT
**  for CHANNELTYPE.  Here are a list of the conventions and meanings:
**	ART	A news article
**	CHAN	An I/O channel
**	CS	Channel state
**	CT	Channel type
**	FNL	Funnel, into which other feeds pour
**	FT	Feed type -- how a site gets told about new articles
**	ICD	In-core data (primarily the active and sys files)
**	LC	Local NNTP connection-receiving channel
**	CC	Control channel (used by ctlinnd)
**	NC	NNTP client channel
**	NG	Newsgroup
**	NGH	Newgroup hashtable
**	PROC	A process (used to feed a site)
**	PS	Process state
**	RC	Remote NNTP connection-receiving channel
**	RCHAN	A channel in "read" state
**	SITE	Something that gets told when we get an article
**	WCHAN	A channel in "write" state
**      WIP     Work-In-Progress, keeps track of articles before committed.
*/
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif	/* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "nntp.h"
#include "paths.h"
#include "logging.h"
#include "libinn.h"
#include "macros.h"
#include "dbz.h"

#if     defined(DO_TCL)
#include <tcl.h>
#undef EXTERN /* TCL defines EXTERN; this undef prevents error messages 
	         when we define it later */
#endif  /* defined(DO_TCL) */


typedef short  SITEIDX;

#define NOSITE		((SITEIDX) -1)


/*
**  Some convenient shorthands.
*/
typedef struct in_addr	INADDR;

#ifndef u_long
#define u_long		unsigned long
#endif

/*
**  Server's operating mode.
*/
typedef enum _OPERATINGMODE {
    OMrunning,
    OMpaused,
    OMthrottled
} OPERATINGMODE;


/*
**  An I/O buffer, including a size, an amount used, and a count of how
**  much space is left if reading or how much still needs to be written.
*/
typedef struct _BUFFER {
    long	Size;
    long	Used;
    long	Left;
    char	*Data;
} BUFFER;


/*
**  What program to handoff a connection to.
*/
typedef enum _HANDOFF {
    HOnnrpd,
    HOnntpd
} HANDOFF;


/*
**  Set of channel types.
*/
typedef enum _CHANNELTYPE {
    CTany,
    CTfree,
    CTremconn,
    CTreject,
    CTnntp,
    CTlocalconn,
    CTcontrol,
    CTfile,
    CTexploder,
    CTprocess
} CHANNELTYPE;


/*
**  The state a channel is in.  Interpretation of this depends on the
**  channel's type.  Used mostly by CTnntp channels.
*/
typedef enum _CHANNELSTATE {
    CSerror,
    CSwaiting,
    CSgetcmd,
    CSgetauth,
    CSwritegoodbye,
    CSwriting,
    CSpaused,
    CSgetarticle,
    CSeatarticle,
    CSgetrep,
    CSgetxbatch
} CHANNELSTATE;


/*
**  I/O channel, the heart of the program.  A channel has input and output
**  buffers, and functions to call when there is input to be read, or when
**  all the output was been written.
*/
typedef struct _CHANNEL {
    CHANNELTYPE		Type;
    CHANNELSTATE	State;
    BOOL		Skip;
    BOOL		Streaming;
    int			fd;
    u_long		Duplicate;
    u_long		Unwanted_s;
    u_long		Unwanted_f;
    u_long		Unwanted_d;
    u_long		Unwanted_g;
    u_long		Unwanted_u;
    u_long		Unwanted_o;
    u_long		Size;
    u_long		Check;
    u_long		Check_send;
    u_long		Check_deferred;
    u_long		Check_got;
    u_long		Takethis;
    u_long		Takethis_Ok;
    u_long		Takethis_Err;
    u_long		Ihave;
    u_long		Ihave_Duplicate;
    u_long		Ihave_Deferred;
    u_long		Ihave_SendIt;
    int			Reported;
    long		Received;
    long		Refused;
    long		Rejected;
    int			BadWrites;
    int			BadReads;
    int			BlockedWrites;
    int			BadCommands;
    time_t		LastActive;
    time_t		NextLog;
    INADDR		Address;
    FUNCPTR		Reader;
    FUNCPTR		WriteDone;
    time_t		Waketime;
    time_t		Started;
    FUNCPTR		Waker;
    POINTER		Argument;
    POINTER		Event;
    BUFFER		In;
    BUFFER		Out;
    BOOL		Tracing;
    BUFFER		Sendid;
    HASH                CurrentMessageIDHash;
#define PRECOMMITCACHESIZE 128
    struct _WIP         *PrecommitWIP[PRECOMMITCACHESIZE];
    int                 PrecommitiCachenext;
    int                 XBatchSize;
    int                 LargeArtSize;
    int			Lastch;
    int			Rest;
    int			SaveUsed;
    int			ActiveCnx;
    int			MaxCnx;
} CHANNEL;


/*
**  A newsgroup has a name in different formats, and a high-water count,
**  also kept in different formats.  It also has a list of sites that
**  get this group.
*/
typedef struct _NEWSGROUP {
    long		Start;		/* Offset into the active file	*/
    char		*Name;
    char		*Dir;		/* The name, as a directory	*/
    int			NameLength;
    ARTNUM		Last;
    ARTNUM		Filenum;	/* File name to use		*/
    int			Lastwidth;
    int			PostCount;	/* Have we already put it here?	*/
    char		*LastString;
    char		*Rest;
    SITEIDX		nSites;
    int			*Sites;
    SITEIDX		nPoison;
    int			*Poison;
    struct _NEWSGROUP	*Alias;
} NEWSGROUP;


/*
**  How a site is fed.
*/
typedef enum _FEEDTYPE {
    FTerror,
    FTfile,
    FTchannel,
    FTexploder,
    FTfunnel,
    FTlogonly,
    FTprogram
} FEEDTYPE;


/*
**  A site may reject something in its subscription list if it has
**  too many hops, or a bad distribution.
*/
typedef struct _SITE {
    char		*Name;
    char		*Entry;
    int			NameLength;
    char		**Exclusions;
    char		**Distributions;
    char		**Patterns;
    BOOL		Poison;
    BOOL		PoisonEntry;
    BOOL		Sendit;
    BOOL		Seenit;
    BOOL		IgnoreControl;
    BOOL		DistRequired;
    BOOL		IgnorePath;
    int			Hops;
    int			Groupcount;
    FEEDTYPE		Type;
    NEWSGROUP		*ng;
    BOOL		Spooling;
    char		*SpoolName;
    BOOL		Working;
    long		StartWriting;
    long		StopWriting;
    long		StartSpooling;
    char		*Param;
    char		FileFlags[FEED_MAXFLAGS + 1];
    long		MaxSize;
    long		MinSize;
    int			Nice;
    CHANNEL		*Channel;
    BOOL		IsMaster;
    int			Master;
    int			Funnel;
    BOOL		FNLwantsnames;
    BUFFER		FNLnames;
    int			Process;
    PID_T		pid;
    long		Flushpoint;
    BUFFER		Buffer;
    BOOL		Buffered;
    char                *Originator;
    int			Next;
    int			Prev;
} SITE;


/*
**  A process is something we start up to send articles.
*/
typedef enum _PROCSTATE {
    PSfree,
    PSrunning,
    PSdead
} PROCSTATE;


/*
**  We track our children and collect them synchronously.
*/
typedef struct _PROCESS {
    PROCSTATE	State;
    PID_T	Pid;
    int		Status;
    time_t	Started;
    time_t	Collected;
    int		Site;
} PROCESS;


/*
**  Miscellaneous data we want to keep on an article.  All the fields
**  are not always valid.
*/
typedef struct _ARTDATA {
    STRING	Poster;
    STRING	Replyto;
    char	*Body;
    time_t	Posted;
    time_t	Arrived;
    time_t	Expires;
    int		Groupcount;
    int		LinesValue;
    char	Lines[SMBUF];
    long	SizeValue;
    char	Size[SMBUF];
    int		SizeLength;
    char	Name[SPOOLNAMEBUFF];
    int		NameLength;
    char	TimeReceived[33];
    int		TimeReceivedLength;
    STRING	MessageID;
    int		MessageIDLength;
    STRING	Newsgroups;
    int		NewsgroupsLength;
    STRING	Distribution;
    int		DistributionLength;
    STRING	Feedsite;
    int		FeedsiteLength;
    STRING      Path;
    int		PathLength;
    STRING	Replic;
    int		ReplicLength;
    HASH	*Hash;
    BUFFER	*Headers;
    BUFFER	*Overview;
} ARTDATA;

typedef struct _WIP {
    HASH        MessageID;       /* Hash of the messageid.  Doing it like this saves
			            us from haveing to allocate and deallocate memory
			            a lot, and also means lookups are faster. */
    time_t      Timestamp;       /* Time we last looked at this MessageID */
    CHANNEL     *Chan;           /* Channel that this message is associated with */
    struct _WIP *Next;           /* Next item in this bucket */
} WIP;

typedef enum {TMR_IDLE, TMR_ARTWRITE, TMR_ARTLINK, TMR_HISWRITE,
	      TMR_HISSYNC, TMR_SITESEND, TMR_ARTCTRL, TMR_ARTCNCL,
	      TMR_HISHAVE, TMR_HISGREP, TMR_MAX} TMRTYPE;



/*
**  In-line macros for efficiency.
**
**  Set or append data to a channel's output buffer.
*/
#define WCHANset(cp, p, l)	BUFFset(&(cp)->Out, (p), (l))
#define WCHANappend(cp, p, l)	BUFFappend(&(cp)->Out, (p), (l))

/*
**  Mark that an I/O error occurred, and block if we got too many.
*/
#define IOError(WHEN, e)	\
	if (--ErrorCount <= 0 || (e) == ENOSPC) ThrottleIOError(WHEN); else


/*
**  Global data.
**
** Do not change "extern" to "EXTERN" in the Global data.  The ones
** marked with "extern" are initialized in innd.c.  The ones marked
** with "EXTERN" are not explicitly initialized in innd.c.
*/
#if	defined(DEFINE_DATA)
#define EXTERN		/* NULL */
#else
#define EXTERN		extern
#endif	/* defined(DEFINE_DATA) */
extern BOOL		AmRoot;
extern BOOL		BufferedLogs;
EXTERN BOOL		AmSlave;
EXTERN BOOL		AnyIncoming;
extern BOOL		Debug;
EXTERN BOOL		ICDneedsetup;
EXTERN BOOL		NeedHeaders;
EXTERN BOOL		NeedOverview;
EXTERN BOOL		NeedPath;
EXTERN BOOL		NNRPFollows;
extern BOOL		NNRPTracing;
extern BOOL		StreamingOff;
extern BOOL		Tracing;
EXTERN int              Overfdcount;
EXTERN int		SeqNum;
EXTERN STRING		path;
EXTERN BUFFER		Path;
EXTERN BUFFER		Pathalias;
EXTERN char		*ModeReason;	/* NNTP reject message		*/
EXTERN char		*NNRPReason;	/* NNRP reject message		*/
EXTERN char		*Reservation;	/* Reserved lock message	*/
EXTERN char		*RejectReason;	/* NNTP reject message		*/
extern char		*SPOOL;
EXTERN char		*Version;
EXTERN FILE		*Errlog;
EXTERN FILE		*Log;
extern char		LogName[];
EXTERN INADDR		MyAddress;
extern int		ErrorCount;
EXTERN int		ICDactivedirty;
EXTERN int		KillerSignal;
EXTERN int		MaxOutgoing;
EXTERN int		nGroups;
EXTERN SITEIDX		nSites;
EXTERN int		PROCneedscan;
extern int		SPOOLlen;
EXTERN int		Xrefbase;
extern long		LargestArticle;
EXTERN NEWSGROUP	**GroupPointers;
EXTERN NEWSGROUP	*Groups;
extern OPERATINGMODE	Mode;
EXTERN SIGVAR		GotTerminate;
EXTERN SITE		*Sites;
EXTERN SITE		ME;
EXTERN struct timeval	TimeOut;
EXTERN TIMEINFO		Now;		/* Reasonably accurate time	*/

/*
** Table size for limiting incoming connects.  Do not change the table
** size unless you look at the code manipulating it in rc.c.
*/
#define REMOTETABLESIZE 128

/*
** Setup the default values.  The REMOTETIMER being zero turns off the
** code to limit incoming connects.
*/
#define REMOTELIMIT	2
#define REMOTETIMER	0
#define REMOTETOTAL	60
#define REJECT_TIMEOUT	10
extern int		RemoteLimit; /* Per host limit. */
extern time_t		RemoteTimer; /* How long to remember connects. */
extern int		RemoteTotal; /* Total limit. */


/*
**  Function declarations.
*/
extern BOOL		FormatLong();
extern BOOL		MakeSpoolDirectory();
extern BOOL		NeedShell();
extern char		**CommaSplit();
extern char		*MaxLength();
extern PID_T		Spawn();
extern NORETURN		CleanupAndExit();
extern void		FileGlue();
extern void		JustCleanup();
extern void		ThrottleIOError();
extern void		ReopenLog();
extern void		xchown();

extern BOOL		ARTidok(const char *MessageID);
extern BOOL		ARTreadschema(void);
extern const char       *ARTreadarticle(char *files);
extern char             *ARTreadheader(char *files);
extern STRING		ARTpost(CHANNEL *cp);
extern void		ARTcancel(const ARTDATA *Data, const char *MessageID, const BOOL Trusted);
extern void		ARTclose(void);
extern void		ARTsetup(void);

extern void		BUFFset(BUFFER *bp, const char *p, const int length);
extern void		BUFFswap(BUFFER *b1, BUFFER *b2);
extern void             BUFFappend(BUFFER *bp, const char *p, const int len);

extern BOOL		CHANsleeping(CHANNEL *cp);
extern CHANNEL		*CHANcreate(int fd, CHANNELTYPE Type, CHANNELSTATE STate, FUNCPTR Reader, FUNCPTR WriteDone);
extern CHANNEL		*CHANiter(int *cp, CHANNELTYPE Type);
extern CHANNEL		*CHANfromdescriptor(int fd);
extern char		*CHANname(const CHANNEL *cp);
extern int		CHANreadtext(CHANNEL *cp);
extern void		CHANclose(CHANNEL *cp, char *name);
extern void		CHANreadloop(void);
extern void		CHANsetup(int i);
extern void		CHANtracing(CHANNEL *cp, BOOL Flag);

extern void		RCHANadd(CHANNEL *cp);
extern void		RCHANremove(CHANNEL *cp);

extern void		SCHANadd(CHANNEL *cp, time_t Waketime, POINTER Event, POINTER Waker, POINTER Argument);
extern void		SCHANremove(CHANNEL *cp);
extern void		SCHANwakeup(POINTER *Event);

extern BOOL		WCHANflush(CHANNEL *cp);
extern void		WCHANadd(CHANNEL *cp);
extern void		WCHANremove(CHANNEL *cp);
extern void		WCHANsetfrombuffer(CHANNEL *cp, BUFFER *bp);

extern void		CCcopyargv();
extern STRING		CCblock();
extern STRING		CCcancel();
extern STRING		CCcheckfile();

extern BOOL		HIShavearticle(const HASH MessageID);
extern BOOL		HISwrite(const ARTDATA *Data, HASH hash, char *paths);
extern BOOL             HISremember(const HASH MessageID);
extern char		*HISfilesfor(const HASH MessageID);
extern void		HISclose(void);
extern void		HISsetup(void);
extern void		HISsync(void);

extern BOOL		ICDnewgroup();
extern char		*ICDreadactive();
extern BOOL		ICDchangegroup();
extern void		ICDclose();
extern BOOL		ICDrenumberactive();
extern BOOL		ICDrmgroup();
extern void		ICDsetup();
extern void		ICDwrite();
extern void		ICDwriteactive();

extern void		CCclose();
extern void		CCsetup();

extern void		LCclose();
extern void		LCsetup();

extern char		**NGsplit();
extern NEWSGROUP	*NGfind();
extern CHANNEL		*NCcreate();
extern void		NGparsefile();
extern BOOL		NGrenumber();
extern BOOL		NGlowmark(NEWSGROUP *ngp, long lomark);

extern void		NCclearwip(CHANNEL *cp);
extern void		NCclose();
extern void		NCsetup();

extern int		PROCwatch();
extern void		PROCunwatch();
/* extern void		PROCclose(); */
extern void		PROCscan();
extern void		PROCsetup();

extern int		RClimit();
extern BOOL		RCnolimit();
extern BOOL		RCauthorized();
extern BOOL		RCcanpost();
extern char		*RChostname();
extern int		RCismaster();
extern void		RCclose();
extern void		RChandoff();
extern void		RCreadlist();
extern void		RCsetup();

extern BOOL		SITEfunnelpatch();
extern BOOL		SITEsetup();
extern BOOL		SITEwantsgroup();
extern BOOL		SITEpoisongroup();
extern char		**SITEreadfile();
extern SITE		*SITEfind();
extern SITE		*SITEfindnext();
extern STRING		SITEparseone();
extern void		SITEchanclose();
extern void		SITEdrop();
extern void		SITEflush();
extern void		SITEflushall();
extern void		SITEforward();
extern void		SITEfree();
extern void		SITEinfo();
extern void		SITElinkall();
extern void		SITEparsefile();
extern void		SITEprocdied();
extern void		SITEsend();
extern void		SITEwrite();

extern void		STATUSinit(void);
extern void             STATUSmainloophook(void);

extern void             TMRinit(void);
extern void             TMRmainloophook(void);
extern void             TMRstart(TMRTYPE t);
extern void             TMRstop(TMRTYPE t);

extern void             WIPsetup(void);
extern WIP              *WIPnew(const char *messageid, CHANNEL *cp);
extern void             WIPprecomfree(CHANNEL *cp);
extern void             WIPfree(WIP *wp);
extern BOOL             WIPinprogress(const char *msgid, CHANNEL *cp, const BOOL Add);
extern WIP              *WIPbyid(const char *mesageid);
extern WIP              *WIPbyhash(const HASH hash);

/*
**  TCL Globals
*/

#if defined(DO_TCL)
extern Tcl_Interp      *TCLInterpreter;
extern BOOL            TCLFilterActive;
extern BUFFER          *TCLCurrArticle;
extern ARTDATA         *TCLCurrData;
#endif /* defined(DO_TCL) */


/*
**  TCL Functions
*/

#if defined(DO_TCL)
extern void            TCLfilter();
extern void            TCLreadfilter();
extern void            TCLsetup();
extern void            TCLclose();
#endif /* defined(DO_TCL) */
