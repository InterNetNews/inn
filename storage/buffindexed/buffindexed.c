/*  $Id$
**
**  Overview buffer and index method.
*/
#include "config.h"
#include "clibrary.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_LIMITS_H
# include <limits.h>
#endif

#include "libinn.h"
#include "macros.h"
#include "ov.h"
#include "paths.h"
#include "qio.h"
#include "ovinterface.h"
#include "storage.h"

#include "buffindexed.h"

#define	OVBUFF_MAGIC	"ovbuff"

/* ovbuff header */
#define	OVBUFFMASIZ	8
#define	OVBUFFNASIZ	16
#define	OVBUFFLASIZ	16
#define	OVBUFFPASIZ	64

#define	OVMAXCYCBUFFNAME	8

#define	OV_HDR_PAGESIZE	16384
#define	OV_BLOCKSIZE	8192
#define	OV_BEFOREBITF	(1 * OV_BLOCKSIZE)
#define	OV_FUDGE	1024

/* ovblock pointer */
typedef struct _OV {
  unsigned int	blocknum;
  short		index;
} OV;

/* ovbuff header */
typedef struct {
  char	magic[OVBUFFMASIZ];
  char	path[OVBUFFPASIZ];
  char	indexa[OVBUFFLASIZ];	/* ASCII version of index */
  char	lena[OVBUFFLASIZ];	/* ASCII version of len */
  char	totala[OVBUFFLASIZ];	/* ASCII version of total */
  char	useda[OVBUFFLASIZ];	/* ASCII version of used */
  char	freea[OVBUFFLASIZ];	/* ASCII version of free */
  char	updateda[OVBUFFLASIZ];	/* ASCII version of updated */
} OVBUFFHEAD;

/* ovbuff info */
typedef struct _OVBUFF {
  unsigned int		index;			/* ovbuff index */
  char			path[OVBUFFPASIZ];	/* Path to file */
  int			magicver;		/* Magic version number */
  int			fd;			/* file descriptor for this
						   ovbuff */
  OFFSET_T		len;			/* Length of writable area, in
						   bytes */
  OFFSET_T		base;			/* Offset (relative to byte 0 of
						   file) to base block */
  unsigned int		freeblk;		/* next free block number no
						   freeblk left if equals
						   totalblk */
  unsigned int		totalblk;		/* number of total blocks */
  unsigned int		usedblk;		/* number of used blocks */
  time_t		updated;		/* Time of last update to
						   header */
  caddr_t		bitfield;		/* Bitfield for ovbuff block in
						   use */
  BOOL			needflush;		/* true if OVBUFFHEAD is needed
						   to be flushed */
  struct _OVBUFF	*next;			/* next ovbuff */
  int			nextchunk;		/* next chunk */
#ifdef OV_DEBUG
  struct ov_trace_array	*trace;
#endif /* OV_DEBUG */
} OVBUFF;

typedef struct _OVINDEXHEAD {
  OV		next;		/* next block */
  ARTNUM	low;		/* lowest article number in the index */
  ARTNUM	high;		/* highest article number in the index */
} OVINDEXHEAD;

typedef struct _OVINDEX {
  ARTNUM	artnum;		/* article number */
  unsigned int	blocknum;	/* overview data block number */
  short		index;		/* overview data block index */
  TOKEN		token;		/* token for this article */
  OFFSET_T	offset;		/* offset from the top in the block */
  int		len;		/* length of the data */
  time_t	arrived;	/* arrived time of article */
  time_t	expires;	/* expire time of article */
} OVINDEX;

#define OVINDEXMAX	((OV_BLOCKSIZE-sizeof(OVINDEXHEAD))/sizeof(OVINDEX))

typedef struct _OVBLOCK {
  OVINDEXHEAD	ovindexhead;		/* overview index header */
  OVINDEX	ovindex[OVINDEXMAX];	/* overview index */
} OVBLOCK;

typedef struct _OVBLKS {
  OVBLOCK	*ovblock;
  caddr_t	addr;
  int		len;
  OV		indexov;
} OVBLKS;

/* Data structure for specifying a location in the group index */
typedef struct {
    int                 recno;             /* Record number in group index */
} GROUPLOC;

#ifdef OV_DEBUG
struct ov_trace	{
  time_t	occupied;
  time_t	freed;
  GROUPLOC	gloc;
};

#define	OV_TRACENUM 10
struct ov_trace_array {
  int			max;
  int			cur;
  struct ov_trace	*ov_trace;
};

struct ov_name_table {
  char			*name;
  int			recno;
  struct ov_name_table	*next;
};

STATIC struct ov_name_table *name_table = NULL;
#endif /* OV_DEBUG */

#define GROUPHEADERHASHSIZE (16 * 1024)
#define GROUPHEADERMAGIC    (~(0xf1f0f33d))

typedef struct {
  int		magic;
  GROUPLOC	hash[GROUPHEADERHASHSIZE];
  GROUPLOC	freelist;
} GROUPHEADER;

/* The group is matched based on the MD5 of the grouname. This may prove to
   be inadequate in the future, if so, the right thing to do is to is
   probably just to add a SHA1 hash into here also.  We get a really nice
   benefit from this being fixed length, we should try to keep it that way.
*/
typedef struct {
  HASH		hash;		/* MD5 hash of the group name, if */
  HASH		alias;		/* If not empty then this is the hash of the
				   group that this group is an alias for */
  ARTNUM	high;		/* High water mark in group */
  ARTNUM	low;		/* Low water mark in group */
  int		count;		/* Number of articles in group */
  int		flag;		/* Posting/Moderation Status */
  time_t	expired;	/* When last expiry */
  time_t	deleted;	/* When this was deleted, 0 otherwise */
  GROUPLOC	next;		/* Next block in this chain */
  OV		baseindex;	/* base index buff */
  OV		curindex;	/* current index buff */
  int		curindexoffset;	/* current index offset for this ovbuff */
  ARTNUM	curhigh;	/* High water mark in group */
  ARTNUM	curlow;		/* Low water mark in group */
  OV		curdata;	/* current offset for this ovbuff */
  OFFSET_T	curoffset;	/* current offset for this ovbuff */
} GROUPENTRY;

typedef struct _GIBLIST {
  OV			ov;
  struct _GIBLIST	*next;
} GIBLIST;

typedef struct _GDB {
  OV		datablk;
  caddr_t	addr;
  caddr_t	data;
  int		len;
  struct _GDB	*next;
} GROUPDATABLOCK;

typedef struct {
  char			*group;
  int			lo;
  int			hi;
  int			cur;
  BOOL			needov;
  GROUPLOC		gloc;
} OVSEARCH;

#define GROUPDATAHASHSIZE	25

STATIC GROUPDATABLOCK	*groupdatablock[GROUPDATAHASHSIZE];

typedef enum {PREPEND_BLK, APPEND_BLK} ADDINDEX;
typedef enum {SRCH_FRWD, SRCH_BKWD} SRCH;

#define	CACHETABLESIZE	128
#define	MAXCACHETIME	(60*5)

#define	_PATH_OVBUFFCONFIG	"buffindexed.conf"

STATIC char LocalLogName[] = "buffindexed";
STATIC long		pagesize = 0;
STATIC OVBUFF		*ovbufftab;
STATIC int              GROUPfd;
STATIC GROUPHEADER      *GROUPheader = NULL;
STATIC GROUPENTRY       *GROUPentries = NULL;
STATIC int              GROUPcount = 0;
STATIC GROUPLOC         GROUPemptyloc = { -1 };
#define	NULLINDEX	(-1)
STATIC OV 	        ovnull = { 0, NULLINDEX };
typedef unsigned long	ULONG;
STATIC ULONG		onarray[64], offarray[64];
STATIC int		longsize = sizeof(long);
STATIC BOOL		Nospace;
STATIC BOOL		Needunlink;
STATIC BOOL		Cutofflow;

STATIC int ovbuffmode;
STATIC int ovpadamount = 128;

STATIC GROUPLOC GROUPnewnode(void);
STATIC BOOL GROUPremapifneeded(GROUPLOC loc);
STATIC void GROUPLOCclear(GROUPLOC *loc);
STATIC BOOL GROUPLOCempty(GROUPLOC loc);
STATIC BOOL GROUPlockhash(LOCKTYPE type);
STATIC BOOL GROUPlock(GROUPLOC gloc, LOCKTYPE type);
STATIC BOOL GROUPfilesize(int count);
STATIC BOOL GROUPexpand(int mode);
STATIC void *ovopensearch(char *group, int low, int high, BOOL needov);
STATIC void ovclosesearch(void *handle, BOOL freeblock);
STATIC OVINDEX	*Gib;
STATIC GIBLIST	*Giblist;
STATIC int	Gibcount;
STATIC char	*Gdb;

#ifdef MMAP_MISSES_WRITES
/* With HP/UX, you definitely do not want to mix mmap-accesses of
   a file with read()s and write()s of the same file */
STATIC OFFSET_T mmapwrite(int fd, void *buf, OFFSET_T nbyte, OFFSET_T offset) {
  int		pagefudge, len;
  OFFSET_T	mmapoffset;
  caddr_t	addr;

  pagefudge = offset % pagesize;
  mmapoffset = offset - pagefudge;
  len = pagefudge + nbyte;

  if ((addr = mmap((caddr_t) 0, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mmapoffset)) == (caddr_t) -1) {
    return -1;
  }
  memcpy(addr+pagefudge, buf, nbyte);
  munmap(addr, len);
  return nbyte;
}
#endif /* MMAP_MISSES_WRITES */

STATIC BOOL ovparse_part_line(char *l) {
  char		*p;
  struct stat	sb;
  OFFSET_T	len, base;
  int		tonextblock;
  OVBUFF	*ovbuff, *tmp = ovbufftab;

  /* ovbuff partition name */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > OVMAXCYCBUFFNAME - 1) {
    syslog(L_ERROR, "%s: bad index in line '%s'", LocalLogName, l);
    return FALSE;
  }
  *p = '\0';
  ovbuff = NEW(OVBUFF, 1);
  ovbuff->index = strtoul(l, NULL, 10);
  for (; tmp != (OVBUFF *)NULL; tmp = tmp->next) {
    if (tmp->index == ovbuff->index) {
      syslog(L_ERROR, "%s: dupulicate index in line '%s'", LocalLogName, l);
      DISPOSE(ovbuff);
      return FALSE;
    }
  }
  l = ++p;

  /* Path to ovbuff partition */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > OVBUFFPASIZ - 1) {
    syslog(L_ERROR, "%s: bad pathname in line '%s'", LocalLogName, l);
    DISPOSE(ovbuff);
    return FALSE;
  }
  *p = '\0';
  memset(ovbuff->path, '\0', OVBUFFPASIZ);
  strcpy(ovbuff->path, l);
  if (stat(ovbuff->path, &sb) < 0) {
    syslog(L_ERROR, "%s: file '%s' does not exist, ignoring '%d'",
           LocalLogName, ovbuff->path, ovbuff->index);
    DISPOSE(ovbuff);
    return FALSE;
  }
  l = ++p;

  /* Length/size of symbolic partition */
  len = strtoul(l, NULL, 10) * (OFFSET_T)1024;     /* This value in KB in decimal */
  /*
  ** The minimum article offset will be the size of the bitfield itself,
  ** len / (blocksize * 8), plus however many additional blocks the OVBUFFHEAD
  ** external header occupies ... then round up to the next block.
  */
  base = len / (OV_BLOCKSIZE * 8) + OV_BEFOREBITF;
  tonextblock = OV_HDR_PAGESIZE - (base & (OV_HDR_PAGESIZE - 1));
  ovbuff->base = base + tonextblock;
  if (S_ISREG(sb.st_mode) && (len != sb.st_size || ovbuff->base > sb.st_size)) {
    if (len != sb.st_size)
      syslog(L_NOTICE, "%s: length mismatch '%ld' for index '%d' (%ld bytes)",
        LocalLogName, len, ovbuff->index, sb.st_size);
    if (ovbuff->base > sb.st_size)
      syslog(L_NOTICE, "%s: length must be at least '%ld' for index '%d' (%ld bytes)",
        LocalLogName, ovbuff->base, ovbuff->index, sb.st_size);
    DISPOSE(ovbuff);
    return FALSE;
  }
  ovbuff->len = len;
  ovbuff->fd = -1;
  ovbuff->next = (OVBUFF *)NULL;
  ovbuff->needflush = FALSE;
  ovbuff->bitfield = (caddr_t)NULL;
  ovbuff->nextchunk = 1;

  if (ovbufftab == (OVBUFF *)NULL)
    ovbufftab = ovbuff;
  else {
    for (tmp = ovbufftab; tmp->next != (OVBUFF *)NULL; tmp = tmp->next);
    tmp->next = ovbuff;
  }
  return TRUE;
}

/*
** ovbuffread_config() -- Read the overview partition/file configuration file.
*/

STATIC BOOL ovbuffread_config(void) {
  char		*config, *from, *to, **ctab = (char **)NULL;
  int		ctab_free = 0;  /* Index to next free slot in ctab */
  int		ctab_i;

  if ((config = ReadInFile(cpcatpath(innconf->pathetc, _PATH_OVBUFFCONFIG),
	(struct stat *)NULL)) == NULL) {
    syslog(L_ERROR, "%s: cannot read %s", LocalLogName,
	cpcatpath(innconf->pathetc, _PATH_OVBUFFCONFIG));
    DISPOSE(config);
    return FALSE;
  }
  for (from = to = config; *from; ) {
    if (*from == '#') {	/* Comment line? */
      while (*from && *from != '\n')
	from++;	/* Skip past it */
      from++;
      continue;	/* Back to top of loop */
    }
    if (*from == '\n') {	/* End or just a blank line? */
      from++;
      continue;		/* Back to top of loop */
    }
    if (ctab_free == 0)
      ctab = NEW(char *, 1);
    else
      RENEW(ctab, char *, ctab_free+1);
    /* If we're here, we've got the beginning of a real entry */
    ctab[ctab_free++] = to = from;
    while (1) {
      if (*from && *from == '\\' && *(from + 1) == '\n') {
	from += 2;	/* Skip past backslash+newline */
	while (*from && isspace((int)*from))
	  from++;
	continue;
      }
      if (*from && *from != '\n')
	*to++ = *from++;
      if (*from == '\n') {
	*to++ = '\0';
	from++;
	break;
      }
      if (! *from)
	break;
    }
  }
  for (ctab_i = 0; ctab_i < ctab_free; ctab_i++) {
    if (!ovparse_part_line(ctab[ctab_i])) {
      DISPOSE(config);
      DISPOSE(ctab);
      return FALSE;
    }
  }
  DISPOSE(config);
  DISPOSE(ctab);
  if (ovbufftab == (OVBUFF *)NULL) {
    syslog(L_ERROR, "%s: no buffindexed defined", LocalLogName);
    return FALSE;
  }
  return TRUE;
}

STATIC char hextbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'a', 'b', 'c', 'd', 'e', 'f'};

STATIC char *offt2hex(OFFSET_T offset, BOOL leadingzeros) {
  static char	buf[24];
  char	*p;

  if (sizeof(OFFSET_T) <= 4) {
    sprintf(buf, (leadingzeros) ? "%016lx" : "%lx", offset);
  } else {
    int	i;

    for (i = 0; i < OVBUFFLASIZ; i++)
      buf[i] = '0';	/* Pad with zeros to start */
    for (i = OVBUFFLASIZ - 1; i >= 0; i--) {
      buf[i] = hextbl[offset & 0xf];
      offset >>= 4;
    }
  }
  if (!leadingzeros) {
    for (p = buf; *p == '0'; p++)
	    ;
    if (*p != '\0')
      return p;
    else
      return p - 1;	/* We converted a "0" and then bypassed all the zeros */
  } else
    return buf;
}

STATIC OFFSET_T hex2offt(char *hex) {
  if (sizeof(OFFSET_T) <= 4) {
    OFFSET_T	rpofft;
    sscanf(hex, "%lx", &rpofft);
    return rpofft;
  } else {
    char		diff;
    OFFSET_T	n = (OFFSET_T) 0;

    for (; *hex != '\0'; hex++) {
      if (*hex >= '0' && *hex <= '9')
	diff = '0';
      else if (*hex >= 'a' && *hex <= 'f')
	diff = 'a' - 10;
      else if (*hex >= 'A' && *hex <= 'F')
	diff = 'A' - 10;
      else {
	/*
	** We used to have a syslog() message here, but the case
	** where we land here because of a ":" happens, er, often.
	*/
	break;
      }
      n += (*hex - diff);
      if (isalnum((int)*(hex + 1)))
	n <<= 4;
    }
    return n;
  }
}

STATIC void ovreadhead(OVBUFF *ovbuff) {
  OVBUFFHEAD	rpx;
  char		buff[OVBUFFLASIZ+1];

  memcpy(&rpx, ovbuff->bitfield, sizeof(OVBUFFHEAD));
  strncpy((char *)buff, rpx.useda, OVBUFFLASIZ);
  buff[OVBUFFLASIZ] = '\0';
  ovbuff->usedblk = (unsigned int)hex2offt((char *)buff);
  strncpy((char *)buff, rpx.freea, OVBUFFLASIZ);
  buff[OVBUFFLASIZ] = '\0';
  ovbuff->freeblk = (unsigned int)hex2offt((char *)buff);
  return;
}

STATIC void ovflushhead(OVBUFF *ovbuff) {
  OVBUFFHEAD	rpx;

  if (!ovbuff->needflush)
    return;
  memset(&rpx, 0, sizeof(OVBUFFHEAD));
  ovbuff->updated = time(NULL);
  strncpy(rpx.magic, OVBUFF_MAGIC, strlen(OVBUFF_MAGIC));
  strncpy(rpx.path, ovbuff->path, OVBUFFPASIZ);
  /* Don't use sprintf() directly ... the terminating '\0' causes grief */
  strncpy(rpx.indexa, offt2hex(ovbuff->index, TRUE), OVBUFFLASIZ);
  strncpy(rpx.lena, offt2hex(ovbuff->len, TRUE), OVBUFFLASIZ);
  strncpy(rpx.totala, offt2hex(ovbuff->totalblk, TRUE), OVBUFFLASIZ);
  strncpy(rpx.useda, offt2hex(ovbuff->usedblk, TRUE), OVBUFFLASIZ);
  strncpy(rpx.freea, offt2hex(ovbuff->freeblk, TRUE), OVBUFFLASIZ);
  strncpy(rpx.updateda, offt2hex(ovbuff->updated, TRUE), OVBUFFLASIZ);
  memcpy(ovbuff->bitfield, &rpx, sizeof(OVBUFFHEAD));
#if defined (MMAP_NEEDS_MSYNC)
#if defined (HAVE_MSYNC_3_ARG)
  msync(ovbuff->bitfield, ovbuff->base, MS_ASYNC);
#else
  msync(ovbuff->bitfield, ovbuff->base);
#endif
#endif
  ovbuff->needflush = FALSE;
  return;
}

STATIC BOOL ovlock(OVBUFF *ovbuff, LOCKTYPE type) {
  return LockRange(ovbuff->fd, type, TRUE, 0, sizeof(OVBUFFHEAD));
}

STATIC BOOL ovbuffinit_disks(void) {
  OVBUFF	*ovbuff = ovbufftab;
  char		buf[64];
  OVBUFFHEAD	*rpx;
  int		i, fd;
  OFFSET_T	tmpo;

  /*
  ** Discover the state of our ovbuffs.  If any of them are in icky shape,
  ** duck shamelessly & return FALSE.
  */
  for (; ovbuff != (OVBUFF *)NULL; ovbuff = ovbuff->next) {
    if (ovbuff->fd < 0) {
      if ((fd = open(ovbuff->path, ovbuffmode & OV_WRITE ? O_RDWR : O_RDONLY)) < 0) {
	syslog(L_ERROR, "%s: ERROR opening '%s' : %m", LocalLogName, ovbuff->path);
	return FALSE;
      } else {
	CloseOnExec(fd, 1);
	ovbuff->fd = fd;
      }
    }
    if ((ovbuff->bitfield =
	 mmap((caddr_t) 0, ovbuff->base, ovbuffmode & OV_WRITE ? (PROT_READ | PROT_WRITE) : PROT_READ,
	      MAP_SHARED, ovbuff->fd, (off_t) 0)) == (MMAP_PTR) -1) {
      syslog(L_ERROR,
	       "%s: ovinitdisks: mmap for %s offset %d len %ld failed: %m",
	       LocalLogName, ovbuff->path, 0, (long) ovbuff->base);
      return FALSE;
    }
    rpx = (OVBUFFHEAD *)ovbuff->bitfield;
    ovlock(ovbuff, LOCK_WRITE);
    if (strncmp(rpx->magic, OVBUFF_MAGIC, strlen(OVBUFF_MAGIC)) == 0) {
	ovbuff->magicver = 1;
	if (strncmp(rpx->path, ovbuff->path, OVBUFFPASIZ) != 0) {
	  syslog(L_ERROR, "%s: Path mismatch: read %s for buffindexed %s",
		   LocalLogName, rpx->path, ovbuff->path);
	  ovbuff->needflush = TRUE;
	}
	strncpy(buf, rpx->indexa, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	i = hex2offt(buf);
	if (i != ovbuff->index) {
	    syslog(L_ERROR, "%s: Mismatch: index '%d' for buffindexed %s",
		   LocalLogName, i, ovbuff->path);
	    ovlock(ovbuff, LOCK_UNLOCK);
	    return FALSE;
	}
	strncpy(buf, rpx->lena, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	tmpo = hex2offt(buf);
	if (tmpo != ovbuff->len) {
	    syslog(L_ERROR, "%s: Mismatch: read 0x%s length for buffindexed %s",
		   LocalLogName, offt2hex(tmpo, FALSE), ovbuff->path);
	    ovlock(ovbuff, LOCK_UNLOCK);
	    return FALSE;
	}
	strncpy(buf, rpx->totala, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	ovbuff->totalblk = hex2offt(buf);
	strncpy(buf, rpx->useda, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	ovbuff->usedblk = hex2offt(buf);
	strncpy(buf, rpx->freea, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	ovbuff->freeblk = hex2offt(buf);
	ovflushhead(ovbuff);
	Needunlink = FALSE;
    } else {
	ovbuff->totalblk = (ovbuff->len - ovbuff->base)/OV_BLOCKSIZE;
	if (ovbuff->totalblk < 1) {
	  syslog(L_ERROR, "%s: too small length '%ld' for buffindexed %s",
	    LocalLogName, (long) ovbuff->len, ovbuff->path);
	  ovlock(ovbuff, LOCK_UNLOCK);
	  return FALSE;
	}
	ovbuff->magicver = 1;
	ovbuff->usedblk = 0;
	ovbuff->freeblk = 0;
	ovbuff->updated = 0;
	ovbuff->needflush = TRUE;
	syslog(L_NOTICE,
		"%s: No magic cookie found for buffindexed %d, initializing",
		LocalLogName, ovbuff->index);
	ovflushhead(ovbuff);
    }
#ifdef OV_DEBUG
    ovbuff->trace = NEW(struct ov_trace_array, ovbuff->totalblk);
    memset(ovbuff->trace, '\0', sizeof(struct ov_trace_array) * ovbuff->totalblk);
#endif /* OV_DEBUG */
    ovlock(ovbuff, LOCK_UNLOCK);
  }
  return TRUE;
}

STATIC int ovusedblock(OVBUFF *ovbuff, int blocknum, BOOL set_operation, BOOL setbitvalue) {
  OFFSET_T	longoffset;
  int		bitoffset;	/* From the 'left' side of the long */
  ULONG		bitlong, mask;

  longoffset = blocknum / (sizeof(long) * 8);
  bitoffset = blocknum % (sizeof(long) * 8);
  bitlong = *((ULONG *) ovbuff->bitfield + (OV_BEFOREBITF / sizeof(long))
		+ longoffset);
  if (set_operation) {
    if (setbitvalue) {
      mask = onarray[bitoffset];
      bitlong |= mask;
    } else {
      mask = offarray[bitoffset];
      bitlong &= mask;
    }
    *((ULONG *) ovbuff->bitfield + (OV_BEFOREBITF / sizeof(long))
      + longoffset) = bitlong;
    return 2;	/* XXX Clean up return semantics */
  }
  /* It's a read operation */
  mask = onarray[bitoffset];
  /* return bitlong & mask; doesn't work if sizeof(ulong) > sizeof(int) */
  if ( bitlong & mask ) return 1; else return 0;
}

STATIC void ovnextblock(OVBUFF *ovbuff) {
  int		i, j, last, lastbit, left;
  ULONG		mask = 0x80000000;
  ULONG		*table;

  last = ovbuff->totalblk/(sizeof(long) * 8);
  if ((left = ovbuff->totalblk % (sizeof(long) * 8)) != 0) {
    last++;
  }
  table = ((ULONG *) ovbuff->bitfield + (OV_BEFOREBITF / sizeof(long)));
  for (i = ovbuff->nextchunk ; i < last ; i++) {
    if (i == last - 1 && left != 0) {
      for (j = 1 ; j < left ; j++) {
	mask |= mask >> 1;
      }
      if ((table[i] & mask) != mask)
	break;
    } else {
      if ((table[i] ^ ~0) != 0)
	break;
    }
  }
  if (i == last) {
    for (i = 0 ; i < ovbuff->nextchunk ; i++) {
      if ((table[i] ^ ~0) != 0)
        break;
    }
    if (i == ovbuff->nextchunk) {
      ovbuff->freeblk = ovbuff->totalblk;
      return;
    }
  }
  if ((i - 1) >= 0 && (last - 1 == i) && left != 0) {
    lastbit = left;
  } else {
    lastbit = sizeof(long) * 8;
  }
  for (j = 0 ; j < lastbit ; j++) {
    if ((table[i] & onarray[j]) == 0)
      break;
  }
  if (j == lastbit) {
    ovbuff->freeblk = ovbuff->totalblk;
    return;
  }
  ovbuff->freeblk = i * sizeof(long) * 8 + j;
  ovbuff->nextchunk = i + 1;
  if (i == last)
    ovbuff->nextchunk = 0;
  return;
}

STATIC OVBUFF *getovbuff(OV ov) {
  OVBUFF	*ovbuff = ovbufftab;
  for (; ovbuff != (OVBUFF *)NULL; ovbuff = ovbuff->next) {
    if (ovbuff->index == ov.index)
      return ovbuff;
  }
  return NULL;
}

#ifdef OV_DEBUG
STATIC OV ovblocknew(GROUPENTRY *ge) {
#else
STATIC OV ovblocknew(void) {
#endif /* OV_DEBUG */
  static OVBUFF	*ovbuffnext = NULL;
  OVBUFF	*ovbuff;
  OV		ov;
#ifdef OV_DEBUG
  int		recno;
  struct ov_trace_array *trace;
#endif /* OV_DEBUG */

  if (ovbuffnext == NULL)
    ovbuffnext = ovbufftab;
  for (ovbuff = ovbuffnext ; ovbuff != (OVBUFF *)NULL ; ovbuff = ovbuff->next) {
    ovlock(ovbuff, LOCK_WRITE);
    ovreadhead(ovbuff);
    if (ovbuff->totalblk != ovbuff->usedblk && ovbuff->freeblk == ovbuff->totalblk) {
      ovnextblock(ovbuff);
    }
    if (ovbuff->totalblk == ovbuff->usedblk || ovbuff->freeblk == ovbuff->totalblk) {
      /* no space left for this ovbuff */
      ovlock(ovbuff, LOCK_UNLOCK);
      continue;
    }
    break;
  }
  if (ovbuff == NULL) {
    for (ovbuff = ovbufftab ; ovbuff != ovbuffnext ; ovbuff = ovbuff->next) {
      ovlock(ovbuff, LOCK_WRITE);
      ovreadhead(ovbuff);
      if (ovbuff->totalblk == ovbuff->usedblk || ovbuff->freeblk == ovbuff->totalblk) {
	/* no space left for this ovbuff */
	ovlock(ovbuff, LOCK_UNLOCK);
	continue;
      }
      break;
    }
    if (ovbuff == ovbuffnext) {
      Nospace = TRUE;
      return ovnull;
    }
  }
#ifdef OV_DEBUG
  recno = ((char *)ge - (char *)&GROUPentries[0])/sizeof(GROUPENTRY);
  if (ovusedblock(ovbuff, ovbuff->freeblk, FALSE, TRUE)) {
    syslog(L_FATAL, "%s: 0x%08x trying to occupy new block(%d, %d), but already occupied", LocalLogName, recno, ovbuff->index, ovbuff->freeblk);
    buffindexed_close();
    abort();
  }
  trace = &ovbuff->trace[ovbuff->freeblk];
  if (trace->ov_trace == NULL) {
    trace->ov_trace = NEW(struct ov_trace, OV_TRACENUM);
    trace->max = OV_TRACENUM;
    memset(trace->ov_trace, '\0', sizeof(struct ov_trace) * OV_TRACENUM);
  } else if (trace->cur + 1 == trace->max) {
    trace->max += OV_TRACENUM;
    RENEW(trace->ov_trace, struct ov_trace, trace->max);
    memset(&trace->ov_trace[trace->cur], '\0', sizeof(struct ov_trace) * (trace->max - trace->cur));
  }
  if (trace->ov_trace[trace->cur].occupied != 0) {
    trace->cur++;
  }
  trace->ov_trace[trace->cur].gloc.recno = recno;
  trace->ov_trace[trace->cur].occupied = time(NULL);
#endif /* OV_DEBUG */
  ov.index = ovbuff->index;
  ov.blocknum = ovbuff->freeblk;
  ovusedblock(ovbuff, ov.blocknum, TRUE, TRUE);
  ovnextblock(ovbuff);
  ovbuff->usedblk++;
  ovbuff->needflush = TRUE;
  ovflushhead(ovbuff);
  ovlock(ovbuff, LOCK_UNLOCK);
  ovbuffnext = ovbuff->next;
  if (ovbuffnext == NULL)
    ovbuffnext = ovbufftab;
  return ov;
}

#ifdef OV_DEBUG
STATIC void ovblockfree(OV ov, GROUPENTRY *ge) {
#else
STATIC void ovblockfree(OV ov) {
#endif /* OV_DEBUG */
  OVBUFF	*ovbuff;
#ifdef OV_DEBUG
  int		recno;
  struct ov_trace_array *trace;
#endif /* OV_DEBUG */

  if (ov.index == NULLINDEX)
    return;
  if ((ovbuff = getovbuff(ov)) == NULL)
    return;
  ovlock(ovbuff, LOCK_WRITE);
#ifdef OV_DEBUG
  recno = ((char *)ge - (char *)&GROUPentries[0])/sizeof(GROUPENTRY);
  if (!ovusedblock(ovbuff, ov.blocknum, FALSE, FALSE)) {
    syslog(L_FATAL, "%s: 0x%08x trying to free block(%d, %d), but already freed", LocalLogName, recno, ov.index, ov.blocknum);
    buffindexed_close();
    abort();
  }
  trace = &ovbuff->trace[ov.blocknum];
  if (trace->ov_trace == NULL) {
    trace->ov_trace = NEW(struct ov_trace, OV_TRACENUM);
    trace->max = OV_TRACENUM;
    memset(trace->ov_trace, '\0', sizeof(struct ov_trace) * OV_TRACENUM);
  } else if (trace->cur + 1 == trace->max) {
    trace->max += OV_TRACENUM;
    RENEW(trace->ov_trace, struct ov_trace, trace->max);
    memset(&trace->ov_trace[trace->cur], '\0', sizeof(struct ov_trace) * (trace->max - trace->cur));
  }
  if (trace->ov_trace[trace->cur].freed != 0) {
    trace->cur++;
  }
  trace->ov_trace[trace->cur].freed = time(NULL);
  trace->ov_trace[trace->cur].gloc.recno = recno;
  trace->cur++;
#endif /* OV_DEBUG */
  ovusedblock(ovbuff, ov.blocknum, TRUE, FALSE);
  ovreadhead(ovbuff);
  if (ovbuff->freeblk == ovbuff->totalblk)
    ovbuff->freeblk = ov.blocknum;
  ovbuff->usedblk--;
  ovbuff->needflush = TRUE;
  ovflushhead(ovbuff);
  ovlock(ovbuff, LOCK_UNLOCK);
  return;
}

BOOL buffindexed_open(int mode) {
  char		dirname[1024];
  char		*groupfn;
  struct stat	sb;
  int		i, flag = 0;
  static int	uninitialized = 1;
  ULONG		on, off;

  if (uninitialized) {
    on = 1;
    off = on;
    off ^= ULONG_MAX;
    for (i = (longsize * 8) - 1; i >= 0; i--) {
      onarray[i] = on;
      offarray[i] = off;
      on <<= 1;
      off = on;
      off ^= ULONG_MAX;
    }
    uninitialized = 0;
  }
  ovbuffmode = mode;
  if (pagesize == 0) {
#if	defined(HAVE_GETPAGESIZE)
    pagesize = getpagesize();
#elif	defined(_SC_PAGESIZE)
    if ((pagesize = sysconf(_SC_PAGESIZE)) < 0) {
      syslog(L_ERROR, "%s: sysconf(_SC_PAGESIZE) failed: %m", LocalLogName);
      return FALSE;
    }
#else
    pagesize = 16384;
#endif
    if ((pagesize > OV_HDR_PAGESIZE) || (OV_HDR_PAGESIZE % pagesize)) {
      syslog(L_ERROR, "%s: OV_HDR_PAGESIZE (%d) is not a multiple of pagesize (%ld)", LocalLogName, OV_HDR_PAGESIZE, pagesize);
      return FALSE;
    }
  }
  memset(&groupdatablock, '\0', sizeof(groupdatablock));
  if (!ovbuffread_config()) {
    return FALSE;
  }
  Needunlink = TRUE;
  if (!ovbuffinit_disks()) {
    return FALSE;
  }

  strcpy(dirname, innconf->pathdb);
  groupfn = NEW(char, strlen(dirname) + strlen("/group.index") + 1);
  strcpy(groupfn, dirname);
  strcat(groupfn, "/group.index");
  if (Needunlink && unlink(groupfn) == 0) {
    syslog(L_NOTICE, "%s: all buffers are brandnew, unlink '%s'", LocalLogName, groupfn);
  }
  GROUPfd = open(groupfn, ovbuffmode & OV_WRITE ? O_RDWR | O_CREAT : O_RDONLY, 0660);
  if (GROUPfd < 0) {
    syslog(L_FATAL, "%s: Could not create %s: %m", LocalLogName, groupfn);
    DISPOSE(groupfn);
    return FALSE;
  }

  if (fstat(GROUPfd, &sb) < 0) {
    syslog(L_FATAL, "%s: Could not fstat %s: %m", LocalLogName, groupfn);
    DISPOSE(groupfn);
    close(GROUPfd);
    return FALSE;
  }
  if (sb.st_size > sizeof(GROUPHEADER)) {
    if (mode & OV_READ)
      flag |= PROT_READ;
    if (mode & OV_WRITE) {
      /*
       * Note: below mapping of groupheader won't work unless we have
       * both PROT_READ and PROT_WRITE perms.
       */
      flag |= PROT_WRITE|PROT_READ;
    }
    GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
    if ((GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount), flag,
	MAP_SHARED, GROUPfd, 0)) == (GROUPHEADER *) -1) {
      syslog(L_FATAL, "%s: Could not mmap %s in buffindexed_open: %m", LocalLogName, groupfn);
      DISPOSE(groupfn);
      close(GROUPfd);
      return FALSE;
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
  } else {
    GROUPcount = 0;
    if (!GROUPexpand(mode)) {
      DISPOSE(groupfn);
      close(GROUPfd);
      return FALSE;
    }
  }
  CloseOnExec(GROUPfd, 1);

  DISPOSE(groupfn);
  Cutofflow = FALSE;

  return TRUE;
}

STATIC GROUPLOC GROUPfind(char *group, BOOL Ignoredeleted) {
  HASH		grouphash;
  unsigned int	i;
  GROUPLOC	loc;

  grouphash = Hash(group, strlen(group));
  memcpy(&i, &grouphash, sizeof(i));

  loc = GROUPheader->hash[i % GROUPHEADERHASHSIZE];
  GROUPremapifneeded(loc);

  while (!GROUPLOCempty(loc)) {
    if (GROUPentries[loc.recno].deleted == 0 || Ignoredeleted) {
      if (memcmp(&grouphash, &GROUPentries[loc.recno].hash, sizeof(HASH)) == 0) {
	return loc;
      }
    }
    loc = GROUPentries[loc.recno].next;
  }
  return GROUPemptyloc;
}

BOOL buffindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag) {
  GROUPLOC	gloc;

  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    return FALSE;
  }
  GROUPlock(gloc, LOCK_READ);
  if (lo != NULL)
    *lo = GROUPentries[gloc.recno].low;
  if (hi != NULL)
    *hi = GROUPentries[gloc.recno].high;
  if (count != NULL)
    *count = GROUPentries[gloc.recno].count;
  if (flag != NULL)
    *flag = GROUPentries[gloc.recno].flag;
  GROUPlock(gloc, LOCK_UNLOCK);
  return TRUE;
}

STATIC void setinitialge(GROUPENTRY *ge, HASH grouphash, char *flag, GROUPLOC next, ARTNUM lo, ARTNUM hi) {
  ge->hash = grouphash;
  if (lo != 0)
    ge->low = lo;
  ge->high = hi;
  ge->expired = ge->deleted = ge->count = 0;
  ge->flag = *flag;
  ge->baseindex = ge->curindex = ge->curdata = ovnull;
  ge->curindexoffset = ge->curoffset = 0;
  ge->next = next;
}

BOOL buffindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag) {
  unsigned int	i;
  HASH		grouphash;
  GROUPLOC	gloc;
  GROUPENTRY	*ge;
  void		*handle;
#ifdef OV_DEBUG
  struct ov_name_table	*ntp;
#endif /* OV_DEBUG */

  gloc = GROUPfind(group, TRUE);
  if (!GROUPLOCempty(gloc)) {
    ge = &GROUPentries[gloc.recno];
    if (GROUPentries[gloc.recno].deleted != 0) {
      grouphash = Hash(group, strlen(group));
      setinitialge(ge, grouphash, flag, ge->next, lo, hi);
    } else {
      ge->flag = *flag;
    }
    return TRUE;
  }
  grouphash = Hash(group, strlen(group));
  memcpy(&i, &grouphash, sizeof(i));
  i = i % GROUPHEADERHASHSIZE;
  GROUPlockhash(LOCK_WRITE);
  gloc = GROUPnewnode();
  ge = &GROUPentries[gloc.recno];
  setinitialge(ge, grouphash, flag, GROUPheader->hash[i], lo, hi);
  GROUPheader->hash[i] = gloc;
#ifdef OV_DEBUG
  ntp = NEW(struct ov_name_table, 1);
  memset(ntp, '\0', sizeof(struct ov_name_table));
  ntp->name = COPY(group);
  ntp->recno = gloc.recno;
  if (name_table == NULL)
    name_table = ntp;
  else {
    ntp->next = name_table;
    name_table = ntp;
  }
#endif /* OV_DEBUG */
  GROUPlockhash(LOCK_UNLOCK);
  return TRUE;
}

STATIC BOOL GROUPfilesize(int count) {
  return (count * sizeof(GROUPENTRY)) + sizeof(GROUPHEADER);
}

/* Check if the given GROUPLOC refers to GROUPENTRY that we don't have mmap'ed,
** if so then see if the file has been grown by another writer and remmap
*/
STATIC BOOL GROUPremapifneeded(GROUPLOC loc) {
  struct stat	sb;

  if (loc.recno < GROUPcount)
    return TRUE;

  if (fstat(GROUPfd, &sb) < 0)
    return FALSE;

  if (GROUPfilesize(GROUPcount) >= sb.st_size)
    return TRUE;

  if (GROUPheader) {
    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
      syslog(L_FATAL, "%s: Could not munmap group.index in GROUPremapifneeded: %m", LocalLogName);
      return FALSE;
    }
  }

  GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
  GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
				     PROT_READ | PROT_WRITE, MAP_SHARED, GROUPfd, 0);
  if (GROUPheader == (GROUPHEADER *) -1) {
    syslog(L_FATAL, "%s: Could not mmap group.index in GROUPremapifneeded: %m", LocalLogName);
    return FALSE;
  }
  GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
  return TRUE;
}

/* This function does not need to lock because it's callers are expected to do so */
STATIC BOOL GROUPexpand(int mode) {
  int	i;
  int	flag = 0;

  if (GROUPheader) {
    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
      syslog(L_FATAL, "%s: Could not munmap group.index in GROUPexpand: %m", LocalLogName);
      return FALSE;
    }
  }
  GROUPcount += 1024;
  if (ftruncate(GROUPfd, GROUPfilesize(GROUPcount)) < 0) {
    syslog(L_FATAL, "%s: Could not extend group.index: %m", LocalLogName);
    return FALSE;
  }
  if (mode & OV_READ)
    flag |= PROT_READ;
  if (mode & OV_WRITE) {
    /*
     * Note: below check of magic won't work unless we have both PROT_READ
     * and PROT_WRITE perms.
     */
    flag |= PROT_WRITE|PROT_READ;
  }
  GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
				     flag, MAP_SHARED, GROUPfd, 0);
  if (GROUPheader == (GROUPHEADER *) -1) {
    syslog(L_FATAL, "%s: Could not mmap group.index in GROUPexpand: %m", LocalLogName);
    return FALSE;
  }
  GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
  if (GROUPheader->magic != GROUPHEADERMAGIC) {
    GROUPheader->magic = GROUPHEADERMAGIC;
    GROUPLOCclear(&GROUPheader->freelist);
    for (i = 0; i < GROUPHEADERHASHSIZE; i++)
      GROUPLOCclear(&GROUPheader->hash[i]);
  }
  /* Walk the new entries from the back to the front, adding them to the freelist */
  for (i = GROUPcount - 1; (GROUPcount - 1024) <= i; i--) {
    GROUPentries[i].next = GROUPheader->freelist;
    GROUPheader->freelist.recno = i;
  }
  return TRUE;
}

STATIC GROUPLOC GROUPnewnode(void) {
  GROUPLOC	loc;

  /* If we didn't find any free space, then make some */
  if (GROUPLOCempty(GROUPheader->freelist)) {
    if (!GROUPexpand(ovbuffmode)) {
      return GROUPemptyloc;
    }
  }
  assert(!GROUPLOCempty(GROUPheader->freelist));
  loc = GROUPheader->freelist;
  GROUPheader->freelist = GROUPentries[GROUPheader->freelist.recno].next;
  return loc;
}

BOOL buffindexed_groupdel(char *group) {
  GROUPLOC	gloc;
  GROUPENTRY	*ge;
  void		*handle;

  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    return TRUE;
  }
  GROUPlock(gloc, LOCK_WRITE);
  ge = &GROUPentries[gloc.recno];
  ge->deleted = time(NULL);
  HashClear(&ge->hash);
  GROUPlock(gloc, LOCK_UNLOCK);
  return TRUE;
}

STATIC void GROUPLOCclear(GROUPLOC *loc) {
  loc->recno = -1;
}

STATIC BOOL GROUPLOCempty(GROUPLOC loc) {
  return (loc.recno < 0);
}

STATIC BOOL GROUPlockhash(LOCKTYPE type) {
  return LockRange(GROUPfd, type, TRUE, 0, sizeof(GROUPHEADER));
}

STATIC BOOL GROUPlock(GROUPLOC gloc, LOCKTYPE type) {
  return LockRange(GROUPfd,
	     type,
	     TRUE,
	     sizeof(GROUPHEADER) + (sizeof(GROUPENTRY) * gloc.recno),
	     sizeof(GROUPENTRY));
}

#ifdef OV_DEBUG
STATIC BOOL ovsetcurindexblock(GROUPENTRY *ge, GROUPENTRY *georig) {
#else
STATIC BOOL ovsetcurindexblock(GROUPENTRY *ge) {
#endif /* OV_DEBUG */
  OVBUFF	*ovbuff;
  OV		ov;
  int		delta, i;
  OVINDEXHEAD	ovindexhead;

  /* there is no index */
#ifdef OV_DEBUG
  ov = ovblocknew(georig ? georig : ge);
#else
  ov = ovblocknew();
#endif /* OV_DEBUG */
  if (ov.index == NULLINDEX) {
    syslog(L_ERROR, "%s: ovsetcurindexblock could not get new block", LocalLogName);
    return FALSE;
  }
  if ((ovbuff = getovbuff(ov)) == NULL) {
    syslog(L_ERROR, "%s: ovsetcurindexblock could not get ovbuff block for new, %d, %d", LocalLogName, ov.index, ov.blocknum);
    return FALSE;
  }
  ovindexhead.next = ovnull;
  ovindexhead.low = 0;
  ovindexhead.high = 0;
#ifdef MMAP_MISSES_WRITES
  if (mmapwrite(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ov.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
#else
  if (pwrite(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ov.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
#endif /* MMAP_MISSES_WRITES */
    syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
    return TRUE;
  }
  if (ge->baseindex.index == NULLINDEX) {
    ge->baseindex = ov;
  } else {
    if ((ovbuff = getovbuff(ge->curindex)) == NULL)
      return FALSE;
#ifdef OV_DEBUG
    if (!ovusedblock(ovbuff, ge->curindex.blocknum, FALSE, FALSE)) {
      syslog(L_FATAL, "%s: block(%d, %d) not occupied (index)", LocalLogName, ovbuff->index, ge->curindex.blocknum);
      abort();
    }
#endif /* OV_DEBUG */
    ovindexhead.next = ov;
    ovindexhead.low = ge->curlow;
    ovindexhead.high = ge->curhigh;
#ifdef MMAP_MISSES_WRITES
    if (mmapwrite(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
#else
    if (pwrite(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
#endif /* MMAP_MISSES_WRITES */
      syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
      return FALSE;
    }
  }
  ge->curindex = ov;
  ge->curindexoffset = 0;
  ge->curlow = 0;
  ge->curhigh = 0;
  return TRUE;
}

#ifdef OV_DEBUG
STATIC BOOL ovaddrec(GROUPENTRY *ge, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires, GROUPENTRY *georig) {
#else
STATIC BOOL ovaddrec(GROUPENTRY *ge, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
#endif /* OV_DEBUG */
  OV		ov;
  OVINDEX	ie;
  OVBUFF	*ovbuff;
  OVINDEXHEAD	ovindexhead;
  BOOL		needupdate = FALSE;
#ifdef OV_DEBUG
  int		recno;
#endif /* OV_DEBUG */

  Nospace = FALSE;
  if (OV_BLOCKSIZE < len) {
    syslog(L_ERROR, "%s: overview data must be under %d", LocalLogName, OV_BLOCKSIZE);
    return FALSE;
  }
  if (ge->curdata.index == NULLINDEX) {
    /* no data block allocated */
#ifdef OV_DEBUG
    ov = ovblocknew(georig ? georig : ge);
#else
    ov = ovblocknew();
#endif /* OV_DEBUG */
    if (ov.index == NULLINDEX) {
      syslog(L_ERROR, "%s: ovaddrec could not get new block", LocalLogName);
      return FALSE;
    }
    if ((ovbuff = getovbuff(ov)) == NULL) {
      syslog(L_ERROR, "%s: ovaddrec could not get ovbuff block for new, %d, %d, %ld", LocalLogName, ov.index, ov.blocknum, artnum);
      return FALSE;
    }
    ge->curdata = ov;
    ge->curoffset = 0;
  } else if ((ovbuff = getovbuff(ge->curdata)) == NULL)
    return FALSE;
  else if (OV_BLOCKSIZE - ge->curoffset < len) {
    /* too short to store data, allocate new block */
#ifdef OV_DEBUG
    ov = ovblocknew(georig ? georig : ge);
#else
    ov = ovblocknew();
#endif /* OV_DEBUG */
    if (ov.index == NULLINDEX) {
      syslog(L_ERROR, "%s: ovaddrec could not get new block", LocalLogName);
      return FALSE;
    }
    if ((ovbuff = getovbuff(ov)) == NULL) {
      syslog(L_ERROR, "%s: ovaddrec could not get ovbuff block for new, %d, %d, %ld", LocalLogName, ov.index, ov.blocknum, artnum);
      return FALSE;
    }
    ge->curdata = ov;
    ge->curoffset = 0;
  }
#ifdef OV_DEBUG
  if (!ovusedblock(ovbuff, ge->curdata.blocknum, FALSE, FALSE)) {
    syslog(L_FATAL, "%s: block(%d, %d) not occupied", LocalLogName, ovbuff->index, ge->curdata.blocknum);
    buffindexed_close();
    abort();
  }
#endif /* OV_DEBUG */
#ifdef MMAP_MISSES_WRITES
  if (mmapwrite(ovbuff->fd, data, len, ovbuff->base + ge->curdata.blocknum * OV_BLOCKSIZE + ge->curoffset) != len) {
#else
  if (pwrite(ovbuff->fd, data, len, ovbuff->base + ge->curdata.blocknum * OV_BLOCKSIZE + ge->curoffset) != len) {
#endif /* MMAP_MISSES_WRITES */
    syslog(L_ERROR, "%s: could not append overview record index '%d', blocknum '%d': %m", LocalLogName, ge->curdata.index, ge->curdata.blocknum);
    return FALSE;
  }
  memset(&ie, '\0', sizeof(ie));
  ie.artnum = artnum;
  ie.len = len;
  ie.index = ge->curdata.index;
  ie.blocknum = ge->curdata.blocknum;
  ie.offset = ge->curoffset;
  ie.token = token;
  ie.arrived = arrived;
  ie.expires = expires;

  if (ge->baseindex.index == NULLINDEX || ge->curindexoffset == OVINDEXMAX) {
#ifdef OV_DEBUG
    if (!ovsetcurindexblock(ge, georig)) {
#else
    if (!ovsetcurindexblock(ge)) {
#endif /* OV_DEBUG */
      syslog(L_ERROR, "%s: could not set current index", LocalLogName);
      return FALSE;
    }
  }
  if ((ovbuff = getovbuff(ge->curindex)) == NULL)
    return FALSE;
#ifdef OV_DEBUG
  if (!ovusedblock(ovbuff, ge->curindex.blocknum, FALSE, FALSE)) {
    syslog(L_FATAL, "%s: block(%d, %d) not occupied (index)", LocalLogName, ovbuff->index, ge->curindex.blocknum);
    buffindexed_close();
    abort();
  }
#endif /* OV_DEBUG */
#ifdef MMAP_MISSES_WRITES
  if (mmapwrite(ovbuff->fd, &ie, sizeof(ie), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE + sizeof(OVINDEXHEAD) + sizeof(ie) * ge->curindexoffset) != sizeof(ie)) {
#else
  if (pwrite(ovbuff->fd, &ie, sizeof(ie), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE + sizeof(OVINDEXHEAD) + sizeof(ie) * ge->curindexoffset) != sizeof(ie)) {
#endif /* MMAP_MISSES_WRITES */
    syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
    return TRUE;
  }
  if ((ge->curlow <= 0) || (ge->curlow > artnum)) {
    ge->curlow = artnum;
    needupdate = TRUE;
  }
  if ((ge->curhigh <= 0) || (ge->curhigh < artnum)) {
    ge->curhigh = artnum;
    needupdate = TRUE;
  }
  if (needupdate) {
    ovindexhead.next = ovnull;
    ovindexhead.low = ge->curlow;
    ovindexhead.high = ge->curhigh;
#ifdef MMAP_MISSES_WRITES
    if (mmapwrite(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
#else
    if (pwrite(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
#endif /* MMAP_MISSES_WRITES */
      syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
      return TRUE;
    }
  }
  if ((ge->low <= 0) || (ge->low > artnum))
    ge->low = artnum;
  if ((ge->high <= 0) || (ge->high < artnum))
    ge->high = artnum;
  ge->curindexoffset++;
  ge->curoffset += len;
  ge->count++;
  return TRUE;
}

BOOL buffindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
  GROUPLOC	gloc;
  GROUPENTRY	*ge;

  if (len > OV_BLOCKSIZE) {
    syslog(L_ERROR, "%s: overview data is too large %d", LocalLogName, len);
    return TRUE;
  }

  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    return TRUE;
  }
  GROUPlock(gloc, LOCK_WRITE);
  /* prepend block(s) if needed. */
  ge = &GROUPentries[gloc.recno];
  if (Cutofflow && ge->low > artnum) {
    GROUPlock(gloc, LOCK_UNLOCK);
    return TRUE;
  }
#ifdef OV_DEBUG
  if (!ovaddrec(ge, artnum, token, data, len, arrived, expires, NULL)) {
#else
  if (!ovaddrec(ge, artnum, token, data, len, arrived, expires)) {
#endif /* OV_DEBUG */
    if (Nospace) {
      GROUPlock(gloc, LOCK_UNLOCK);
      syslog(L_ERROR, "%s: no space left for buffer, adding '%s'", LocalLogName, group);
      return FALSE;
    }
    syslog(L_ERROR, "%s: could not add overview for '%s'", LocalLogName, group);
  }
  GROUPlock(gloc, LOCK_UNLOCK);

  return TRUE;
}

BOOL buffindexed_cancel(TOKEN token) {
    return TRUE;
}

#ifdef OV_DEBUG
STATIC void freegroupblock(GROUPENTRY *ge) {
#else
STATIC void freegroupblock(void) {
#endif /* OV_DEBUG */
  GROUPDATABLOCK	*gdb;
  int			i;
  OV			ov;
  GIBLIST		*giblist;

  for (giblist = Giblist ; giblist != NULL ; giblist = giblist->next) {
#ifdef OV_DEBUG
    ovblockfree(giblist->ov, ge);
#else
    ovblockfree(giblist->ov);
#endif /* OV_DEBUG */
  }
  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdb->next) {
#ifdef OV_DEBUG
      ovblockfree(gdb->datablk, ge);
#else
      ovblockfree(gdb->datablk);
#endif /* OV_DEBUG */
    }
  }
}

STATIC void ovgroupunmap(void) {
  GROUPDATABLOCK	*gdb, *gdbnext;
  int			i;
  GIBLIST		*giblist, *giblistnext;

  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdbnext) {
      gdbnext = gdb->next;
      DISPOSE(gdb);
    }
    groupdatablock[i] = NULL;
  }
  for (giblist = Giblist ; giblist != NULL ; giblist = giblistnext) {
    giblistnext = giblist->next;
    DISPOSE(giblist);
  }
  Giblist = NULL;
  if (Gib != NULL) {
    DISPOSE(Gib);
    Gib = NULL;
  }
  if (Gdb != NULL) {
    DISPOSE(Gdb);
    Gdb = NULL;
  }
}

STATIC void insertgdb(OV *ov, GROUPDATABLOCK *gdb) {
  gdb->next = groupdatablock[(ov->index + ov->blocknum) % GROUPDATAHASHSIZE];
  groupdatablock[(ov->index + ov->blocknum) % GROUPDATAHASHSIZE] = gdb;
  return;
}

STATIC GROUPDATABLOCK *searchgdb(OV *ov) {
  GROUPDATABLOCK	*gdb;

  gdb = groupdatablock[(ov->index + ov->blocknum) % GROUPDATAHASHSIZE];
  for (; gdb != NULL ; gdb = gdb->next) {
    if (ov->index == gdb->datablk.index && ov->blocknum == gdb->datablk.blocknum)
      break;
  }
  return gdb;
}

STATIC int INDEXcompare(CPOINTER p1, CPOINTER p2) {
  OVINDEX	*oi1;
  OVINDEX	*oi2;
 
  oi1 = (OVINDEX *)p1;
  oi2 = (OVINDEX *)p2;  
  return oi1->artnum - oi2->artnum;
}

STATIC BOOL ovgroupmmap(GROUPENTRY *ge, int low, int high, BOOL needov) {
  OV			ov = ge->baseindex;
  OVBUFF		*ovbuff;
  GROUPDATABLOCK	*gdb;
  int			pagefudge, base, limit, i, count, len;
  OFFSET_T		offset, mmapoffset;
  OVBLOCK		*ovblock;
  caddr_t		addr;
  GIBLIST		*giblist;

  Gibcount = 0;
  if (high - low < 0)
    return TRUE;
  i = 0;
  if (high - low + 1 < ge->count)
    Gibcount = high - low + 1;
  else
    Gibcount = ge->count;
  if (Gibcount == 0)
    return TRUE;
  Gib = NEW(OVINDEX, Gibcount);
  count = 0;
  while (ov.index != NULLINDEX) {
    ovbuff = getovbuff(ov);
    if (ovbuff == NULL) {
      syslog(L_ERROR, "%s: ovgroupmmap ovbuff is null(ovindex is %d, ovblock is %d", LocalLogName, ov.index, ov.blocknum);
      ovgroupunmap();
      return FALSE;
    }
    offset = ovbuff->base + (ov.blocknum * OV_BLOCKSIZE);
    pagefudge = offset % pagesize;
    mmapoffset = offset - pagefudge;
    len = pagefudge + OV_BLOCKSIZE;
    if ((addr = mmap((caddr_t) 0, len, PROT_READ, MAP_SHARED, ovbuff->fd, mmapoffset)) == (MMAP_PTR) -1) {
      syslog(L_ERROR, "%s: ovgroupmmap could not mmap index block: %m", LocalLogName);
      ovgroupunmap();
      return FALSE;
    }
    ovblock = (OVBLOCK *)(addr + pagefudge);
    if (low > ovblock->ovindexhead.high || high < ovblock->ovindexhead.low) {
      ov = ovblock->ovindexhead.next;
      munmap(addr, len);
      continue;
    }
    if (ov.index == ge->curindex.index && ov.blocknum == ge->curindex.blocknum) {
      limit = ge->curindexoffset;
    } else {
      limit = OVINDEXMAX;
    }
    for (i = 0 ; i < limit ; i++) {
      if (ovblock->ovindex[i].artnum >= low && ovblock->ovindex[i].artnum <= high) {
	if (Gibcount == count) {
	  Gibcount += OV_FUDGE;
	  RENEW(Gib, OVINDEX, Gibcount);
	}
	Gib[count++] = ovblock->ovindex[i];
      }
    }
    giblist = NEW(GIBLIST, 1);
    giblist->ov = ov;
    giblist->next = Giblist;
    Giblist = giblist;
    ov = ovblock->ovindexhead.next;
    munmap(addr, len);
  }
  Gibcount = count;
  qsort((POINTER)Gib, Gibcount, sizeof(OVINDEX), INDEXcompare);
  /* Remove duplicates. */
  for (i = 0; i < Gibcount - 1; i++) {
    if (Gib[i].artnum == Gib[i+1].artnum) {
      /* lower position is removed */
      Gib[i].artnum = 0;
    }
  }
  if (!needov)
    return TRUE;
  count = 0;
  for (i = 0 ; i < Gibcount ; i++) {
    if (Gib[i].artnum == 0)
      continue;
    ov.index = Gib[i].index;
    ov.blocknum = Gib[i].blocknum;
    gdb = searchgdb(&ov);
    if (gdb != NULL)
      continue;
    ovbuff = getovbuff(ov);
    if (ovbuff == NULL)
      continue;
    gdb = NEW(GROUPDATABLOCK, 1);
    gdb->datablk = ov;
    gdb->next = NULL;
    insertgdb(&ov, gdb);
    count++;
  }
  if (count == 0)
    return TRUE;
  Gdb = NEW(char, count * OV_BLOCKSIZE);
  count = 0;
  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdb->next) {
      ov = gdb->datablk;
      ovbuff = getovbuff(ov);
      offset = ovbuff->base + (ov.blocknum * OV_BLOCKSIZE);
      pagefudge = offset % pagesize;
      mmapoffset = offset - pagefudge;
      gdb->len = pagefudge + OV_BLOCKSIZE;
      if ((gdb->addr = mmap((caddr_t) 0, gdb->len, PROT_READ, MAP_SHARED, ovbuff->fd, mmapoffset)) == (MMAP_PTR) -1) {
        syslog(L_ERROR, "%s: ovgroupmmap could not mmap data block: %m", LocalLogName);
        DISPOSE(gdb);
        ovgroupunmap();
        return FALSE;
      }
      gdb->data = gdb->addr + pagefudge;
      memcpy(&Gdb[count * OV_BLOCKSIZE], gdb->data, OV_BLOCKSIZE);
      munmap(gdb->addr, gdb->len);
      gdb->data = &Gdb[count * OV_BLOCKSIZE];
      count++;
    }
  }
  return TRUE;
}

STATIC void *ovopensearch(char *group, int low, int high, BOOL needov) {
  GROUPLOC		gloc;
  GROUPENTRY		*ge;
  OVSEARCH		*search;

  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc))
    return NULL;

  ge = &GROUPentries[gloc.recno];
  if (low < ge->low)
    low = ge->low;
  if (high > ge->high)
    high = ge->high;

  if (!ovgroupmmap(ge, low, high, needov)) {
    return NULL;
  }

  search = NEW(OVSEARCH, 1);
  search->hi = high;
  search->lo = low;
  search->cur = 0;
  search->group = COPY(group);
  search->needov = needov;
  search->gloc = gloc;
  return (void *)search;
}

void *buffindexed_opensearch(char *group, int low, int high) {
  GROUPLOC		gloc;
  void			*handle;

  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    return NULL;
  }
  GROUPlock(gloc, LOCK_WRITE);
  if ((handle = ovopensearch(group, low, high, TRUE)) == NULL)
    GROUPlock(gloc, LOCK_UNLOCK);
  return(handle);
}

BOOL ovsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived, time_t *expires) {
  OVSEARCH		*search = (OVSEARCH *)handle;
  OVBLOCK		*ovblock;
  OV			srchov;
  GROUPDATABLOCK	*gdb;

  if (search->cur == Gibcount) {
    return FALSE;
  }
  while (Gib[search->cur].artnum == 0) {
    search->cur++;
    if (search->cur == Gibcount)
      return FALSE;
  }

  if (search->needov) {
    if (Gib[search->cur].index == NULLINDEX) {
      if (len)
	*len = 0;
      if (artnum)
	*artnum = Gib[search->cur].artnum;
    } else {
      if (artnum)
	*artnum = Gib[search->cur].artnum;
      if (len)
	*len = Gib[search->cur].len;
      if (arrived)
	*arrived = Gib[search->cur].arrived;
      if (expires)
	*expires = Gib[search->cur].expires;
      if (data) {
	srchov.index = Gib[search->cur].index;
	srchov.blocknum = Gib[search->cur].blocknum;
	gdb = searchgdb(&srchov);
	if (gdb == NULL) {
	  if (len)
	    *len = 0;
	  search->cur++;
	  return TRUE;
	}
	*data = gdb->data + Gib[search->cur].offset;
      }
    }
  }
  if (token) {
    if (Gib[search->cur].index == NULLINDEX && !search->needov) {
      search->cur++;
      return FALSE;
    }
    *token = Gib[search->cur].token;
  }
  search->cur++;
  return TRUE;
}

BOOL buffindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived) {
  return(ovsearch(handle, artnum, data, len, token, arrived, NULL));
}

STATIC void ovclosesearch(void *handle, BOOL freeblock) {
  OVSEARCH	*search = (OVSEARCH *)handle;
#ifdef OV_DEBUG
  GROUPENTRY	*ge;
  GROUPLOC	gloc;
#endif /* OV_DEBUG */

  if (freeblock) {
#ifdef OV_DEBUG
    gloc = GROUPfind(search->group, FALSE);
    if (!GROUPLOCempty(gloc)) {
      ge = &GROUPentries[gloc.recno];
      freegroupblock(ge);
    }
#else
    freegroupblock();
#endif /* OV_DEBUG */
  }
  ovgroupunmap();
  DISPOSE(search->group);
  DISPOSE(search);
  return;
}

void buffindexed_closesearch(void *handle) {
  OVSEARCH	*search = (OVSEARCH *)handle;
  GROUPLOC	gloc;

  gloc = search->gloc;
  ovclosesearch(handle, FALSE);
  GROUPlock(gloc, LOCK_UNLOCK);
}

BOOL buffindexed_getartinfo(char *group, ARTNUM artnum, char **data, int *len, TOKEN *token) {
  GROUPLOC	gloc;
  void		*handle;
  BOOL		retval;

  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    return FALSE;
  }
  GROUPlock(gloc, LOCK_WRITE);
  if (!(handle = ovopensearch(group, artnum, artnum, FALSE))) {
    GROUPlock(gloc, LOCK_UNLOCK);
    return FALSE;
  }
  retval = buffindexed_search(handle, NULL, data, len, token, NULL);
  ovclosesearch(handle, FALSE);
  GROUPlock(gloc, LOCK_UNLOCK);
  return retval;
}

BOOL buffindexed_expiregroup(char *group, int *lo) {
  void		*handle;
  GROUPENTRY	newge, *ge;
  GROUPLOC	gloc, next;
  char		*data;
  int		i, j, len;
  TOKEN		token;
  ARTNUM	artnum, low, high;
  ARTHANDLE	*ah;
  char		flag;
  HASH		hash;
  time_t	arrived, expires;

  if (group == NULL) {
    for (i = 0 ; i < GROUPheader->freelist.recno ; i++) {
      gloc.recno = i;
      GROUPlock(gloc, LOCK_WRITE);
      ge = &GROUPentries[gloc.recno];
      if (ge->expired >= OVrealnow || ge->count == 0) {
	GROUPlock(gloc, LOCK_UNLOCK);
	continue;
      }
      if (!ovgroupmmap(ge, ge->low, ge->high, TRUE)) {
	GROUPlock(gloc, LOCK_UNLOCK);
	syslog(L_ERROR, "%s: could not mmap overview for hidden groups(%d)", LocalLogName, i);
	continue;
      }
      for (j = 0 ; j < Gibcount ; j++) {
	if (Gib[j].artnum == 0)
	  continue;
	/* this may be duplicated, but ignore it in this case */
	OVEXPremove(Gib[j].token, TRUE, NULL, 0);
      }
#ifdef OV_DEBUG
      freegroupblock(ge);
#else
      freegroupblock();
#endif
      ovgroupunmap();
      ge->expired = time(NULL);
      ge->count = 0;
      GROUPlock(gloc, LOCK_UNLOCK);
    }
    return TRUE;
  }
  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    return FALSE;
  }
  GROUPlock(gloc, LOCK_WRITE);
  ge = &GROUPentries[gloc.recno];
  if (ge->count == 0) {
    if (lo)
      *lo = ge->low;
    ge->expired = time(NULL);
    GROUPlock(gloc, LOCK_UNLOCK);
    return TRUE;
  }
  flag = ge->flag;
  hash = ge->hash;
  next = ge->next;
  low = ge->low;
  high = ge->high;

  newge.low = 0;
  setinitialge(&newge, hash, &flag, next, 0, high);
  if ((handle = ovopensearch(group, low, high, TRUE)) == NULL) {
    ge->expired = time(NULL);
    GROUPlock(gloc, LOCK_UNLOCK);
    syslog(L_ERROR, "%s: could not open overview for '%s'", LocalLogName, group);
    return FALSE;
  }
  while (ovsearch(handle, &artnum, &data, &len, &token, &arrived, &expires)) {
    ah = NULL;
    if (len == 0)
      continue; 
    if (!SMprobe(EXPENSIVESTAT, &token, NULL) || OVstatall) {
      if ((ah = SMretrieve(token, RETR_STAT)) == NULL)
        continue; 
      SMfreearticle(ah);
    } else {
      if (!OVhisthasmsgid(data))
	continue; 
    }
    if (innconf->groupbaseexpiry && OVgroupbasedexpire(token, group, data, len, arrived, expires))
      continue;
#ifdef OV_DEBUG
    if (!ovaddrec(&newge, artnum, token, data, len, arrived, expires, ge)) {
#else
    if (!ovaddrec(&newge, artnum, token, data, len, arrived, expires)) {
#endif /* OV_DEBUG */
      ovclosesearch(handle, TRUE);
      ge->expired = time(NULL);
      GROUPlock(gloc, LOCK_UNLOCK);
      syslog(L_ERROR, "%s: could not add new overview for '%s'", LocalLogName, group);
      return FALSE;
    }
  }
  if (newge.low == 0)
    /* no article for the group */
    newge.low = newge.high;
  *ge = newge;
  if (lo) {
    if (ge->count == 0)
      /* lomark should be himark + 1, if no article for the group */
      *lo = ge->low + 1;
    else
      *lo = ge->low;
  }
  ovclosesearch(handle, TRUE);
  ge->expired = time(NULL);
  GROUPlock(gloc, LOCK_UNLOCK);
  return TRUE;
}

BOOL buffindexed_ctl(OVCTLTYPE type, void *val) {
  int		total, used, *i;
  OVBUFF	*ovbuff = ovbufftab;
  OVSORTTYPE	*sorttype;

  switch (type) {
  case OVSPACE:
    for (total = used = 0 ; ovbuff != (OVBUFF *)NULL ; ovbuff = ovbuff->next) {
      ovlock(ovbuff, LOCK_READ);
      ovreadhead(ovbuff);
      total += ovbuff->totalblk;
      used += ovbuff->usedblk;
      ovlock(ovbuff, LOCK_UNLOCK);
    }
    i = (int *)val;
    *i = (used * 100) / total;
    return TRUE;
  case OVSORT:
    sorttype = (OVSORTTYPE *)val;
    *sorttype = OVNOSORT;
    return TRUE;
  case OVCUTOFFLOW:
    Cutofflow = *(BOOL *)val;
    return TRUE;
  case OVSTATICSEARCH:
    i = (int *)val;
    *i = FALSE;
    return TRUE;
  default:
    return FALSE;
  }
}

void buffindexed_close(void) {
  struct stat	sb;
  OVBUFF	*ovbuffnext, *ovbuff = ovbufftab;
#ifdef OV_DEBUG
  FILE		*F = NULL;
  PID_T		pid;
  char		*path = NULL;
  int		i,j;
  struct ov_trace_array *trace;
  struct ov_name_table	*ntp;
#endif /* OV_DEBUG */

#ifdef OV_DEBUG
  for (; ovbuff != (OVBUFF *)NULL; ovbuff = ovbuff->next) {
    for (i = 0 ; i < ovbuff->totalblk ; i++) {
      trace = &ovbuff->trace[i];
      if (trace->ov_trace == NULL)
	continue;
      for (j = 0 ; j <= trace->cur && j < trace->max ; j++) {
	if (trace->ov_trace[j].occupied != 0 ||
	  trace->ov_trace[j].freed != 0) {
	  if (F == NULL) {
	    path = NEW(char, strlen(innconf->pathtmp) + 10);
	    pid = getpid();
	    sprintf(path, "%s/%d", innconf->pathtmp, pid);
	    if ((F = fopen(path, "w")) == NULL) {
	      syslog(L_ERROR, "%s: could not open %s: %m", LocalLogName, path);
	      break;
	    }
	  }
	  fprintf(F, "%d: % 6d, % 2d: 0x%08x, % 10d, % 10d\n", ovbuff->index, i, j,
	  trace->ov_trace[j].gloc.recno,
	  trace->ov_trace[j].occupied,
	  trace->ov_trace[j].freed);
	}
      }
    }
  }
  if ((ntp = name_table) != NULL) {
    if (F == NULL) {
      path = NEW(char, strlen(innconf->pathtmp) + 10);
      pid = getpid();
      sprintf(path, "%s/%d", innconf->pathtmp, pid);
      if ((F = fopen(path, "w")) == NULL) {
        syslog(L_ERROR, "%s: could not open %s: %m", LocalLogName, path);
      }
    }
    if (F != NULL) {
      while(ntp) {
        fprintf(F, "0x%08x: %s\n", ntp->recno, ntp->name);
        ntp = ntp->next;
      }
    }
  }
  if (F != NULL)
    fclose(F);
  if (path != NULL)
    DISPOSE(path);
#endif /* OV_DEBUG */
  if (fstat(GROUPfd, &sb) < 0)
    return;
  close(GROUPfd);

  if (GROUPheader) {
    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
      syslog(L_FATAL, "%s: could not munmap group.index in buffindexed_close: %m", LocalLogName);
      return;
    }
  }
  for (; ovbuff != (OVBUFF *)NULL; ovbuff = ovbuffnext) {
    ovbuffnext = ovbuff->next;
    DISPOSE(ovbuff);
  }
  ovbufftab = NULL;
}

#ifdef DEBUG
STATIC int countgdb(void) {
  int			i, count = 0;
  GROUPDATABLOCK	*gdb;

  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdb->next)
      count++;
  }
  return count;
}

main(int argc, char **argv) {
  char			*group, flag[2], buff[OV_BLOCKSIZE];
  int			lo, hi, count, flags, i;
  OVSEARCH		*search;
  OVBLOCK		*ovblock;
  OVINDEX		*ovindex;
  OVBUFF		*ovbuff;
  GROUPENTRY		*ge;
  GROUPLOC		gloc;
  GIBLIST		*giblist;

  if (argc != 2) {
    fprintf(stderr, "only one argument can be specified\n");
    exit(1);
  }
  /* if innconf isn't already read in, do so. */
  if (innconf == NULL) {
    if (ReadInnConf() < 0) {
      fprintf(stderr, "reading inn.conf failed\n");
      exit(1);
    }
  }
  if (!buffindexed_open(OV_READ)) {
    fprintf(stderr, "buffindexed_open failed\n");
    exit(1);
  }
  fprintf(stdout, "GROUPheader->freelist.recno is %d(0x%08x)\n", GROUPheader->freelist.recno, GROUPheader->freelist.recno);
  group = argv[1];
  if (isdigit(*group)) {
    gloc.recno = atoi(group);
    ge = &GROUPentries[gloc.recno];
    fprintf(stdout, "left articles are %d for %d, last expiry is %d\n", ge->count, gloc.recno, ge->expired);
    if (ge->count == 0) {
      GROUPlock(gloc, LOCK_UNLOCK);
      exit(0);
    }
    if (!ovgroupmmap(ge, ge->low, ge->high, TRUE)) {
      fprintf(stderr, "ovgroupmmap failed\n");
      GROUPlock(gloc, LOCK_UNLOCK);
    }
    for (giblist = Giblist, i = 0 ; giblist != NULL ; giblist = giblist->next, i++);
    fprintf(stdout, "%d index block(s)\n", i);
    fprintf(stdout, "%d data block(s)\n", countgdb());
    for (giblist = Giblist ; giblist != NULL ; giblist = giblist->next) {
      fprintf(stdout, "  % 8d(% 5d)\n", giblist->ov.blocknum, giblist->ov.index);
    }
    for (i = 0 ; i < Gibcount ; i++) {
      if (Gib[i].artnum == 0)
	continue;
      if (Gib[i].index == NULLINDEX)
	fprintf(stdout, "    %d empty\n");
      else {
	fprintf(stdout, "    %d %d\n", Gib[i].offset, Gib[i].len);
      }
    }
    ovgroupunmap();
    GROUPlock(gloc, LOCK_UNLOCK);
    exit(0);
  }
  gloc = GROUPfind(group, FALSE);
  if (GROUPLOCempty(gloc)) {
    fprintf(stderr, "gloc is null\n");
  }
  GROUPlock(gloc, LOCK_READ);
  ge = &GROUPentries[gloc.recno];
  fprintf(stdout, "base %d(%d), cur %d(%d), expired at %s\n", ge->baseindex.blocknum, ge->baseindex.index, ge->curindex.blocknum, ge->curindex.index, ge->expired == 0 ? "none\n" : ctime(&ge->expired));
  if (!buffindexed_groupstats(group, &lo, &hi, &count, &flags)) {
    fprintf(stderr, "buffindexed_groupstats failed for group %s\n", group);
    exit(1);
  }
  flag[0] = (char)flags;
  flag[1] = '\0';
  fprintf(stdout, "%s: low is %d, high is %d, count is %d, flag is '%s'\n", group, lo, hi, count, flag);
  if ((search = (OVSEARCH *)ovopensearch(group, lo, hi, TRUE)) == NULL) {
    fprintf(stderr, "ovopensearch failed for group %s\n", group);
    exit(1);
  }
  fprintf(stdout, "  gloc is %d(0x%08x)\n", search->gloc.recno, search->gloc.recno);
  for (giblist = Giblist, i = 0 ; giblist != NULL ; giblist = giblist->next, i++);
  fprintf(stdout, "%d index block(s)\n", i);
  fprintf(stdout, "%d data block(s)\n", countgdb());
  for (giblist = Giblist ; giblist != NULL ; giblist = giblist->next) {
    fprintf(stdout, "  % 8d(% 5d)\n", giblist->ov.blocknum, giblist->ov.index);
  }
  for (i = 0 ; i < Gibcount ; i++) {
    if (Gib[i].artnum == 0)
      continue;
    if (Gib[i].index == NULLINDEX)
      fprintf(stdout, "    %d empty\n");
    else {
      fprintf(stdout, "    %d %d\n", Gib[i].offset, Gib[i].len);
    }
  }
  {
    ARTNUM artnum;
    char *data;
    int len;
    TOKEN token;
    while (buffindexed_search((void *)search, &artnum, &data, &len, &token, NULL)) {
      if (len == 0)
	fprintf(stdout, "%d: len is 0\n", artnum);
      else {
	memcpy(buff, data, len);
	buff[len] = '\0';
	fprintf(stdout, "%d: %s\n", artnum, buff);
      }
    }
  }
}
#endif /* DEBUG */
