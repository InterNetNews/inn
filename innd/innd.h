/*  $Id$
**
**  Many of the data types used here have abbreviations, such as CT
**  for CHANNELTYPE.  Here are a list of the conventions and meanings:
**
**      ART     A news article
**      CHAN    An I/O channel
**      CS      Channel state
**      CT      Channel type
**      FNL     Funnel, into which other feeds pour
**      FT      Feed type -- how a site gets told about new articles
**      ICD     In-core data (primarily the active and sys files)
**      LC      Local NNTP connection-receiving channel
**      CC      Control channel (used by ctlinnd)
**      NC      NNTP client channel
**      NG      Newsgroup
**      NGH     Newgroup hashtable
**      PROC    A process (used to feed a site)
**      PS      Process state
**      RC      Remote NNTP connection-receiving channel
**      RCHAN   A channel in "read" state
**      SITE    Something that gets told when we get an article
**      WCHAN   A channel in "write" state
**      WIP     Work-In-Progress, keeps track of articles before committed.
*/

#ifndef INND_H
#define INND_H 1

#include "config.h"
#include "portable/time.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <syslog.h> 
#include <sys/socket.h>
#include <sys/stat.h>

#include "dbz.h"
#include "libinn.h"
#include "macros.h"
#include "nntp.h"
#include "paths.h"
#include "storage.h"

/* TCL defines EXTERN, so undef it after inclusion since we use it. */
#if DO_TCL
# include <tcl.h>
# undef EXTERN
#endif

BEGIN_DECLS

typedef short  SITEIDX;
#define NOSITE ((SITEIDX) -1)

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
    long        Size;
    long        Used;
    long        Left;
    char *      Data;
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
    CSeatcommand,
    CSgetxbatch,
    CScancel
} CHANNELSTATE;


/*
**  I/O channel, the heart of the program.  A channel has input and output
**  buffers, and functions to call when there is input to be read, or when
**  all the output was been written.
*/
typedef struct _CHANNEL {
    CHANNELTYPE         Type;
    CHANNELSTATE        State;
    int                 fd;
    bool                Skip;
    bool                Streaming;
    bool                NoResendId;
    bool                privileged;
    unsigned long       Duplicate;
    unsigned long       Unwanted_s;
    unsigned long       Unwanted_f;
    unsigned long       Unwanted_d;
    unsigned long       Unwanted_g;
    unsigned long       Unwanted_u;
    unsigned long       Unwanted_o;
    float               Size;
    float               DuplicateSize;
    unsigned long       Check;
    unsigned long       Check_send;
    unsigned long       Check_deferred;
    unsigned long       Check_got;
    unsigned long       Check_cybercan;
    unsigned long       Takethis;
    unsigned long       Takethis_Ok;
    unsigned long       Takethis_Err;
    unsigned long       Ihave;
    unsigned long       Ihave_Duplicate;
    unsigned long       Ihave_Deferred;
    unsigned long       Ihave_SendIt;
    unsigned long       Ihave_Cybercan;
    int                 Reported;
    long                Received;
    long                Refused;
    long                Rejected;
    int                 BadWrites;
    int                 BadReads;
    int                 BlockedWrites;
    int                 BadCommands;
    time_t              LastActive;
    time_t              NextLog;
    struct in_addr      Address;
    FUNCPTR             Reader;
    FUNCPTR             WriteDone;
    time_t              Waketime;
    time_t              Started;
    FUNCPTR             Waker;
    POINTER             Argument;
    POINTER             Event;
    BUFFER              In;
    BUFFER              Out;
    bool                Tracing;
    BUFFER              Sendid;
    HASH                CurrentMessageIDHash;
#define PRECOMMITCACHESIZE 128
    struct _WIP *       PrecommitWIP[PRECOMMITCACHESIZE];
    int                 PrecommitiCachenext;
    int                 XBatchSize;
    int                 LargeArtSize;
    int                 LargeCmdSize;
    int                 Lastch;
    int                 Rest;
    int                 SaveUsed;
    int                 ActiveCnx;
    int                 MaxCnx;
    int                 HoldTime;
    time_t              ArtBeg;
    int                 ArtMax;
} CHANNEL;


/*
**  A newsgroup has a name in different formats, and a high-water count,
**  also kept in different formats.  It also has a list of sites that
**  get this group.
*/
typedef struct _NEWSGROUP {
    long                Start;          /* Offset into the active file  */
    char *              Name;
    char *              Dir;            /* The name, as a directory     */
    int                 NameLength;
    ARTNUM              Last;
    ARTNUM              Filenum;        /* File name to use             */
    int                 Lastwidth;
    int                 PostCount;      /* Have we already put it here? */
    char *              LastString;
    char *              Rest;           /* Flags, NOT NULL TERMINATED */
    SITEIDX             nSites;
    int *               Sites;
    SITEIDX             nPoison;
    int *               Poison;
    struct _NEWSGROUP * Alias;
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
    char *              Name;
    char *              Entry;
    int                 NameLength;
    char **             Exclusions;
    char **             Distributions;
    char **             Patterns;
    bool                Poison;
    bool                PoisonEntry;
    bool                Sendit;
    bool                Seenit;
    bool                IgnoreControl;
    bool                DistRequired;
    bool                IgnorePath;
    bool                ControlOnly;
    bool                DontWantNonExist;
    bool                NeedOverviewCreation;
    bool                FeedwithoutOriginator;
    bool                DropFiltered;
    int                 Hops;
    int                 Groupcount;
    int                 Followcount;
    int                 Crosscount;
    FEEDTYPE            Type;
    NEWSGROUP *         ng;
    bool                Spooling;
    char *              SpoolName;
    bool                Working;
    long                StartWriting;
    long                StopWriting;
    long                StartSpooling;
    char *              Param;
    char                FileFlags[FEED_MAXFLAGS + 1];
    long                MaxSize;
    long                MinSize;
    int                 Nice;
    CHANNEL *           Channel;
    bool                IsMaster;
    int                 Master;
    int                 Funnel;
    bool                FNLwantsnames;
    BUFFER              FNLnames;
    int                 Process;
    pid_t               pid;
    long                Flushpoint;
    BUFFER              Buffer;
    bool                Buffered;
    char **             Originator;
    int                 Next;
    int                 Prev;
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
    PROCSTATE   State;
    pid_t       Pid;
    int         Status;
    time_t      Started;
    time_t      Collected;
    int         Site;
} PROCESS;


/*
**  Miscellaneous data we want to keep on an article.  All the fields
**  are not always valid.
*/
typedef struct _ARTDATA {
    char *      Poster;
    char *      Replyto;
    char *      Body;
    time_t      Posted;
    time_t      Arrived;
    time_t      Expires;
    int         Groupcount;
    int         Followcount;
    int         LinesValue;
    char        Lines[SMBUF];
    long        SizeValue;
    char        Size[SMBUF];
    int         SizeLength;
    char        Name[SPOOLNAMEBUFF];
    int         NameLength;
    char        TimeReceived[33];
    int         TimeReceivedLength;
    char *      MessageID;
    int         MessageIDLength;
    char *      Newsgroups;
    int         NewsgroupsLength;
    char *      Distribution;
    int         DistributionLength;
    char *      Feedsite;
    int         FeedsiteLength;
    char *      Path;
    int         PathLength;
    char *      Replic;
    int         ReplicLength;
    char *      StoredGroup;
    int         StoredGroupLength;
    HASH *      Hash;
    BUFFER *    Headers;
    BUFFER *    Overview;
} ARTDATA;

/*
**  A work in progress entry, an article that we've been offered but haven't
**  received yet.
*/
typedef struct _WIP {
    HASH        MessageID;      /* Hash of the messageid.  Doing it like
                                   this saves us from haveing to allocate
                                   and deallocate memory a lot, and also
                                   means lookups are faster. */ 
    time_t      Timestamp;      /* Time we last looked at this MessageID */
    CHANNEL *   Chan;           /* Channel that this message is associated
                                   with */
    struct _WIP * Next;         /* Next item in this bucket */
} WIP;

/*
**  Supported timers.  If you add new timers to this list, also add them to
**  the list of tags at the top of timer.c.
*/
enum timer {
    TMR_IDLE,                   /* Server is completely idle. */
    TMR_HISHAVE,                /* Looking up ID in history (yes/no). */
    TMR_HISGREP,                /* Looking up ID in history (data). */
    TMR_HISWRITE,               /* Writing to history. */
    TMR_HISSYNC,                /* Syncing history to disk. */
    TMR_ARTCLEAN,               /* Analyzing an incoming article. */
    TMR_ARTWRITE,               /* Writing an article. */
    TMR_ARTLINK,                /* Linking an article (tradspool). */
    TMR_ARTCTRL,                /* Processing a control message. */
    TMR_ARTCNCL,                /* Processing a cancel message. */
    TMR_SITESEND,               /* Sending an article to feeds. */
    TMR_OVERV,                  /* Generating overview information. */
    TMR_PERL,                   /* Perl filter. */
    TMR_PYTHON,                 /* Python filter. */
    TMR_MAX
};



/*
**  In-line macros for efficiency.
**
**  Set or append data to a channel's output buffer.
*/
#define WCHANset(cp, p, l)      BUFFset(&(cp)->Out, (p), (l))
#define WCHANappend(cp, p, l)   BUFFappend(&(cp)->Out, (p), (l))

/*
**  Mark that an I/O error occurred, and block if we got too many.
*/
#define IOError(WHEN, e)                        \
    do {                                        \
        if (--ErrorCount <= 0 || (e) == ENOSPC) \
            ThrottleIOError(WHEN);              \
    } while (0)



/*
**  Global data.
**
** Do not change "extern" to "EXTERN" in the Global data.  The ones
** marked with "extern" are initialized in innd.c.  The ones marked
** with "EXTERN" are not explicitly initialized in innd.c.
*/
#if defined(DEFINE_DATA)
# define EXTERN         /* NULL */
#else
# define EXTERN         extern
#endif
extern bool             AmRoot;
extern bool             BufferedLogs;
EXTERN bool             AnyIncoming;
extern bool             Debug;
EXTERN bool             ICDneedsetup;
EXTERN bool             NeedHeaders;
EXTERN bool             NeedOverview;
EXTERN bool             NeedPath;
extern bool             NNRPTracing;
extern bool             StreamingOff;
extern bool             Tracing;
EXTERN int              Overfdcount;
EXTERN int              SeqNum;
EXTERN char *           path;
EXTERN BUFFER           Path;
EXTERN BUFFER           Pathalias;
EXTERN char *           ModeReason;     /* NNTP reject message          */
EXTERN char *           NNRPReason;     /* NNRP reject message          */
EXTERN char *           Reservation;    /* Reserved lock message        */
EXTERN char *           RejectReason;   /* NNTP reject message          */
EXTERN FILE *           Errlog;
EXTERN FILE *           Log;
extern char             LogName[];
EXTERN struct in_addr   MyAddress;
extern int              ErrorCount;
EXTERN int              ICDactivedirty;
EXTERN int              KillerSignal;
EXTERN int              MaxOutgoing;
EXTERN int              nGroups;
EXTERN SITEIDX          nSites;
EXTERN int              PROCneedscan;
extern int              SPOOLlen;
EXTERN int              Xrefbase;
extern long             LargestArticle;
EXTERN NEWSGROUP **     GroupPointers;
EXTERN NEWSGROUP *      Groups;
extern OPERATINGMODE    Mode;
EXTERN sig_atomic_t     GotTerminate;
EXTERN SITE *           Sites;
EXTERN SITE             ME;
EXTERN struct timeval   TimeOut;
EXTERN TIMEINFO         Now;            /* Reasonably accurate time     */
EXTERN bool             ThrottledbyIOError;
EXTERN bool             AddAlias;
EXTERN bool             Hassamepath;
EXTERN char *           NCgreeting;

/*
** Table size for limiting incoming connects.  Do not change the table
** size unless you look at the code manipulating it in rc.c.
*/
#define REMOTETABLESIZE 128

/*
** Setup the default values.  The REMOTETIMER being zero turns off the
** code to limit incoming connects.
*/
#define REMOTELIMIT     2
#define REMOTETIMER     0
#define REMOTETOTAL     60
#define REJECT_TIMEOUT  10
extern int              RemoteLimit; /* Per host limit. */
extern time_t           RemoteTimer; /* How long to remember connects. */
extern int              RemoteTotal; /* Total limit. */


/*
**  Function declarations.
*/
extern bool             FormatLong();
extern bool             MakeSpoolDirectory();
extern bool             NeedShell();
extern char **          CommaSplit();
extern char *           MaxLength();
extern pid_t            Spawn();
extern void             CleanupAndExit();
extern void             FileGlue();
extern void             JustCleanup();
extern void             ThrottleIOError();
extern void             ThrottleNoMatchError();
extern void             ReopenLog();
extern void             xchown();

extern bool             ARTidok(const char *MessageID);
extern bool             ARTreadschema(void);
extern const char *     ARTreadarticle(char *files);
extern char *           ARTreadheader(char *files);
extern const char *     ARTpost(CHANNEL *cp);
extern void             ARTcancel(const ARTDATA *Data,
                                  const char *MessageID, bool Trusted);
extern void             ARTclose(void);
extern void             ARTsetup(void);

extern void             BUFFset(BUFFER *bp, const char *p, const int length);
extern void             BUFFswap(BUFFER *b1, BUFFER *b2);
extern void             BUFFappend(BUFFER *bp, const char *p, const int len);
extern void             BUFFtrimcr(BUFFER *bp);

extern bool             CHANsleeping(CHANNEL *cp);
extern CHANNEL *        CHANcreate(int fd, CHANNELTYPE Type,
                                   CHANNELSTATE State, FUNCPTR Reader,
                                   FUNCPTR WriteDone);
extern CHANNEL *        CHANiter(int *cp, CHANNELTYPE Type);
extern CHANNEL *        CHANfromdescriptor(int fd);
extern char *           CHANname(const CHANNEL *cp);
extern int              CHANreadtext(CHANNEL *cp);
extern void             CHANclose(CHANNEL *cp, char *name);
extern void             CHANreadloop(void);
extern void             CHANsetup(int i);
extern void             CHANshutdown();
extern void             CHANtracing(CHANNEL *cp, bool Flag);
extern void             CHANsetActiveCnx(CHANNEL *cp);

extern void             RCHANadd(CHANNEL *cp);
extern void             RCHANremove(CHANNEL *cp);

extern void             SCHANadd(CHANNEL *cp, time_t Waketime,
                                 POINTER Event, FUNCPTR Waker,
                                 POINTER Argument);
extern void             SCHANremove(CHANNEL *cp);
extern void             SCHANwakeup(POINTER *Event);

extern bool             WCHANflush(CHANNEL *cp);
extern void             WCHANadd(CHANNEL *cp);
extern void             WCHANremove(CHANNEL *cp);
extern void             WCHANsetfrombuffer(CHANNEL *cp, BUFFER *bp);

extern void             CCcopyargv();
extern const char *     CCaddhist(char *av[]);
extern const char *     CCblock();
extern const char *     CCcancel();
extern const char *     CCcheckfile();

extern bool             HIShavearticle(const HASH MessageID);
extern bool             HISwrite(const ARTDATA *Data, const HASH hash,
                                 char *paths, TOKEN *token);
extern bool             HISremember(const HASH MessageID);
extern TOKEN *          HISfilesfor(const HASH MessageID);
extern void             HISclose(void);
extern void             HISsetup(void);
extern void             HISsync(void);

extern bool             ICDnewgroup();
extern char *           ICDreadactive();
extern bool             ICDchangegroup();
extern void             ICDclose();
extern bool             ICDrenumberactive();
extern bool             ICDrmgroup();
extern void             ICDsetup();
extern void             ICDwrite();
extern void             ICDwriteactive();

extern void             CCclose();
extern void             CCsetup();

extern void             LCclose();
extern void             LCsetup();

extern char **          NGsplit();
extern NEWSGROUP *      NGfind();
extern void             NGclose();
extern CHANNEL *        NCcreate(int fd, bool MustAuthorize, bool IsLocal);
extern void             NGparsefile();
extern bool             NGrenumber();
extern bool             NGlowmark(NEWSGROUP *ngp, long lomark);

extern void             NCclearwip(CHANNEL *cp);
extern void             NCclose();
extern void             NCsetup();
extern void             NCwritereply(CHANNEL *cp, char *text);
extern void             NCwriteshutdown(CHANNEL *cp, char *text);

/* perl.c */
extern char *           PLartfilter(char *artBody, int lines);
extern char *           PLmidfilter(char *messageID);
extern void             PLmode(OPERATINGMODE Mode, OPERATINGMODE NewMode,
                               char *reason);
extern void             PLxsinit();

extern int              PROCwatch();
extern void             PROCunwatch();
/* extern void          PROCclose(); */
extern void             PROCscan();
extern void             PROCsetup();

extern int              RClimit();
extern bool             RCnolimit();
extern bool             RCauthorized();
extern int              RCcanpost();
extern char *           RChostname();
extern char *           RClabelname(CHANNEL *cp);
extern void             RCclose();
extern void             RChandoff();
extern void             RCreadlist();
extern void             RCsetup();

extern bool             SITEfunnelpatch();
extern bool             SITEsetup();
extern bool             SITEwantsgroup();
extern bool             SITEpoisongroup();
extern char **          SITEreadfile();
extern SITE *           SITEfind();
extern SITE *           SITEfindnext();
extern const char *     SITEparseone();
extern void             SITEchanclose();
extern void             SITEdrop();
extern void             SITEflush();
extern void             SITEflushall();
extern void             SITEforward();
extern void             SITEfree();
extern void             SITEinfo();
extern void             SITElinkall();
extern void             SITEparsefile();
extern void             SITEprocdied();
extern void             SITEsend();
extern void             SITEwrite();

extern void             STATUSinit(void);
extern void             STATUSmainloophook(void);

/* timer.c */
extern void             TMRinit(void);
extern int              TMRmainloophook(void);
extern void             TMRstart(enum timer timer);
extern void             TMRstop(enum timer timer);

extern void             WIPsetup(void);
extern WIP *            WIPnew(const char *messageid, CHANNEL *cp);
extern void             WIPprecomfree(CHANNEL *cp);
extern void             WIPfree(WIP *wp);
extern bool             WIPinprogress(const char *msgid, CHANNEL *cp,
                                      bool Add);
extern WIP *            WIPbyid(const char *mesageid);
extern WIP *            WIPbyhash(const HASH hash);

/*
**  TCL globals and functions
*/
#if DO_TCL
extern Tcl_Interp *     TCLInterpreter;
extern bool             TCLFilterActive;
extern BUFFER *         TCLCurrArticle;
extern ARTDATA *        TCLCurrData;

extern void             TCLfilter();
extern void             TCLreadfilter();
extern void             TCLsetup();
extern void             TCLclose();
#endif /* DO_TCL */

/*
**  Python globals and functions
*/
#if DO_PYTHON
extern bool             PythonFilterActive;

extern char *           PYcontrol(char **av);
extern int              PYreadfilter(void);
extern char *           PYartfilter(char *artBody, long artLen, int lines);
extern char *           PYmidfilter(char *messageID, int msglen);
extern void             PYmode(OPERATINGMODE Mode, OPERATINGMODE newmode,
                               char *reason);
extern void             PYsetup(void);
extern void             PYclose(void);
#endif /* DO_PYTHON */

END_DECLS

#endif /* INND_H */
