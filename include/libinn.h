/*  $Revision$
**
**  Here be declarations of functions in the InterNetNews library.
*/

#include <sys/stat.h>
#include <sys/uio.h>

/* Memory allocation. */
    /* Worst-case alignment, in order to shut lint up. */
    /* =()<typedef @<ALIGNPTR>@	*ALIGNPTR;>()= */
typedef int	*ALIGNPTR;
extern ALIGNPTR	xmalloc(unsigned int i);
extern ALIGNPTR	xrealloc(char *p, unsigned int i);

/* Headers. */
extern char	        *GenerateMessageID(void);
extern const char	*HeaderFind(const char *Article, const char *Header, const int size);
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
extern char	*GetFQDN(void);
extern char	*GetConfigValue(char *value);
extern char	*GetFileConfigValue(char *value);
extern BOOL      GetBooleanConfigValue(char *value, BOOL DefaultValue);
extern char	*GetModeratorAddress(FILE *FromServer, FILE *ToServer, char *group);

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

/* Miscellaneous. */
typedef struct _OVERINDEX {
    long    artnum;
/* =()<    @<LSEEKVAL>@   offset;>()= */
    off_t   offset;
    int     size;
} OVERINDEX;

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
