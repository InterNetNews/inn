/*  $Revision$
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

/* Tell C++ not to mangle prototypes. */
#ifdef __cplusplus
extern "C" {
#endif

/*
**  MEMORY MANAGEMENT
*/
extern void *   xmalloc(size_t size, const char *file, int line);
extern void *   xrealloc(void *p, size_t size, const char *file, int line);

/* This function is called whenever a memory allocation fails. */
extern int (*xmemfailure)(const char *, size_t, const char *, int);


/* String handling. */
#if defined(STDC_HEADERS) || defined(HAVE_STDARG_H)
extern void *   concat(const char *first, ...);
#else
extern void *   concat();
#endif    

/* Headers. */
extern char	        *GenerateMessageID(void);
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
extern int      LockFile(int fd, BOOL block);

#ifdef HAVE_FCNTL
typedef enum { LOCK_READ, LOCK_WRITE, LOCK_UNLOCK } LOCKTYPE;

extern BOOL     LockRange(int fd, LOCKTYPE type, BOOL block,
                          OFFSET_T offset, OFFSET_T size);
#endif
    
/* Parameter retrieval. */

struct conf_vars {
	char *fromhost;		/* Host for the From line */
#if 0
||LABEL  fromhost
||
||DOC     .TP
||DOC     .I fromhost
||DOC     This is the name of the host to use when building the From header line.
||DOC     The default is the fully-qualified domain name of the local host.
||DOC     The value of the FROMHOST environment variable, if it exists,
||DOC     overrides this.

||Lines with ||SAMPLE are placed in samples/inn.conf
||SAMPLE #fromhost:      our.domain

||Lines with ||CLEAR are placed in getconfig.c::ClearInnConf()
||CLEAR if (innconf->fromhost != NULL) DISPOSE(innconf->fromhost);

||Lines with ||DEFAULT are placed in getconfig.c::SetDefaults()
||DEFAULT innconf->fromhost = NULL;
||DEFAULT if (p = getenv(_ENV_FROMHOST)) { innconf->fromhost = COPY(p); }

||Lines with ||READ are placed in getconfig.c::ReadInnConf()
||READ /*  Special: For read, must not overwrite the ENV_FROMHOST set by DEFAULT */
||READ   if (EQ(ConfigBuff,"fromhost")) {
||READ       if (innconf->fromhost == NULL) { innconf->fromhost = COPY(p); }
||READ   } else

||Lines with ||GETVALUE are placed in getconfig.c::GetConfigValue()
||GETVALUE if (EQ(value,"fromhost")) { return innconf->fromhost; }
#endif

	char *server;		/* NNTP server to post to */
	char *pathhost;		/* Host for the Path line */
	char *pathalias;	/* Prepended Host for the Path line */
	char *organization;	/* Data for the Organization line */
	char *moderatormailer;	/* Default host to mail moderated articles */
	char *domain;		/* Default domain of local host */
	int hiscachesize;	/* Size of the history cache in kB */
	int xrefslave;		/* master server for slaving */
	char *complaints;	/* Addr for X-Complaints-To: header */
	int spoolfirst;		/* Spool newly posted article or only on error*/
	int timer;		/* Performance monitoring interval */
	int status;		/* Status file update interval */
	int articlemmap;	/* mmap articles? */
	char *mta;		/* Which MTA to mail moderated posts */
	char *mailcmd;		/* Which command for report/control type mail */
	int checkincludedtext;	/* Reject if too much included text */
	int maxforks;		/* Give up after fork failure */
	long maxartsize;	/* Reject articles bigger than this */
	int nicekids;		/* Kids get niced to this */
	int verifycancels;	/* Verify cancels against article author */
	int logcancelcomm;	/* Log "ctlinnd cancel" commands to syslog? */
	int wanttrash;		/* Put unwanted articles in 'junk' */
	int remembertrash;	/* Put unwanted article ID's into history */
	int linecountfuzz;	/* Check linecount and adjust of out by more */
	int peertimeout;	/* How long peers can be inactive */
	int clienttimeout;	/* How long nnrpd can be inactive */
	int readerswhenstopped;	/* Allow nnrpd when server is paused */
	int allownewnews;	/* Allow use of the 'NEWNEWS' command */
	long localmaxartsize;	/* Max article size of local postings */
	int logartsize;		/* Log article sizes */
	int logipaddr;		/* Log by host IP address */
	int chaninacttime;	/* Wait time between noticing inact chans */
	int maxconnections;	/* Max number of incoming NNTP connections */
	int chanretrytime;	/* Wait this many secs before chan restarts */
	int artcutoff;		/* Max article age */
	int pauseretrytime;	/* Secs before seeing if pause is ended */
	int nntplinklog;	/* Put nntplink info (filename) into the log */
	int nntpactsync;	/* Log NNTP activity after this many articles */
	int badiocount;		/* Read/write failures until chan is put to sleep or closed? */
	int blockbackoff;	/* Multiplier for sleep in EWOULDBLOCK writes */
	int icdsynccount;	/* How many article-writes between active and history updates */
	char *bindaddress;	/* Which interface IP to bind to */
	char *sourceaddress;	/* Source IP for outgoing NNTP connections */
	int port;		/* Which port INND should listen on */
	int readertrack;	/* Enable/Disable the reader tracking system */
	int strippostcc;	/* Strip To:, Cc: and Bcc: lines from posts */
        char keywords;		/* enable keyword generationg in overview */
        int keylimit;		/* max allocated space for keywords. */
        int keyartlimit;        /* Max size of an article for keyword generation */
        int keymaxwords;	/* Max count of interesting workd */
        int nnrpperlauth;       /* Use perl for nnrpd authentication */
        int addnntppostinghost; /* Add NNTP-Posting-Host: header to posts */
        int addnntppostingdate; /* Add NNTP-Posting-DATE: header to posts */

	char *pathnews;	
	char *pathbin;
	char *pathfilter;
	char *pathcontrol;
	char *pathdb;
	char *pathetc;
	char *pathrun;
	char *pathlog;
	char *pathspool;
	char *patharticles;
	char *pathoverview;
	char *pathoutgoing;
	char *pathincoming;
	char *patharchive;
	char *pathtmp;
	char *pathuniover;
	int logsitename;	/* log site names? */
	char *pathhttp;
	char *nnrpdposthost;
	int nnrpdpostport;
	int nnrpdoverstats;
	int storeonxref;	/* Should SMstore() see Xref to detemine class */
	int backoff_auth;
	char *backoff_db;
	long backoff_k;
	long backoff_postfast;
	long backoff_postslow;
	long backoff_trigger;
	int refusecybercancels;
	int nnrpdcheckart;
	int nicenewnews;	/* If NEWNEWS command is used, set nice */
	int nicennrpd;
	int usecontrolchan;
	int mergetogroups;
	int noreader;
	int nnrpdauthsender;
	long cnfscheckfudgesize;
	int rlimitnofile;
	int ignorenewsgroups;
	int overcachesize;
	int enableoverview;
	int wireformat; /* enable/disable wire format for tradspool */
	char *ovmethod;
        int useoverchan; /* should innd write overview, or should overchan */
        int immediatecancel;
	int ovmmapthreshold;
};
extern struct	conf_vars *innconf;
extern char	*innconffile;
extern char	*GetFQDN(void);
extern char	*GetConfigValue(char *value);
extern char	*GetFileConfigValue(char *value);
extern BOOL      GetBooleanConfigValue(char *value, BOOL DefaultValue);
extern char	*GetModeratorAddress(FILE *FromServer, FILE *ToServer, char *group);
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
extern int	xwrite(int fd, char *p, int i);
extern int	xwritev(int fd, struct iovec *vp, int vpcount);
extern int	GetResourceUsage(double *usertime, double *systime);
extern int	SetNonBlocking(int fd, BOOL flag);
extern void	CloseOnExec(int fd, int flag);
extern void	Radix32(unsigned long, char *buff);
extern char	*INNVersion(void);
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
