/*
** Cnfs disk/file mode header file
*/

#ifndef	___CNFS_PRIVATE_H
#define ___CNFS_PRIVATE_H

#include	<unistd.h>
#include	<sys/types.h>

/* =()<#define _PATH_CYCBUFFCONFIG	"@<_PATH_CYCBUFFCONFIG>@">()= */
#define _PATH_CYCBUFFCONFIG	"/var/news/etc/cycbuff.conf"

/* =()<#define MAX_CYCBUFFS	@<MAX_CYCBUFFS>@>()= */
#define MAX_CYCBUFFS	16
/* =()<typedef @<CYCBUFF_OFF_T>@	CYCBUFF_OFF_T;>()= */
typedef off_t	CYCBUFF_OFF_T;
/* =()<#define	CNFSseek		@<CNFSseekcall>@>()= */
#define	CNFSseek		lseek

#define	CNFS_MAGICV1	"Cycbuff"	/* CNFSMASIZ bytes */
#define	CNFS_MAGICV2	"CBuf1"		/* CNFSMASIZ bytes */
#define	CNFS_MAGICV3	"CBuf3"		/* CNFSMASIZ bytes */
#define	CNFS_BLOCKSIZE	512		/* Unit block size we'll work with */
/* Amount of data stored at beginning of CYCBUFF before the bitfield */
#define	CNFS_BEFOREBITF	(1 * CNFS_BLOCKSIZE)

struct metacycbuff;		/* Definition comes below */
#define	CNFSMASIZ	8
#define	CNFSNASIZ	16	/* Effective size is 9, not 16 */
#define	CNFSPASIZ	64
#define	CNFSLASIZ	16	/* Match length of ASCII hex CYCBUFF_OFF_T
				   representation */
typedef struct {
  char		magic[CNFSMASIZ];/* Magic string found at beginning of each 
				   cycbuff: CNFS_MAGIC */
  char		name[CNFSNASIZ];	/* Symbolic name: 15 bytes */
  int		index;		/* Index number (int. use only) */
  char		path[CNFSPASIZ];	/* Path to file: 63 bytes */
  CYCBUFF_OFF_T unused;		/* No longer used; backward compat for V1 */
  CYCBUFF_OFF_T len;		/* Length of writable area, in bytes */
  CYCBUFF_OFF_T free;		/* Offset (relative to byte 0 of file) to first
				   freely available byte */
  struct metacycbuff *mymeta;	/* Pointer to my "parent" metacycbuff */
  time_t	updated;	/* Time of last update to header */
  int		fdrd;		/* O_RDONLY file descriptor for this cycbuff */
  int		fdrdwr;		/* O_RDWR file descriptor for this cycbuff,
				   for use by "innd" only! */
  int           fdrdwr_inuse;	/* True if fdrdwr is in use */
  U_INT32_T	cyclenum;	/* Number of current cycle, 0 = invalid */
  int		magicver;	/* Magic version number */
#ifdef	XXX_RAWHACK_USE_MMAP
  caddr_t	*mmap_header;	/* mmap()'ed pointer to header of cycbuff */
#endif	/* XXX_RAWHACK_USE_MMAP */
  int		articlepending;	/* Flag: true if article is pending for write
				   i.e. CNFSgetartname() called but CNFSwrote()
				   hasn't yet been called */
  caddr_t	bitfield;	/* Bitfield for article in use */
  CYCBUFF_OFF_T	minartoffset;	/* The minimum offset allowed for article
				   storage */
} CYCBUFF;
extern CYCBUFF cycbufftab[MAX_CYCBUFFS];

/*
** A structure suitable for thwapping onto disk in a quasi-portable way.
** We assume that sizeof(CYCBUFFEXTERN) < CNFS_BLOCKSIZE.
*/
typedef struct {
    char	magic[CNFSMASIZ];
    char	name[CNFSNASIZ];
    char	path[CNFSPASIZ];
    char	lena[CNFSLASIZ];		/* ASCII version of len */
    char	freea[CNFSLASIZ];	/* ASCII version of free */
    char	updateda[CNFSLASIZ];	/* ASCII version of updated */
    char	cyclenuma[CNFSLASIZ];	/* ASCII version of cyclenum */
} CYCBUFFEXTERN;

/* =()<#define MAX_META_MEMBERS	@<MAX_META_MEMBERS>@>()= */
#define MAX_META_MEMBERS	16
/* =()<#define MAX_METACYCBUFFS	@<MAX_METACYCBUFFS>@>()= */
#define MAX_METACYCBUFFS	8
/* =()<#define METACYCBUFF_UPDATE	@<METACYCBUFF_UPDATE>@>()= */
#define METACYCBUFF_UPDATE	25

#define	METACYCBUFF_INTER	0	/* Interleaved */
#define	METACYCBUFF_SEQ		1	/* Sequential */

typedef struct metacycbuff {
  char		name[16];	/* Symbolic name of the pool */
  int		type;		/* METACYCBUFF_INTER or METACYCBUFF_SEQ */
  int		count;		/* Number of files/devs in this pool */
  CYCBUFF	*members[MAX_META_MEMBERS];	/* Member cycbuffs */
  int		memb_next;	/* Index to next member to write onto */
  unsigned long	write_count;	/* Number of writes since last header flush */
  char		lastmemb[16];	/* Last cycbuff member in a sequential
				   metacycbuff to have written something.
				   A Hack(tm).  Used only in the first 
				   member cycbuff, anyway. */
} METACYCBUFF;

/* =()<#define MAX_METACYCBUFF_RULES	@<MAX_METACYCBUFF_RULES>@>()= */
#define MAX_METACYCBUFF_RULES	24

typedef struct {
  char		*pattern;
  METACYCBUFF	*dest;
} CNFSEXPIRERULES;

/* XXX old stuff #define	CNFS_ZOTTF	0x01234 */
extern long		the_true_zottf;
#define	CNFS_ZOTTF	the_true_zottf	/* Global buried in raw.c */

typedef struct {
  long		zottf;		/* This should always be 0x01234*/
  long		size;		/* Size of the article */
  char		m_id[64];	/* We'll only store up to 63 bytes of the 
				   Message-ID, that should be good enough */
} CNFSARTHEADER;

typedef struct {
    caddr_t	ptr;		/* Start of mmap'ed region */
    caddr_t	chunk;		/* Start of article CNFSARTHEADER */
    size_t	size;		/* Size of mmap'ed region */
    int		startofart;	/* Flag: true if expected to be at start of
				   article (including CNFSARTHEADER) */
} CNFSART_CHUNK;

/* NNRP-related stuff */

#define	XXX_NNRP_MAPLINEWIDTH	34	/* 15 max length for cycbuff name,
					   8 max for 32 bit byte offset,
					   8 max for 32 bit cyclenum
					   3 for colon separators and \n */
#define	XXX_NNRP_MAPFILENAME	"name.map"	/* Dumb name, huh? */
#define	XXX_NNRP_BODYWIDTH	"XXX Unused"	/* Should implement this? */
#define	XXX_NNRP_BODYSTART	"BodyStart"
#define	XXX_NNRP_LOWART		"LowArt"
#define	XXX_NNRP_LASTREWRITE	"LastRewrite"

/* GLOBAL VARS */

/* True if there will be many CNFSartmaybehere() calls in a very
   short period of time, to avoid calling time() a bizillion times. */
extern int manyartmaybeherestofollow;
extern BOOL AmNnrpd;

/* Prototypes */

int CNFSread_config(void);
void CNFSparse_part_line(char *);
void CNFSparse_metapart_line(char *);
void CNFSparse_groups_line(char *);
CYCBUFF *CNFSgetcycbuffbyname(char *);
int CNFSgetcycbuffindexbyname(char *);
METACYCBUFF *CNFSgetmetacycbuffbyname(char *);
/* XXX char *CNFSgetartname(int, char *, ARTDATA *); */
void CNFSwrote(char *, int);
void CNFSinit_disks(int);
int CNFSopenfd(char *, int);
void CNFSflushhead(CYCBUFF *);
void CNFSflushallheads(void);
int CNFSisrawartnam(char *);
char *CNFSartnam2rpnam(char *);
CYCBUFF_OFF_T CNFSartnam2offset(char *);
int CNFSgetsharedfdrdwr(char *);
/* XXX RAWART_CHUNK *CNFSmmapchunk(char *, size_t); */
/* XXX void CNFSmunmapchunk(RAWART_CHUNK *); */
CNFSARTHEADER *CNFSchunk2rah(CNFSART_CHUNK *);
char *CNFSchunk2arttext(CNFSART_CHUNK *);
char *CNFSofft2hex(CYCBUFF_OFF_T, int);
CYCBUFF_OFF_T CNFShex2offt(char *);
int CNFSartmaybehere(char *);
void CNFSsetusedbitbyname(char *, BOOL);
void CNFSsetusedbitbyrp(CYCBUFF *, CYCBUFF_OFF_T, BOOL);
void CNFSmunmapbitfields(void);
int CNFSisusedbitsetbyname(char *);
int CNFSArtMayBeHere(CYCBUFF *, CYCBUFF_OFF_T, INT32_T);
int CNFSUsedBlock(CYCBUFF *, CYCBUFF_OFF_T, BOOL, BOOL);
void CNFSpost_config_debug(void);
void CNFSpost_init_debug(void);
static TOKEN CNFSMakeToken(char *, CYCBUFF_OFF_T, INT32_T, STORAGECLASS);
static BOOL CNFSBreakToken(TOKEN, char *, CYCBUFF_OFF_T *, INT32_T *);
int CNFSGetReadFd(CYCBUFF *, char *, CYCBUFF_OFF_T, INT32_T, BOOL);
int CNFSPutReadFd(CYCBUFF *, char *, int);
int CNFSGetWriteFd(CYCBUFF *, char *, CYCBUFF_OFF_T);
int CNFSPutWriteFd(CYCBUFF *, char *, int);
int CNFSReadFreeAndCycle(CYCBUFF *);

#endif	/* ! ___CNFS_PRIVATE_H */
