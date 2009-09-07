/*  $Id$
**
**  CNFS disk/file mode header file.
*/

#ifndef	CNFS_PRIVATE_H
#define CNFS_PRIVATE_H 1

#include <sys/types.h>
#include <unistd.h>

#define _PATH_CYCBUFFCONFIG	"cycbuff.conf"

/* Page boundary on which to mmap() the CNFS article usage header.  Should be
   a multiple of the pagesize for all the architectures you expect might need
   access to your CNFS buffer.  If you don't expect to share your buffer
   across several platforms, you can use 'pagesize' here. */
#define	CNFS_HDR_PAGESIZE       16384

#define	CNFS_MAGICV1	"Cycbuff"	/* CNFSMASIZ bytes */
#define	CNFS_MAGICV2	"CBuf1"		/* CNFSMASIZ bytes */
#define	CNFS_MAGICV3	"CBuf3"		/* CNFSMASIZ bytes */
#define	CNFS_MAGICV4	"CBuf4"		/* CNFSMASIZ bytes */
#define	CNFS_DFL_BLOCKSIZE	4096	/* Unit block size we'll work with */
#define	CNFS_MAX_BLOCKSIZE	16384	/* Max unit block size */

/* Amount of data stored at beginning of CYCBUFF before the bitfield */
#define	CNFS_BEFOREBITF		512	/* Rounded up to CNFS_HDR_PAGESIZE */

struct metacycbuff;		/* Definition comes below */

#define	CNFSMAXCYCBUFFNAME	8
#define	CNFSMASIZ	8
#define	CNFSNASIZ	16	/* Effective size is 9, not 16 */
#define	CNFSPASIZ	64
#define	CNFSLASIZ	16	/* Match length of ASCII hex off_t
				   representation */

typedef struct _CYCBUFF {
  char		name[CNFSNASIZ];/* Symbolic name */
  char		path[CNFSPASIZ];/* Path to file */
  off_t         len;		/* Length of writable area, in bytes */
  off_t         free;		/* Offset (relative to byte 0 of file) to first
				   freely available byte */
  time_t	updated;	/* Time of last update to header */
  int		fd;		/* file descriptor for this cycbuff */
  uint32_t	cyclenum;	/* Number of current cycle, 0 = invalid */
  int		magicver;	/* Magic version number */
  void *	bitfield;	/* Bitfield for article in use */
  off_t         minartoffset;	/* The minimum offset allowed for article
				   storage */
  bool		needflush;	/* true if CYCBUFFEXTERN is needed to be
				   flushed */
  int		blksz;		/* Blocksize */
  struct _CYCBUFF	*next;
  bool		currentbuff;	/* true if this cycbuff is currently used */
  char		metaname[CNFSNASIZ];/* Symbolic name of meta */
  int		order;		/* Order in meta, start from 1 not 0 */
} CYCBUFF;

/*
** A structure suitable for thwapping onto disk in a quasi-portable way.
** We assume that sizeof(CYCBUFFEXTERN) < CNFS_BLOCKSIZE.
*/
typedef struct {
    char	magic[CNFSMASIZ];
    char	name[CNFSNASIZ];
    char	path[CNFSPASIZ];
    char	lena[CNFSLASIZ];	/* ASCII version of len */
    char	freea[CNFSLASIZ];	/* ASCII version of free */
    char	updateda[CNFSLASIZ];	/* ASCII version of updated */
    char	cyclenuma[CNFSLASIZ];	/* ASCII version of cyclenum */
    char	metaname[CNFSNASIZ];
    char	orderinmeta[CNFSLASIZ];
    char	currentbuff[CNFSMASIZ];
    char	blksza[CNFSLASIZ];	/* ASCII version of blksz */
} CYCBUFFEXTERN;

#define METACYCBUFF_UPDATE	25
#define REFRESH_INTERVAL	30

typedef enum {INTERLEAVE, SEQUENTIAL} METAMODE;

typedef struct metacycbuff {
  char		*name;		/* Symbolic name of the pool */
  int		count;		/* Number of files/devs in this pool */
  CYCBUFF	**members;	/* Member cycbuffs */
  int		memb_next;	/* Index to next member to write onto */
  unsigned long	write_count;	/* Number of writes since last header flush */
  struct metacycbuff	*next;
  METAMODE	metamode;
} METACYCBUFF;

typedef struct _CNFSEXPIRERULES {
  STORAGECLASS		class;
  METACYCBUFF		*dest;
  struct _CNFSEXPIRERULES	*next;
} CNFSEXPIRERULES;

typedef struct {
  long		size;		/* Size of the article */
  time_t	arrived;	/* This is the time when article arrived */
  STORAGECLASS	class;		/* storage class */
} CNFSARTHEADER;

/* uncomment below for old cnfs spool */
/* #ifdef OLD_CNFS */
typedef struct {
  long      zottf;      /* This should always be 0x01234*/
  long      size;       /* Size of the article */
  char      m_id[64];   /* We'll only store up to 63 bytes of the
	                       Message-ID, that should be good enough */
} oldCNFSARTHEADER;

#endif /* !CNFS_PRIVATE_H */
