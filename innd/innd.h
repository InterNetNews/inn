/*
**  Many of the data types used here have abbreviations, such as CT for a
**  channel type.  Here are a list of the conventions and meanings:
**
**    ART   A news article
**    CHAN  An I/O channel
**    CS    Channel state
**    CT    Channel type
**    FNL   Funnel, into which other feeds pour
**    FT    Feed type -- how a site gets told about new articles
**    ICD   In-core data (primarily the active and sys files)
**    LC    Local NNTP connection-receiving channel
**    CC    Control channel (used by ctlinnd)
**    NC    NNTP client channel
**    NG    Newsgroup
**    NGH   Newgroup hashtable
**    PROC  A process (used to feed a site)
**    PS    Process state
**    RC    Remote NNTP connection-receiving channel
**    RCHAN A channel in "read" state
**    SITE  Something that gets told when we get an article
**    WCHAN A channel in "write" state
**    WIP   Work-In-Progress, keeps track of articles before committed.
*/

#ifndef INND_H
#define INND_H 1

#include "config.h"
#include "portable/macros.h"
#include "portable/sd-daemon.h"
#include "portable/socket.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>

#ifdef HAVE_SYS_TIME_H
#    include <sys/time.h>
#endif
#include <time.h>

#include "inn/buffer.h"
#include "inn/history.h"
#include "inn/libinn.h"
#include "inn/messages.h"
#include "inn/nntp.h"
#include "inn/paths.h"
#include "inn/storage.h"
#include "inn/timer.h"
#include "inn/vector.h"

BEGIN_DECLS

typedef short SITEIDX;
#define NOSITE          ((SITEIDX) - 1)

/*
**  Various constants.
*/

/* Used for storing group subscriptions for feeds. */
#define SUB_DEFAULT     false
#define SUB_NEGATE      '!'
#define SUB_POISON      '@'

/* Special characters for newsfeeds entries. */
#define NF_FIELD_SEP    ':'
#define NF_SUBFIELD_SEP '/'


/*
**  Server's operating mode.  Note that OMshutdown is only used internally
**  while shutting down in CCshutdown or CCxexec and does not need to be
**  handled outside of the Perl and Python filtering code.
*/
typedef enum _OPERATINGMODE {
    OMrunning,
    OMpaused,
    OMthrottled,
    OMshutdown
} OPERATINGMODE;


typedef struct _LISTBUFFER {
    char *Data;
    int DataLength;
    char **List;
    int ListLength;
} LISTBUFFER;

/*
**  What program to handoff a connection to.
*/
typedef enum _HANDOFF {
    HOnnrpd,
    HOnntpd
} HANDOFF;


/*
**  Header field types.
*/
typedef enum _ARTHEADERTYPE {
    HTreq, /* Drop article if this is missing */
    HTobs, /* Obsolete header field but keep untouched */
    HTstd, /* Standard optional header field */
    HTsav  /* Save header field, but may be deleted from article */
} ARTHEADERTYPE;


/*
**  Entry in the header table.
*/
typedef struct _ARTHEADER {
    const char *Name;
    ARTHEADERTYPE Type;
    int Size; /* Length of Name. */
} ARTHEADER;


/*
**  Header body.
*/
typedef struct _HDRCONTENT {
    char *Value;   /* Don't copy, shows where it begins. */
    int Length;    /* Length of Value (tailing CRLF is not
                      included).  -1 if duplicated. */
    char LastChar; /* Saved char when the last one is cut during parsing.
                      Usually '\r' but it may be another char. */
} HDRCONTENT;


/*
**  A way to index into the header table.
*/
#define HDR_FOUND(_x)                     (hc[(_x)].Length > 0)
#define HDR_LASTCHAR_SAVE(_x)             hc[(_x)].LastChar = hc[(_x)].Value[hc[_x].Length]
#define HDR_PARSE_START(_x)               hc[(_x)].Value[hc[_x].Length] = '\0'
#define HDR(_x)                           (hc[(_x)].Value)
/* HDR_LEN does not includes trailing "\r\n" */
#define HDR_LEN(_x)                       (hc[(_x)].Length)
#define HDR_PARSE_END(_x)                 hc[(_x)].Value[hc[_x].Length] = hc[(_x)].LastChar


#define HDR__APPROVED                     0
#define HDR__CONTROL                      1
#define HDR__DATE                         2
#define HDR__DISTRIBUTION                 3
#define HDR__EXPIRES                      4
#define HDR__FROM                         5
#define HDR__LINES                        6
#define HDR__MESSAGE_ID                   7
#define HDR__NEWSGROUPS                   8
#define HDR__PATH                         9
#define HDR__REPLY_TO                     10
#define HDR__SENDER                       11
#define HDR__SUBJECT                      12
#define HDR__SUPERSEDES                   13
#define HDR__BYTES                        14
#define HDR__ALSOCONTROL                  15
#define HDR__REFERENCES                   16
#define HDR__XREF                         17
#define HDR__KEYWORDS                     18
#define HDR__XTRACE                       19
#define HDR__DATERECEIVED                 20
#define HDR__POSTED                       21
#define HDR__POSTINGVERSION               22
#define HDR__RECEIVED                     23
#define HDR__RELAYVERSION                 24
#define HDR__NNTPPOSTINGHOST              25
#define HDR__FOLLOWUPTO                   26
#define HDR__ORGANIZATION                 27
#define HDR__CONTENTTYPE                  28
#define HDR__CONTENTBASE                  29
#define HDR__CONTENTDISPOSITION           30
#define HDR__XNEWSREADER                  31
#define HDR__XMAILER                      32
#define HDR__XNEWSPOSTER                  33
#define HDR__XCANCELLEDBY                 34
#define HDR__XCANCELEDBY                  35
#define HDR__CANCEL_KEY                   36
#define HDR__USER_AGENT                   37
#define HDR__X_ORIGINAL_MESSAGE_ID        38
#define HDR__CANCEL_LOCK                  39
#define HDR__CONTENT_TRANSFER_ENCODING    40
#define HDR__FACE                         41
#define HDR__INJECTION_INFO               42
#define HDR__LIST_ID                      43
#define HDR__MIME_VERSION                 44
#define HDR__ORIGINATOR                   45
#define HDR__X_AUTH                       46
#define HDR__X_COMPLAINTS_TO              47
#define HDR__X_FACE                       48
#define HDR__X_HTTP_USERAGENT             49
#define HDR__X_HTTP_VIA                   50
#define HDR__X_MODBOT                     51
#define HDR__X_MODTRACE                   52
#define HDR__X_NO_ARCHIVE                 53
#define HDR__X_ORIGINAL_TRACE             54
#define HDR__X_ORIGINATING_IP             55
#define HDR__X_PGP_KEY                    56
#define HDR__X_PGP_SIG                    57
#define HDR__X_POSTER_TRACE               58
#define HDR__X_POSTFILTER                 59
#define HDR__X_PROXY_USER                 60
#define HDR__X_SUBMISSIONS_TO             61
#define HDR__X_USENET_PROVIDER            62
#define HDR__IN_REPLY_TO                  63
#define HDR__INJECTION_DATE               64
#define HDR__NNTP_POSTING_DATE            65
#define HDR__X_USER_ID                    66
#define HDR__X_AUTH_SENDER                67
#define HDR__X_ORIGINAL_NNTP_POSTING_HOST 68
#define HDR__ORIGINAL_SENDER              69
#define HDR__NNTP_POSTING_PATH            70
#define HDR__ARCHIVE                      71
#define HDR__ARCHIVED_AT                  72
#define HDR__SUMMARY                      73
#define HDR__COMMENTS                     74
#define HDR__JABBER_ID                    75
#define MAX_ARTHEADER                     76

/*
**  Miscellaneous data we want to keep on an article.  All the fields
**  are not always valid.
*/
typedef struct _ARTDATA {
    size_t Body;             /* where body begins in article
                                it indicates offset from bp->Data */
    time_t Posted;           /* when article posted */
    time_t Arrived;          /* when article arrived */
    time_t Expires;          /* when article should be expired */
    int Lines;               /* number of body lines */
    int HeaderLines;         /* number of header lines */
    char LinesBuffer[SMBUF]; /* Generated Lines header field body */
    long BytesValue;         /* size of stored article, "\r\n" is
                                counted as 2 bytes */
    char Bytes[SMBUF];       /* Generated Bytes header field body. */
    int BytesLength;         /* Generated Bytes header field
                                body length. */
    char *BytesHeader;       /* Where Bytes header field begins in
                                received article */
    char TokenText[(sizeof(TOKEN) * 2) + 3];
    /* token of stored article */
    LISTBUFFER Newsgroups;   /* newsgroup list */
    int Groupcount;          /* number of newsgroups */
    int Followcount;         /* number of folloup to newsgroups */
    char *Xref;              /* generated Xref header field body */
    int XrefLength;          /* generated Xref header field body
                                length */
    int XrefBufLength;       /* buffer length of generated Xref
                                header field body */
    LISTBUFFER Distribution; /* distribution list */
    const char *Feedsite;    /* who gives me this article */
    int FeedsiteLength;      /* length of Feedsite */
    LISTBUFFER Path;         /* path name list */
    int StoredGroupLength;   /* 1st newsgroup name in Xref */
    char *Replic;            /* replication data */
    int ReplicLength;        /* length of Replic */
    HASH *Hash;              /* Message-ID hash */
    struct buffer Headers;   /* buffer for headers which will be sent
                                to site */
    struct buffer Overview;  /* buffer for overview data */
    int CRwithoutLF;         /* counter for '\r' without '\n' */
    int LFwithoutCR;         /* counter for '\n' without '\r' */
    int DotStuffedLines;     /* counter for lines beginning with '.' */
    size_t CurHeader;        /* where current header field starts.
                                this is used for folded header fields
                                it indicates offset from bp->Data */
    HDRCONTENT HdrContent[MAX_ARTHEADER];
    /* includes system header fields info */
    bool AddAlias;       /* Whether Pathalias should be added
                            to this article */
    bool Hassamepath;    /* Whether this article matches Path */
    bool Hassamecluster; /* Whether this article matches
                            Pathcluster */
} ARTDATA;


/* Set of channel types. */
enum channel_type {
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
};

/* The state a channel is in.  Interpretation of this depends on the channel's
   type.  Used mostly by CTnntp channels. */
enum channel_state {
    CSerror,
    CSwaiting,
    CSgetcmd,
    CSgetauth,
    CSwritegoodbye,
    CSwriting,
    CSpaused,
    CSgetheader,
    CSgetbody,
    CSgotarticle,
    CSgotlargearticle,
    CSnoarticle,
    CSeatarticle,
    CSeatcommand,
    CSgetxbatch,
    CScancel
};


#define SAVE_AMT           10 /* used for eating article/command */
#define PRECOMMITCACHESIZE 128

/*
**  I/O channel, the heart of the program.  A channel has input and output
**  buffers, and functions to call when there is input to be read, or when
**  all the output was been written.  Many callback functions take a
**  pointer to a channel, so set up a typedef for that.
*/
struct _CHANNEL;
typedef void (*innd_callback_func)(struct _CHANNEL *);
typedef void (*innd_callback_nntp_func)(struct _CHANNEL *);

typedef struct _CHANNEL {
    enum channel_type Type;
    enum channel_state State;
    int fd;
    int ac;    /* Number of arguments in NNTP command. */
    char **av; /* List of arguments in NNTP command. */
    bool Skip;
    bool Ignore;
    bool Streaming;
    bool ResendId;
    bool privileged;
    bool List;
    bool Xbatch;
    bool CanAuthenticate; /* Can use AUTHINFO? */
    bool IsAuthenticated; /* No need to use AUTHINFO? */
    bool HasSentUsername; /* Has used AUTHINFO USER? */
    unsigned long Duplicate;
    unsigned long Duplicate_checkpoint;
    unsigned long Unwanted_s;
    unsigned long Unwanted_f;
    unsigned long Unwanted_d;
    unsigned long Unwanted_g;
    unsigned long Unwanted_u;
    unsigned long Unwanted_o;
    float Size;
    float Size_checkpoint;
    float DuplicateSize;
    float DuplicateSize_checkpoint;
    float RejectSize;
    float RejectSize_checkpoint;
    unsigned long Check;
    unsigned long Check_send;
    unsigned long Check_deferred;
    unsigned long Check_got;
    unsigned long Takethis;
    unsigned long Takethis_Ok;
    unsigned long Takethis_Err;
    unsigned long Ihave;
    unsigned long Ihave_Duplicate;
    unsigned long Ihave_Deferred;
    unsigned long Ihave_SendIt;
    unsigned long Reported;
    unsigned long Received;
    unsigned long Received_checkpoint;
    unsigned long Refused;
    unsigned long Refused_checkpoint;
    unsigned long Rejected;
    unsigned long Rejected_checkpoint;
    unsigned long BadWrites;
    unsigned long BadReads;
    unsigned long BlockedWrites;
    unsigned int BadCommands;
    time_t LastActive;
    time_t NextLog;
    struct sockaddr_storage Address;
    innd_callback_func Reader;
    innd_callback_func WriteDone;
    time_t Waketime;
    time_t Started;
    time_t Started_checkpoint;
    innd_callback_func Waker;
    void *Argument;
    void *Event;
    struct buffer In;
    struct buffer Out;
    bool Tracing;
    struct buffer Sendid;
    HASH CurrentMessageIDHash;
    struct _WIP *PrecommitWIP[PRECOMMITCACHESIZE];
    int PrecommitiCachenext;
    int XBatchSize;
    int LargeArtSize;
    int LargeCmdSize;
    int ActiveCnx;
    int MaxCnx;
    int HoldTime;
    time_t ArtBeg;
    int ArtMax;
    size_t Start;      /* where current cmd/article starts
                          it indicates offset from bp->Data */
    size_t Next;       /* next pointer to read
                          it indicates offset from bp->Data */
    char Error[SMBUF]; /* error buffer */
    ARTDATA Data;      /* used for processing article */
    char Name[SMBUF];  /* storage for CHANname */
} CHANNEL;

#define DEFAULTNGBOXSIZE 64

/*
**  Different types of rejected articles.
*/
typedef enum {
    REJECT_DUPLICATE,
    REJECT_SITE,
    REJECT_FILTER,
    REJECT_DISTRIB,
    REJECT_GROUP,
    REJECT_UNAPP,
    REJECT_OTHER
} Reject_type;

/*
**  A newsgroup has a name in different formats, and a high water count,
**  also kept in different formats.  It also has a list of sites that
**  get this group.
*/
typedef struct _NEWSGROUP {
    long Start; /* Offset into the active file */
    char *Name;
    int NameLength;
    ARTNUM Last;
    ARTNUM Filenum; /* File name to use */
    int Lastwidth;
    int PostCount; /* Have we already put it here? */
    char *LastString;
    char *Rest; /* Flags, NOT NULL TERMINATED */
    SITEIDX nSites;
    int *Sites;
    SITEIDX nPoison;
    int *Poison;
    struct _NEWSGROUP *Alias;
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
**  Diablo-style hashed feeds or hashfeeds.
*/
#define HASHFEED_QH  1
#define HASHFEED_MD5 2

typedef struct _HASHFEEDLIST {
    int type;
    unsigned int begin;
    unsigned int end;
    unsigned int mod;
    unsigned int offset;
    struct _HASHFEEDLIST *next;
} HASHFEEDLIST;

/*
**  A site may reject something in its subscription list if it has
**  too many hops, or a bad distribution.
*/
typedef struct _SITE {
    const char *Name;
    char *Entry;
    int NameLength;
    char **Exclusions;
    char **Distributions;
    char **Patterns;
    bool Poison;
    bool PoisonEntry;
    bool Sendit;
    bool Seenit;
    bool IgnoreControl;
    bool DistRequired;
    bool IgnorePath;
    bool ControlOnly;
    bool DontWantNonExist;
    bool NeedOverviewCreation;
    bool FeedwithoutOriginator;
    bool DropFiltered;
    bool FeedTrash;
    int Hops;
    int Groupcount;
    int Followcount;
    int Crosscount;
    FEEDTYPE Type;
    NEWSGROUP *ng;
    bool Spooling;
    char *SpoolName;
    bool Working;
    long StartWriting;
    long StopWriting;
    long StartSpooling;
    char *Param;
    char FileFlags[FEED_MAXFLAGS + 1];
    long MaxSize;
    long MinSize;
    int Nice;
    CHANNEL *Channel;
    bool IsMaster;
    int Master;
    int Funnel;
    bool FNLwantsnames;
    struct buffer FNLnames;
    int Process;
    pid_t pid;
    size_t Flushpoint;
    struct buffer Buffer;
    bool Buffered;
    char **Originator;
    HASHFEEDLIST *HashFeedList;
    int Next;
    int Prev;
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
    PROCSTATE State;
    pid_t Pid;
    int Status;
    time_t Started;
    time_t Collected;
    int Site;
} PROCESS;

/*
**  A work in progress entry, an article that we've been offered but haven't
**  received yet.
*/
typedef struct _WIP {
    HASH MessageID;    /* Hash of the messageid.  Doing it like
                          this saves us from having to allocate
                          and deallocate memory a lot, and also
                          means lookups are faster. */
    time_t Timestamp;  /* Time we last looked at this MessageID */
    CHANNEL *Chan;     /* Channel that this message is associated
                          with */
    struct _WIP *Next; /* Next item in this bucket */
} WIP;

/*
**  Supported timers.  If you add new timers to this list, also add them to
**  the list of tags in chan.c.
*/
enum timer {
    TMR_IDLE = TMR_APPLICATION, /* Server is completely idle. */
    TMR_ARTCLEAN,               /* Analyzing an incoming article. */
    TMR_ARTWRITE,               /* Writing an article. */
    TMR_ARTCNCL,                /* Processing a cancel message. */
    TMR_SITESEND,               /* Sending an article to feeds. */
    TMR_OVERV,                  /* Generating overview information. */
    TMR_PERL,                   /* Perl filter. */
    TMR_PYTHON,                 /* Python filter. */
    TMR_NNTPREAD,               /* Reading NNTP data from the network. */
    TMR_ARTPARSE,               /* Parsing an article. */
    TMR_ARTLOG,                 /* Logging article disposition. */
    TMR_DATAMOVE,               /* Moving data. */
    TMR_MAX
};


/*
**  In-line macros for efficiency.
**
**  Set or append data to a channel's output buffer.
*/
#define WCHANset(cp, p, l)    buffer_set(&(cp)->Out, (p), (l))
#define WCHANappend(cp, p, l) buffer_append(&(cp)->Out, (p), (l))

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
#    define EXTERN /* NULL */
#else
#    define EXTERN extern
#endif
extern const ARTHEADER ARTheaders[MAX_ARTHEADER];
extern bool BufferedLogs;
EXTERN bool AnyIncoming;
extern bool Debug;
extern bool laxmid;
EXTERN bool ICDneedsetup;
EXTERN bool NeedHeaders;
EXTERN bool NeedOverview;
EXTERN bool NeedPath;
EXTERN bool NeedStoredGroup;
EXTERN bool NeedReplicdata;
extern bool NNRPTracing;
extern bool StreamingOff;
extern bool Tracing;
EXTERN struct buffer Path;
EXTERN struct buffer Pathalias;
EXTERN struct buffer Pathcluster;
EXTERN char *ModeReason;   /* NNTP reject message */
EXTERN char *NNRPReason;   /* NNRP reject message */
EXTERN char *Reservation;  /* Reserved lock message */
EXTERN char *RejectReason; /* NNTP reject message */
EXTERN FILE *Errlog;
EXTERN FILE *Log;
extern char LogName[];
extern int ErrorCount;
EXTERN unsigned long ICDactivedirty;
EXTERN int MaxOutgoing;
EXTERN int nGroups;
EXTERN SITEIDX nSites;
EXTERN int PROCneedscan;
EXTERN NEWSGROUP **GroupPointers;
EXTERN NEWSGROUP *Groups;
extern OPERATINGMODE Mode;
EXTERN sig_atomic_t GotTerminate;
EXTERN SITE *Sites;
EXTERN SITE ME;
EXTERN struct timeval TimeOut;
EXTERN struct timeval Now; /* Reasonably accurate time */
EXTERN bool ThrottledbyIOError;
EXTERN char *NCgreeting;
EXTERN struct history *History;

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
extern int RemoteLimit;    /* Per host limit. */
extern time_t RemoteTimer; /* How long to remember connects. */
extern int RemoteTotal;    /* Total limit. */


/*
**  Function declarations.
*/
extern void InndHisOpen(void);
extern void InndHisClose(void);
extern bool InndHisWrite(const char *key, time_t arrived, time_t posted,
                         time_t expires, TOKEN *token);
extern bool InndHisRemember(const char *key, time_t posted);
extern void InndHisLogStats(void);
extern bool FormatLong(char *p, unsigned long value, int width);
extern bool NeedShell(char *p, const char **av, const char **end);
extern char **CommaSplit(char *text);
extern void SetupListBuffer(int size, LISTBUFFER *list);
extern char *MaxLength(const char *p, const char *q);
extern pid_t Spawn(int niceval, int fd0, int fd1, int fd2, char *const av[]);
extern void CleanupAndExit(int status, const char *why)
    __attribute__((__noreturn__));
extern void JustCleanup(void);
extern void ThrottleIOError(const char *when);
extern void ThrottleNoMatchError(void);
extern void ReopenLog(FILE *F);
extern void xchown(char *p);

extern bool ARTreadschema(void);
extern const char *ARTreadarticle(char *files);
extern char *ARTreadheader(char *files);
extern bool ARTpost(CHANNEL *cp);
extern void ARTcancel(const ARTDATA *data, const char *MessageID,
                      bool Trusted);
extern void ARTclose(void);
extern void ARTsetup(void);
extern void ARTprepare(CHANNEL *cp);
extern void ARTparse(CHANNEL *cp);
extern void ARTlogreject(CHANNEL *cp, const char *text);
extern void ARTreject(Reject_type, CHANNEL *);

extern bool CHANsleeping(CHANNEL *cp);
extern bool CHANsystemdsa(CHANNEL *cp);
extern CHANNEL *CHANcreate(int fd, enum channel_type type,
                           enum channel_state state, innd_callback_func reader,
                           innd_callback_func write_done);
extern CHANNEL *CHANiter(int *ip, enum channel_type type);
extern CHANNEL *CHANfromdescriptor(int fd);
extern char *CHANname(CHANNEL *cp);
extern int CHANreadtext(CHANNEL *cp);
extern void CHANclose(CHANNEL *cp, const char *name);
extern void CHANreadloop(void) __attribute__((__noreturn__));
extern void CHANsetup(int count);
extern void CHANshutdown(void);
extern void CHANtracing(CHANNEL *cp, bool flag);
extern void CHANcount_active(CHANNEL *cp);

extern void RCHANadd(CHANNEL *cp);
extern void RCHANremove(CHANNEL *cp);

extern void SCHANadd(CHANNEL *cp, time_t wake, void *event,
                     innd_callback_func waker, void *arg);
extern void SCHANremove(CHANNEL *cp);
extern void SCHANwakeup(void *event);

extern bool WCHANflush(CHANNEL *cp);
extern void WCHANadd(CHANNEL *cp);
extern void WCHANremove(CHANNEL *cp);
extern void WCHANsetfrombuffer(CHANNEL *cp, struct buffer *bp);

extern void CCcopyargv(char *av[]);
extern const char *CCaddhist(char *av[]);
extern const char *CCblock(OPERATINGMODE NewMode, char *reason);
extern const char *CCcancel(char *av[]);
extern const char *CCcheckfile(char *av[]);

extern bool ICDnewgroup(char *Name, char *Rest);
extern char *ICDreadactive(char **endp);
extern bool ICDchangegroup(NEWSGROUP *ngp, char *Rest);
extern void ICDclose(void);
extern bool ICDrenumberactive(void);
extern bool ICDrmgroup(NEWSGROUP *ngp);
extern void ICDsetup(bool StartSites);
extern void ICDwrite(void);
extern void ICDwriteactive(void);

extern void CCclose(void);
extern void CCsetup(void);

extern void KEYgenerate(HDRCONTENT *, const char *, size_t);

extern void LCclose(void);
extern void LCsetup(void);

extern int NGsplit(char *p, int size, LISTBUFFER *list);
extern NEWSGROUP *NGfind(const char *Name);
extern void NGclose(void);
extern CHANNEL *NCcreate(int fd, bool MustAuthorize, bool IsLocal);
extern void NGparsefile(void);
extern bool NGrenumber(NEWSGROUP *ngp);
extern bool NGlowmark(NEWSGROUP *ngp, long lomark);

extern void NCclearwip(CHANNEL *cp);
extern void NCclose(void);
extern void NCsetup(void);
extern void NCwritereply(CHANNEL *cp, const char *text);
extern void NCwriteshutdown(CHANNEL *cp, const char *text);

/* perl.c */
extern char *PLartfilter(const ARTDATA *Data, char *artBody, long artLen,
                         int lines);
extern char *PLmidfilter(char *messageID);
extern void PLmode(OPERATINGMODE mode, OPERATINGMODE NewMode, char *reason);
extern char *PLstats(void);
extern void PLxsinit(void);

extern int PROCwatch(pid_t pid, int site);
extern void PROCunwatch(int process);
/* extern void PROCclose(bool Quickly); */
extern void PROCscan(void);
extern void PROCsetup(int i);

extern int RClimit(CHANNEL *cp);
extern bool RCnolimit(CHANNEL *cp);
extern bool RCauthorized(CHANNEL *cp, char *pass);
extern int RCcanpost(CHANNEL *cp, char *group);
extern char *RChostname(const CHANNEL *cp);
extern char *RClabelname(CHANNEL *cp);
extern void RCclose(void);
extern void RChandoff(int fd, HANDOFF h);
extern void RCreadlist(void);
extern void RCsetup(void);

extern bool SITEfunnelpatch(void);
extern bool SITEsetup(SITE *sp);
extern bool SITEwantsgroup(SITE *sp, char *name);
extern bool SITEpoisongroup(SITE *sp, char *name);
extern char **SITEreadfile(const bool ReadOnly);
extern SITE *SITEfind(const char *p);
extern SITE *SITEfindnext(const char *p, SITE *sp);
extern const char *SITEparseone(char *Entry, SITE *sp, char *subbed,
                                char *poison);
extern void SITEchanclose(CHANNEL *cp);
extern void SITEdrop(SITE *sp);
extern void SITEflush(SITE *sp, const bool Restart);
extern void SITEflushall(const bool Restart);
extern void SITEforward(SITE *sp, const char *text);
extern void SITEfree(SITE *sp);
extern void SITEinfo(struct buffer *bp, SITE *sp, bool Verbose);
extern void SITEparsefile(bool StartSite);
extern void SITEprocdied(SITE *sp, int process, PROCESS *pp);
extern void SITEsend(SITE *sp, ARTDATA *Data);
extern void SITEwrite(SITE *sp, const char *text);

extern void STATUSinit(void);
extern void STATUSmainloophook(void);

extern void WIPsetup(void);
extern WIP *WIPnew(const char *messageid, CHANNEL *cp);
extern void WIPprecomfree(CHANNEL *cp);
extern void WIPfree(WIP *wp);
extern bool WIPinprogress(const char *msgid, CHANNEL *cp, bool Precommit);
extern WIP *WIPbyid(const char *messageid);
extern WIP *WIPbyhash(const HASH hash);

/*
**  Python globals and functions
*/
#if DO_PYTHON
extern bool PythonFilterActive;

void PYfilter(bool value);
extern const char *PYcontrol(char **av);
extern int PYreadfilter(void);
extern char *PYartfilter(const ARTDATA *Data, char *artBody, long artLen,
                         int lines);
extern char *PYmidfilter(char *messageID, int msglen);
extern void PYmode(OPERATINGMODE mode, OPERATINGMODE newmode, char *reason);
extern void PYsetup(void);
extern void PYclose(void);
#    if PY_MAJOR_VERSION >= 3
extern PyMODINIT_FUNC PyInit_INN(void);
#    else
extern void PyInit_INN(void);
#    endif
#endif /* DO_PYTHON */

END_DECLS

#endif /* INND_H */
