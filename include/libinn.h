/*  $Id$
**
**  Here be declarations of functions in the InterNetNews library.
*/
#ifndef __LIBINN_H__
#define __LIBINN_H__

/* We've probably already included this; only include it if we need it. */
#ifndef __CONFIG_H__
# include "config.h"
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

/* Linux defines some of these.  Later versions of INN will use different
   symbols, but this is the simple fix. */
#undef LOCK_READ
#undef LOCK_WRITE
#undef LOCK_UNLOCK

/* Tell C++ not to mangle prototypes. */
#ifdef __cplusplus
extern "C" {
#endif

/*
**  VERSION INFORMATION
*/
extern const int        inn_version[3];
extern const char       inn_version_extra[];
extern const char       inn_version_string[];

/* This function is deprecated.  Nothing in INN should use it, and it may
   eventually go away entirely. */
extern const char *     INNVersion(void);

#define HAVE_INN_VERSION_H

/*
**  MEMORY MANAGEMENT
*/
extern void *   xmalloc(size_t size, const char *file, int line);
extern void *   xrealloc(void *p, size_t size, const char *file, int line);
extern char *   xstrdup(const char *s, const char *file, int line);

/* This function is called whenever a memory allocation fails. */
extern int (*xmemfailure)(const char *, size_t, const char *, int);


/* String handling. */
#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
extern void *   concat(const char *first, ...);
#else
extern void *   concat();
#endif    

/* Headers. */
extern char	        *GenerateMessageID(char *domain);
extern const char	*HeaderFindMem(const char *Article, const int ArtLen, const char *Header, const int HeaderLen);
extern const char       *HeaderFindDisk(const char *file, const char *Header, const int HeaderLen);
extern void	        HeaderCleanFrom(char *from);
extern struct _DDHANDLE	*DDstart(FILE *FromServer, FILE *ToServer);
extern void		DDcheck(struct _DDHANDLE *h, char *group);
extern char		*DDend(struct _DDHANDLE *h);

/* NNTP functions. */
extern int	NNTPlocalopen(FILE **FromServerp, FILE **ToServerp, char *errbuff);
extern int	NNTPremoteopen(int port, FILE **FromServerp, FILE **ToServerp, char *errbuff);
extern int      NNTPconnect(char *host, int port, FILE **FromServerp, FILE **ToServerp, char *errbuff);
extern int	NNTPsendarticle(char *, FILE *F, BOOL Terminate);
extern int	NNTPsendpassword(char *server, FILE *FromServer, FILE *ToServer);
extern int      server_init(char *host, int port);

/* Opening the active file on a client. */
extern FILE	*CAopen(FILE *FromServer, FILE *ToServer);
extern FILE	*CAlistopen(FILE *FromServer, FILE *ToServer, char *request);
extern FILE     *CA_listopen(char *pathname, FILE *FromServer, FILE *ToServer, char *request);
extern void	CAclose(void);

/* File locking. */
typedef enum { LOCK_READ, LOCK_WRITE, LOCK_UNLOCK } LOCKTYPE;
extern BOOL      lock_file(int fd, LOCKTYPE type, BOOL block);

#ifdef HAVE_FCNTL
extern BOOL     LockRange(int fd, LOCKTYPE type, BOOL block,
                          OFFSET_T offset, OFFSET_T size);
#endif
    
/*
**  INNCONF SETTINGS
**
**  This structure is organized in the same order as the variables contained
**  in it are mentioned in the inn.conf documentation, and broken down into
**  the same sections.
*/
struct conf_vars {
    /* General Settings */
    char *domain;               /* Default domain of local host */
    char *mailcmd;              /* Command to send report/control type mail */
    char *mta;                  /* MTA for mailing to moderators, innmail */
    char *pathhost;             /* Entry for the Path line */
    char *server;               /* Default server to connect to */

    /* Feed Configuration */
    int artcutoff;              /* Max accepted article age */
    char *bindaddress;          /* Which interface IP to bind to */
    int hiscachesize;           /* Size of the history cache in kB */
    int ignorenewsgroups;       /* Propagate cmsgs by affected group? */
    int immediatecancel;        /* Immediately cancel timecaf messages? */
    int linecountfuzz;          /* Check linecount and reject if off by more */
    long maxartsize;            /* Reject articles bigger than this */
    int maxconnections;         /* Max number of incoming NNTP connections */
    char *pathalias;            /* Prepended Host for the Path line */
    int port;                   /* Which port innd should listen on */
    int refusecybercancels;     /* Reject message IDs with "<cancel."? */
    int remembertrash;          /* Put unwanted article IDs into history */
    char *sourceaddress;        /* Source IP for outgoing NNTP connections */
    int usecontrolchan;         /* Use a channel feed for control messages? */
    int verifycancels;          /* Verify cancels against article author */
    int wanttrash;              /* Put unwanted articles in junk */
    int wipcheck;               /* How long to defer other copies of article */
    int wipexpire;              /* How long to keep pending article record */

    /* Article Storage */
    long cnfscheckfudgesize;    /* Additional CNFS integrity checking */
    int enableoverview;         /* Store overview info for articles? */
    int groupbaseexpiry;        /* Do expiry by newsgroup? */
    int mergetogroups;          /* Refile articles from to.* into to */
    int overcachesize;          /* fd size cache for tradindexed */
    char *ovgrouppat;           /* Newsgroups to store overview for */
    char *ovmethod;             /* Which overview method to use */
    int storeonxref;            /* SMstore use Xref to detemine class? */
    int useoverchan;            /* overchan write the overview, not innd? */
    int wireformat;             /* Store tradspool artilces in wire format? */
    int xrefslave;              /* Act as a slave of another server? */

    /* Reading */
    int allownewnews;           /* Allow use of the NEWNEWS command */
    int articlemmap;            /* Use mmap to read articles? */
    int clienttimeout;          /* How long nnrpd can be inactive */
    int nnrpdcheckart;          /* Check article existence before returning? */
    int nnrpperlauth;           /* Use Perl for nnrpd authentication */
    int nnrppythonauth;         /* Use Python for nnrpd authentication */
    int noreader;               /* Refuse to fork nnrpd for readers? */
    int readerswhenstopped;     /* Allow nnrpd when server is paused */
    int readertrack;            /* Use the reader tracking system? */

    /* Reading -- Keyword Support */
    char keywords;              /* Generate keywords in overview? */
    int keyartlimit;            /* Max article size for keyword generation */
    int keylimit;               /* Max allocated space for keywords */
    int keymaxwords;            /* Max count of interesting works */

    /* Posting */
    int addnntppostingdate;     /* Add NNTP-Posting-Date: to posts */
    int addnntppostinghost;     /* Add NNTP-Posting-Host: to posts */
    int checkincludedtext;      /* Reject if too much included text */
    char *complaints;           /* Address for X-Complaints-To: */
    char *fromhost;             /* Host for the From: line */
    long localmaxartsize;       /* Max article size of local postings */
    char *moderatormailer;      /* Default host to mail moderated articles */
    int nnrpdauthsender;        /* Add authenticated Sender: header? */
    char *nnrpdposthost;        /* Host postings should be forwarded to */
    int nnrpdpostport;          /* Port postings should be forwarded to */
    char *organization;         /* Data for the Organization: header */
    int spoolfirst;             /* Spool all posted articles? */
    int strippostcc;            /* Strip To:, Cc: and Bcc: from posts */

    /* Posting -- Exponential Backoff */
    int backoff_auth;           /* Backoff by user, not IP address */
    char *backoff_db;           /* Directory for backoff databases */
    long backoff_k;             /* Multiple for the sleep time */
    long backoff_postfast;      /* Upper time limit for fast posting */
    long backoff_postslow;      /* Lower time limit for slow posting */
    long backoff_trigger;       /* Number of postings before triggered */

    /* Logging */
    int logartsize;             /* Log article sizes? */
    int logcancelcomm;          /* Log ctlinnd cancel commands to syslog? */
    int logipaddr;              /* Log by host IP address? */
    int logsitename;            /* Log outgoing site names? */
    int nnrpdoverstats;         /* Log overview statistics? */
    int nntpactsync;            /* Checkpoint log after this many articles */
    int nntplinklog;            /* Put storage token into the log? */
    int status;                 /* Status file update interval */
    int timer;                  /* Performance monitoring interval */

    /* System Tuning */
    int badiocount;             /* Failure count before dropping channel */
    int blockbackoff;           /* Multiplier for sleep in EAGAIN writes */
    int chaninacttime;          /* Wait before noticing inactive channels */
    int chanretrytime;          /* How long before channel restarts */
    int icdsynccount;           /* Articles between active & history updates */
    int maxforks;               /* Give up after this many fork failure */
    int nicekids;               /* Child processes get niced to this */
    int nicenewnews;            /* If NEWNEWS command is used, nice to this */
    int nicennrpd;              /* nnrpd is niced to this */
    int pauseretrytime;         /* Seconds before seeing if pause is ended */
    int peertimeout;            /* How long peers can be inactive */
    int rlimitnofile;           /* File descriptor limit to set */

    /* Paths */
    char *patharchive;          /* Archived news. */
    char *patharticles;         /* Articles. */
    char *pathbin;              /* News binaries. */
    char *pathcontrol;          /* Control message processing routines */
    char *pathdb;               /* News database files */
    char *pathetc;              /* News configuration files */
    char *pathfilter;           /* Filtering code */
    char *pathhttp;             /* HTML files */
    char *pathincoming;         /* Incoming spooled news */
    char *pathlog;              /* Log files */
    char *pathnews;             /* Home directory for news user */
    char *pathoutgoing;         /* Outgoing news batch files */
    char *pathoverview;         /* Overview infomation */
    char *pathrun;              /* Runtime state and sockets */
    char *pathspool;            /* Root of news spool hierarchy */
    char *pathtmp;              /* Temporary files for the news system */
};

extern struct	conf_vars *innconf;
extern char	*innconffile;
extern char	*GetFQDN(char *domain);
extern char	*GetConfigValue(char *value);
extern char	*GetFileConfigValue(char *value);
extern BOOL      GetBooleanConfigValue(char *value, BOOL DefaultValue);
extern char	*GetModeratorAddress(FILE *FromServer, FILE *ToServer, char *group, char *moderatormailer);
extern int ReadInnConf();
extern char *cpcatpath(char *p, char *f);

#define	TEMPORARYOPEN	0
#define	INND_HISTORY	1
#define	DBZ_DIR		2
#define	DBZ_BASE	3

/* Time functions. */
typedef struct _TIMEINFO {
    time_t	time;
    long	usec;
    long	tzone;
} TIMEINFO;
extern time_t	parsedate(char *p, TIMEINFO *now);
extern BOOL     makedate(time_t, BOOL local, char *buff, size_t buflen);
extern int	GetTimeInfo(TIMEINFO *Now);

/* Hash functions */
typedef struct {
    char        hash[16];
} HASH;
HASH Hash(const void *value, const size_t len);
/* Return the hash of a case mapped message-id */
HASH HashMessageID(const char *MessageID);
BOOL HashEmpty(const HASH hash);
void HashClear(HASH *hash);
char *HashToText(const HASH hash);
HASH TextToHash(const char *text);
int HashCompare(const HASH *h1, const HASH *h2);

/* Miscellaneous. */
extern int	dbzneedfilecount(void);
extern BOOL     MakeDirectory(char *Name, BOOL Recurse);
extern int	getfdcount(void);
extern int	wildmat(const char *text, const char *p);
extern pid_t	waitnb(int *statusp);
extern int	xread(int fd, char *p, OFFSET_T i);
extern ssize_t  xpwrite(int fd, const void *buffer, size_t size, off_t offset);
extern ssize_t  xwrite(int fd, const void *buffer, size_t size);
extern ssize_t  xwritev(int fd, const struct iovec *iov, int iovcnt);
extern int	GetResourceUsage(double *usertime, double *systime);
extern int	SetNonBlocking(int fd, BOOL flag);
extern void	CloseOnExec(int fd, int flag);
extern void	Radix32(unsigned long, char *buff);
extern char	*ReadInDescriptor(int fd, struct stat *Sbp);
extern char	*ReadInFile(const char *name, struct stat *Sbp);
extern void	TempName(char *dir, char *buff);
extern FILE	*xfopena(const char *p);
extern BOOL	fdreserve(int fdnum);
extern FILE	*Fopen(const char *p, char *type, int index);
extern int	Fclose(FILE *fp);
extern void	(*xsignal(int signum, void (*sigfunc)(int )))(int );

const char  *Aliasgetnamebyhash(const HASH hash);
HASH Aliasgethashbyhash(const HASH hash);
HASH Aliasgethashbygroup(const char *group);
const char  *Aliasgetnamebygroup(const char *group);
BOOL LoadGroupAliases(void);

extern int      argify(char *line, char ***argvp);
extern void     freeargify(char ***argvp);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* LIBINN_H */
