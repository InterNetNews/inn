/*  $Revision$
**
**  Here be declarations of functions in the InterNetNews library.
*/

#include <sys/stat.h>
#include <sys/uio.h>
#include <storage.h>

/* Memory allocation. */
    /* Worst-case alignment, in order to shut lint up. */
    /* =()<typedef @<ALIGNPTR>@	*ALIGNPTR;>()= */
typedef long	*ALIGNPTR;
extern ALIGNPTR	xmalloc(unsigned int i);
extern ALIGNPTR	xrealloc(char *p, unsigned int i);

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

/* Parameter retrieval. */

struct conf_vars {
#define	CONF_VAR_FROMHOST 0
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

#define	CONF_VAR_SERVER 1
	char *server;		/* NNTP server to post to */
#define	CONF_VAR_PATHHOST 2
	char *pathhost;		/* Host for the Path line */
#define	CONF_VAR_PATHALIAS 3
	char *pathalias;	/* Prepended Host for the Path line */
#define	CONF_VAR_ORGANIZATION 4
	char *organization;	/* Data for the Organization line */
#define	CONF_VAR_MODERATORMAILER 5
	char *moderatormailer;	/* Default host to mail moderated articles */
#define	CONF_VAR_DOMAIN 6
	char *domain;		/* Default domain of local host */
#define	CONF_VAR_MIMEVERSION 7
	char *mimeversion;	/* Default mime version */
#define	CONF_VAR_MIMECONTENTTYPE 8
	char *mimecontenttype;	/* Default Content-Type */
#define	CONF_VAR_MIMEENCODING 9
	char *mimeencoding;	/* Default encoding */
#define	CONF_VAR_HISCACHESIZE 10
	int hiscachesize;	/* Size of the history cache in kB */
#define	CONF_VAR_WIREFORMAT 11
	int wireformat;		/* Toggle for wireformat articles */
#define	CONF_VAR_XREFSLAVE 12
	int xrefslave;		/* master server for slaving */
#define	CONF_VAR_COMPLAINTS 13
	char *complaints;	/* Addr for X-Complaints-To: header */
#define	CONF_VAR_SPOOLFIRST 14
	int spoolfirst;		/* Spool newly posted article or only on error*/
#define	CONF_VAR_WRITELINKS 15
	int writelinks;		/* Write crossposts to the history */
#define	CONF_VAR_TIMER 16
	int timer;		/* Performance monitoring interval */
#define	CONF_VAR_STATUS 17
	int status;		/* Status file update interval */
#define	CONF_VAR_STORAGEAPI 18
	int storageapi;		/* Use the storage api? */
#define	CONF_VAR_ARTICLEMMAP 19
	int articlemmap;	/* mmap articles? */
#define	CONF_VAR_OVERVIEWMMAP 20
	int overviewmmap;	/* mmap overviews and indices? */
#define	CONF_VAR_MTA 21
	char *mta;		/* Which MTA to mail moderated posts */
#define	CONF_VAR_MAILCMD 22
	char *mailcmd;		/* Which command for report/control type mail */
#define	CONF_VAR_CHECKINCLUDEDTEXT 23
	int checkincludedtext;	/* Reject if too much included text */
#define	CONF_VAR_MAXFORKS 24
	int maxforks;		/* Give up after fork failure */
#define	CONF_VAR_MAXARTSIZE 25
	long maxartsize;	/* Reject articles bigger than this */
#define	CONF_VAR_NICEKIDS 26
	int nicekids;		/* Kids get niced to this */
#define	CONF_VAR_VERIFYCANCELS 27
	int verifycancels;	/* Verify cancels against article author */
#define	CONF_VAR_LOGCANCELCOMM 28
	int logcancelcomm;	/* Log "ctlinnd cancel" commands to syslog? */
#define	CONF_VAR_WANTTRASH 29
	int wanttrash;		/* Put unwanted articles in 'junk' */
#define	CONF_VAR_REMEMBERTRASH 30
	int remembertrash;	/* Put unwanted article ID's into history */
#define	CONF_VAR_LINECOUNTFUZZ 31
	int linecountfuzz;	/* Check linecount and adjust of out by more */
#define	CONF_VAR_PEERTIMEOUT 32
	int peertimeout;	/* How long peers can be inactive */
#define	CONF_VAR_CLIENTTIMEOUT 33
	int clienttimeout;	/* How long nnrpd can be inactive */
#define	CONF_VAR_ALLOWREADERS 34
	int allowreaders;	/* Allow nnrpd when server is paused */
#define	CONF_VAR_ALLOWNEWNEWS 35
	int allownewnews;	/* Allow use of the 'NEWNEWS' command */
#define	CONF_VAR_LOCALMAXARTSIZE 36
	long localmaxartsize;	/* Max article size of local postings */
#define	CONF_VAR_LOGARTSIZE 37
	int logartsize;		/* Log article sizes */
#define	CONF_VAR_LOGIPADDR 38
	int logipaddr;		/* Log by host IP address */
#define	CONF_VAR_CHANINACTTIME 39
	int chaninacttime;	/* Wait time between noticing inact chans */
#define	CONF_VAR_MAXCONNECTIONS 40
	int maxconnections;	/* Max number of incoming NNTP connections */
#define	CONF_VAR_CHANRETRYTIME 41
	int chanretrytime;	/* Wait this many secs before chan restarts */
#define	CONF_VAR_ARTCUTOFF 42
	int artcutoff;		/* Max article age */
#define	CONF_VAR_PAUSERETRYTIME 43
	int pauseretrytime;	/* Secs before seeing if pause is ended */
#define	CONF_VAR_NNTPLINKLOG 44
	int nntplinklog;	/* Put nntplink info (filename) into the log */
#define	CONF_VAR_NNTPACTSYNC 45
	int nntpactsync;	/* Log NNTP activity after this many articles */
#define	CONF_VAR_BADIOCOUNT 46
	int badiocount;		/* Read/write failures until chan is put to sleep or closed? */
#define	CONF_VAR_BLOCKBACKOFF 47
	int blockbackoff;	/* Multiplier for sleep in EWOULDBLOCK writes */
#define	CONF_VAR_ICDSYNCCOUNT 48
	int icdsynccount;	/* How many article-writes between active and history updates */
#define	CONF_VAR_BINDADDRESS 49
	char *bindaddress;	/* Which interface IP to bind to */
#define	CONF_VAR_PORT 50
	int port;		/* Which port INND should listen on */
#define	CONF_VAR_READERTRACK 51
	int readertrack;	/* Enable/Disable the reader tracking system */
#define	CONF_VAR_STRIPPOSTCC 52
	int strippostcc;	/* Strip To:, Cc: and Bcc: lines from posts */
#define	CONF_VAR_OVERVIEWNAME 53
	char *overviewname;	/* Name of the file to store overview data */

#define	CONF_VAR_KEYWORDS 54
        char keywords;		/* enable keyword generationg in overview */
#define	CONF_VAR_KEYLIMIT 55
        int keylimit;		/* max allocated space for keywords. */
#define	CONF_VAR_KEYARTLIMIT 56
        int keyartlimit;        /* Max size of an article for keyword generation */
#define	CONF_VAR_KEYMAXWORDS 57
        int keymaxwords;	/* Max count of interesting workd */

#define	CONF_VAR_PATHNEWS 58
	char *pathnews;	
#define	CONF_VAR_PATHBIN 59
	char *pathbin;
#define	CONF_VAR_PATHFILTER 60
	char *pathfilter;
#define	CONF_VAR_PATHCONTROL 61
	char *pathcontrol;
#define	CONF_VAR_PATHDB 62
	char *pathdb;
#define	CONF_VAR_PATHETC 63
	char *pathetc;
#define	CONF_VAR_PATHRUN 64
	char *pathrun;
#define	CONF_VAR_PATHLOG 65
	char *pathlog;
#define	CONF_VAR_PATHSPOOL 66
	char *pathspool;
#define	CONF_VAR_PATHARTICLES 67
	char *patharticles;
#define	CONF_VAR_PATHOVERVIEW 68
	char *pathoverview;
#define	CONF_VAR_PATHOUTGOING 69
	char *pathoutgoing;
#define	CONF_VAR_PATHINCOMING 70
	char *pathincoming;
#define	CONF_VAR_PATHARCHIVE 71
	char *patharchive;
#define	CONF_VAR_LOGSITENAME 72
	int logsitename;	/* log site names? */
#define	CONF_VAR_PATHHTTP 73
	char *pathhttp;
#define	CONF_VAR_NNRPDPOSTHOST 74
	char *nnrpdposthost;
#define	CONF_VAR_EXTENDEDDBZ 75
	int extendeddbz;
#define	CONF_VAR_NNRPDOVERSTATS 76
	int nnrpdoverstats;
#define	MAX_CONF_VAR 77
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
HASH Hash(const void *value, size_t len);
/* Return the hash of a case mapped message-id */
HASH HashMessageID(const char *MessageID);
BOOL HashEmpty(const HASH hash);
void HashClear(HASH *hash);
char *HashToText(const HASH hash);
HASH TextToHash(const char *text);
int HashCompare(const HASH *h1, const HASH *h2);

/* Overview handling */
typedef enum {OVER_CTL, OVER_DIR, OVER_NEWDIR, OVER_MODE, OVER_NEWMODE, OVER_MMAP, OVER_BUFFERED} OVERSETUP;
#define OVERINDEXPACKSIZE      (sizeof(unsigned long) + sizeof(HASH))
typedef struct _OVERINDEX {
    unsigned long       artnum;
    HASH                hash;
} OVERINDEX;

extern void OVERsetoffset(TOKEN *token, OFFSET_T *offset, unsigned char *overindex);
extern void OVERmaketoken(TOKEN *token, OFFSET_T offset, unsigned char overindex);
extern BOOL OVERsetup(OVERSETUP type, void *value);
extern BOOL OVERinit(void);
extern BOOL OVERnewinit(void);
extern BOOL OVERreinit(void);
extern BOOL OVERreplace(void);
extern int OVERgetnum(void);
extern BOOL OVERstore(TOKEN *token, char *Overdata, int Overlen);
extern char *OVERretrieve(TOKEN *token, int *Overlen);
extern BOOL OVERcancel(TOKEN *token);
extern void OVERshutdown(void);

void PackOverIndex(OVERINDEX *index, char *packedindex);
void UnpackOverIndex(char *packedindex, OVERINDEX *index);

/* Miscellaneous. */
extern int	dbzneedfilecount(void);
extern BOOL     MakeDirectory(char *Name, BOOL Recurse);
extern int	getfdcount(void);
extern int	wildmat(const char *text, const char *p);
extern PID_T	waitnb(int *statusp);
extern int	xread(int fd, char *p, OFFSET_T i);
extern int	xwrite(int fd, char *p, int i);
extern int	xwritev(int fd, struct iovec *vp, int vpcount);
extern int	LockFile(int fd, BOOL Block);
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
