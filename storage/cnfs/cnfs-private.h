/*
** Cnfs disk/file mode header file
*/

#ifndef	___CNFS_PRIVATE_H
#define ___CNFS_PRIVATE_H

#include	<unistd.h>
#include	<sys/types.h>

#define _PATH_CYCBUFFCONFIG	"cycbuff.conf"

/* =()<typedef @<CYCBUFF_OFF_T>@	CYCBUFF_OFF_T;>()= */
typedef off_t	CYCBUFF_OFF_T;
/* =()<#define	CNFSseek		@<CNFSseekcall>@>()= */
#define	CNFSseek		lseek
/* =()<#define	CNFS_HDR_PAGESIZE	@<CNFS_HDR_PAGESIZE>@>()= */
#define	CNFS_HDR_PAGESIZE	16384

#define	CNFS_MAGICV1	"Cycbuff"	/* CNFSMASIZ bytes */
#define	CNFS_MAGICV2	"CBuf1"		/* CNFSMASIZ bytes */
#define	CNFS_MAGICV3	"CBuf3"		/* CNFSMASIZ bytes */
#define	CNFS_BLOCKSIZE	512		/* Unit block size we'll work with */
/* Amount of data stored at beginning of CYCBUFF before the bitfield */
#define	CNFS_BEFOREBITF	(1 * CNFS_BLOCKSIZE)

struct metacycbuff;		/* Definition comes below */
#define	CNFSMAXCYCBUFFNAME	8
#define	CNFSMASIZ	8
#define	CNFSNASIZ	16	/* Effective size is 9, not 16 */
#define	CNFSPASIZ	64
#define	CNFSLASIZ	16	/* Match length of ASCII hex CYCBUFF_OFF_T
				   representation */
typedef struct _CYCBUFF {
  char		name[CNFSNASIZ];/* Symbolic name */
  char		path[CNFSPASIZ];/* Path to file */
  CYCBUFF_OFF_T len;		/* Length of writable area, in bytes */
  CYCBUFF_OFF_T free;		/* Offset (relative to byte 0 of file) to first
				   freely available byte */
  time_t	updated;	/* Time of last update to header */
  int		fd;		/* file descriptor for this cycbuff */
  U_INT32_T	cyclenum;	/* Number of current cycle, 0 = invalid */
  int		magicver;	/* Magic version number */
  caddr_t	bitfield;	/* Bitfield for article in use */
  CYCBUFF_OFF_T	minartoffset;	/* The minimum offset allowed for article
				   storage */
  BOOL		needflush;	/* true if CYCBUFFEXTERN is needed to be
				   flushed */
  struct _CYCBUFF	*next;
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
} CYCBUFFEXTERN;

#define METACYCBUFF_UPDATE	25
#define REFRESH_INTERVAL	30

typedef struct metacycbuff {
  char		*name;		/* Symbolic name of the pool */
  int		count;		/* Number of files/devs in this pool */
  CYCBUFF	**members;	/* Member cycbuffs */
  int		memb_next;	/* Index to next member to write onto */
  unsigned long	write_count;	/* Number of writes since last header flush */
  struct metacycbuff	*next;
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

typedef struct {
  long      zottf;      /* This should always be 0x01234*/
  long      size;       /* Size of the article */
  char      m_id[64];   /* We'll only store up to 63 bytes of the
	                       Message-ID, that should be good enough */
} oldCNFSARTHEADER;

#endif	/* ! ___CNFS_PRIVATE_H */
