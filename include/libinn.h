/*  $Revision$
**
**  Here be declarations of functions in the InterNetNews library.
*/

/* Memory allocation. */
    /* Worst-case alignment, in order to shut lint up. */
    /* =()<typedef @<ALIGNPTR>@	*ALIGNPTR;>()= */
typedef int	*ALIGNPTR;
extern ALIGNPTR	xmalloc();
extern ALIGNPTR	xrealloc();

/* Headers. */
extern char	*GenerateMessageID();
extern char	*HeaderFind();
extern void	HeaderCleanFrom();

extern struct _DDHANDLE	*DDstart();
extern void		DDcheck();
extern char		*DDend();

/* NNTP functions. */
extern int	NNTPlocalopen();
extern int	NNTPremoteopen();
extern int	NNTPconnect();
extern int	NNTPsendarticle();
extern int	NNTPsendpassword();

/* Opening the active file on a client. */
extern FILE	*CAopen();
extern FILE	*CAlistopen();
extern void	CAclose();

/* Parameter retrieval. */
extern char	*GetFQDN();
extern char	*GetConfigValue();
extern char	*GetFileConfigValue();
extern BOOL      GetBooleanConfigValue();
extern char	*GetModeratorAddress();

/* Time functions. */
typedef struct _TIMEINFO {
    time_t	time;
    long	usec;
    long	tzone;
} TIMEINFO;
extern time_t	parsedate();
extern int	GetTimeInfo();

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

/* Miscellaneous. */
typedef struct _OVERINDEX {
    long    artnum;
/* =()<    @<LSEEKVAL>@   offset;>()= */
    off_t   offset;
    int     size;
} OVERINDEX;
 
extern int	getfdcount();
extern int	wildmat();
/* =()<extern @<PID_T>@	waitnb();>()= */
extern pid_t	waitnb();
extern int	xread();
extern int	xwrite();
extern int	xwritev();
extern int	LockFile();
extern int	GetResourceUsage();
extern int	SetNonBlocking();
extern void	CloseOnExec();
extern void	Radix32();
extern char	*INNVersion();
extern char	*ReadInDescriptor();
extern char	*ReadInFile();
extern void	TempName();
extern FILE	*xfopena();
