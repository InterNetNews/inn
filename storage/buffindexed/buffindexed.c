/*  $Id$
**
**  Overview buffer and index method.
*/

/*
** Buffindexed using shared memory on ovbuff by Sang-yong Suh
**
** During the recent discussions in inn-workers, Alex Kiernan found
** that INN LockRange() is not working for MMAPed file.  This explains
** why buffindexed has long outstanding bugs such as "could not MMAP...".
**
** This version corrects the file locking error by using shared memory.
** The bitfield of each buffer file is loaded into memory, and is shared
** by all programs such as innd, expireover, makehistory, and overchan.
** The locking problem is handled by semaphore.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#include <syslog.h>
#include <sys/stat.h>
#include <time.h>

#include "inn/innconf.h"
#include "libinn.h"
#include "ov.h"
#include "paths.h"
#include "ovinterface.h"
#include "storage.h"

/* Yes. I know that it violates INN coding style. However, this allows
 * me to compile this new version without reconfiguring INN.
 * If all goes well, shmem.c should go to $INN/lib, and shmem.h should
 * go to $INN/include.
 */
#include "shmem.h"

#include "buffindexed.h"

#define	OVBUFF_MAGIC	"ovbuff"
#define OVBUFF_VERSION	2

/*
** Because ovbuff bitfields are residing in memory, we don't have to
** do file write for each update.  Instead we'll do it at every
** OVBUFF_SYNC_COUNT updates.
*/
#define OVBUFF_SYNC_COUNT	(innconf->icdsynccount * 10 + 1)
/* #define OVBUFF_SYNC_COUNT	1 */

/* ovbuff header */
#define	OVBUFFMASIZ	8
#define	OVBUFFNASIZ	16
#define	OVBUFFLASIZ	16
#define	OVBUFFPASIZ	64

#define	OVMAXCYCBUFFNAME	8

#define OV_HDR_PAGESIZE 16384
#define OV_BEFOREBITF   (1 * OV_BLOCKSIZE)
#define	OV_BLOCKSIZE	8192
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
  /*
   * The following parts will be synced to the bitfield
   */
  int          version;         /* magic version number */
  unsigned int freeblk;         /* next free block number */
  unsigned int usedblk;         /* number of used blocks */
} OVBUFFHEAD;

/* ovbuff info */
typedef struct _OVBUFF {
  unsigned int		index;			/* ovbuff index */
  char			path[OVBUFFPASIZ];	/* Path to file */
  int			fd;			/* file descriptor for this
						   ovbuff */
  off_t 		len;			/* Length of writable area, in
						   bytes */
  off_t 		base;			/* Offset (relative to byte
						   0 of file) to base block */
  unsigned int		freeblk;		/* next free block number no
						   freeblk left if equals
						   totalblk */
  unsigned int		totalblk;		/* number of total blocks */
  unsigned int		usedblk;		/* number of used blocks */
  time_t		updated;		/* Time of last update to
						   header */
  void *		bitfield;		/* Bitfield for ovbuff block in
						   use */
  int			dirty;			/* OVBUFFHEAD dirty count */
  struct _OVBUFF	*next;			/* next ovbuff */
  int			nextchunk;		/* next chunk */
  smcd_t		*smc;			/* shared mem control data */
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
  off_t         offset;		/* offset from the top in the block */
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
  void *	addr;
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

static struct ov_name_table *name_table = NULL;
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
  HASH		hash;		/* MD5 hash of the group name */
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
  off_t         curoffset;	/* current offset for this ovbuff */
} GROUPENTRY;

typedef struct _GIBLIST {
  OV			ov;
  struct _GIBLIST	*next;
} GIBLIST;

typedef struct _GDB {
  OV		datablk;
  void *	addr;
  void *	data;
  int		len;
  bool		mmapped;
  struct _GDB	*next;
} GROUPDATABLOCK;

typedef struct {
  char			*group;
  ARTNUM		lo;
  ARTNUM		hi;
  int			cur;
  bool			needov;
  GROUPLOC		gloc;
  int			count;
  GROUPDATABLOCK	gdb;	/* used for caching current block */
} OVSEARCH;

#define GROUPDATAHASHSIZE	25

static GROUPDATABLOCK	*groupdatablock[GROUPDATAHASHSIZE];

typedef enum {PREPEND_BLK, APPEND_BLK} ADDINDEX;
typedef enum {SRCH_FRWD, SRCH_BKWD} SRCH;

#define	_PATH_OVBUFFCONFIG	"buffindexed.conf"

static char LocalLogName[] = "buffindexed";
static long		pagesize = 0;
static OVBUFF		*ovbufftab = NULL;
static OVBUFF           *ovbuffnext = NULL;
static int              GROUPfd;
static GROUPHEADER      *GROUPheader = NULL;
static GROUPENTRY       *GROUPentries = NULL;
static int              GROUPcount = 0;
static GROUPLOC         GROUPemptyloc = { -1 };
#define	NULLINDEX	(-1)
static OV 	        ovnull = { 0, NULLINDEX };
typedef unsigned long	ULONG;
static ULONG		onarray[64], offarray[64];
static int		longsize = sizeof(long);
static bool		Nospace;
static bool		Needunlink;
static bool		Cutofflow;
static bool		Cache;
static OVSEARCH		*Cachesearch;

static int ovbuffmode;

static GROUPLOC GROUPnewnode(void);
static bool GROUPremapifneeded(GROUPLOC loc);
static void GROUPLOCclear(GROUPLOC *loc);
static bool GROUPLOCempty(GROUPLOC loc);
static bool GROUPlockhash(enum inn_locktype type);
static bool GROUPlock(GROUPLOC gloc, enum inn_locktype type);
static off_t GROUPfilesize(int count);
static bool GROUPexpand(int mode);
static void *ovopensearch(char *group, ARTNUM low, ARTNUM high, bool needov);
static void ovclosesearch(void *handle, bool freeblock);
static OVINDEX	*Gib;
static GIBLIST	*Giblist;
static int	Gibcount;

#ifdef MMAP_MISSES_WRITES
#define PWRITE(fd, buf, nbyte, offset)  mmapwrite(fd, buf, nbyte, offset)
#else
#define PWRITE(fd, buf, nbyte, offset)  pwrite(fd, buf, nbyte, offset)
#endif

#ifdef MMAP_MISSES_WRITES
/* With HP/UX, you definitely do not want to mix mmap-accesses of
   a file with read()s and write()s of the same file */
static off_t mmapwrite(int fd, void *buf, off_t nbyte, off_t offset) {
  int		pagefudge, len;
  off_t         mmapoffset;
  void *	addr;

  pagefudge = offset % pagesize;
  mmapoffset = offset - pagefudge;
  len = pagefudge + nbyte;

  if ((addr = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, mmapoffset)) == MAP_FAILED) {
    return -1;
  }
  memcpy(addr+pagefudge, buf, nbyte);
  munmap(addr, len);
  return nbyte;
}
#endif /* MMAP_MISSES_WRITES */

static bool ovparse_part_line(char *l) {
  char		*p;
  struct stat	sb;
  off_t         len, base;
  int		tonextblock;
  OVBUFF	*ovbuff, *tmp = ovbufftab;

  /* ovbuff partition name */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > OVMAXCYCBUFFNAME - 1) {
    syslog(L_ERROR, "%s: bad index in line '%s'", LocalLogName, l);
    return false;
  }
  *p = '\0';
  ovbuff = xmalloc(sizeof(OVBUFF));
  ovbuff->index = strtoul(l, NULL, 10);
  for (; tmp != (OVBUFF *)NULL; tmp = tmp->next) {
    if (tmp->index == ovbuff->index) {
      syslog(L_ERROR, "%s: dupulicate index in line '%s'", LocalLogName, l);
      free(ovbuff);
      return false;
    }
  }
  l = ++p;

  /* Path to ovbuff partition */
  if ((p = strchr(l, ':')) == NULL || p - l <= 0 || p - l > OVBUFFPASIZ - 1) {
    syslog(L_ERROR, "%s: bad pathname in line '%s'", LocalLogName, l);
    free(ovbuff);
    return false;
  }
  *p = '\0';
  memset(ovbuff->path, '\0', OVBUFFPASIZ);
  strlcpy(ovbuff->path, l, OVBUFFPASIZ);
  if (stat(ovbuff->path, &sb) < 0) {
    syslog(L_ERROR, "%s: file '%s' does not exist, ignoring '%d'",
	   LocalLogName, ovbuff->path, ovbuff->index);
    free(ovbuff);
    return false;
  }
  l = ++p;

  /* Length/size of symbolic partition */
  len = strtoul(l, NULL, 10) * 1024;     /* This value in KB in decimal */
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
      syslog(L_NOTICE, "%s: length mismatch '%lu' for index '%d' (%lu bytes)",
	LocalLogName, (unsigned long) len, ovbuff->index,
	(unsigned long) sb.st_size);
    if (ovbuff->base > sb.st_size)
      syslog(L_NOTICE, "%s: length must be at least '%lu' for index '%d' (%lu bytes)", 
	LocalLogName, (unsigned long) ovbuff->base, ovbuff->index,
	(unsigned long) sb.st_size);
    free(ovbuff);
    return false;
  }
  ovbuff->len = len;
  ovbuff->fd = -1;
  ovbuff->next = (OVBUFF *)NULL;
  ovbuff->dirty = 0;
  ovbuff->bitfield = NULL;
  ovbuff->nextchunk = 1;

  if (ovbufftab == (OVBUFF *)NULL)
    ovbufftab = ovbuff;
  else {
    for (tmp = ovbufftab; tmp->next != (OVBUFF *)NULL; tmp = tmp->next);
    tmp->next = ovbuff;
  }
  return true;
}

/*
** ovbuffread_config() -- Read the overview partition/file configuration file.
*/

static bool ovbuffread_config(void) {
  char		*path, *config, *from, *to, **ctab = (char **)NULL;
  int		ctab_free = 0;  /* Index to next free slot in ctab */
  int		ctab_i;

  path = concatpath(innconf->pathetc, _PATH_OVBUFFCONFIG);
  config = ReadInFile(path, NULL);
  if (config == NULL) {
    syslog(L_ERROR, "%s: cannot read %s", LocalLogName, path);
    free(config);
    free(path);
    return false;
  }
  free(path);
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
      ctab = xmalloc(sizeof(char *));
    else
      ctab = xrealloc(ctab, (ctab_free + 1) * sizeof(char *));
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
      free(config);
      free(ctab);
      return false;
    }
  }
  free(config);
  free(ctab);
  if (ovbufftab == (OVBUFF *)NULL) {
    syslog(L_ERROR, "%s: no buffindexed defined", LocalLogName);
    return false;
  }
  return true;
}

static char hextbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'a', 'b', 'c', 'd', 'e', 'f'};

static char *offt2hex(off_t offset, bool leadingzeros) {
  static char	buf[24];
  char	*p;

  if (sizeof(off_t) <= sizeof(unsigned long)) {
    snprintf(buf, sizeof(buf), (leadingzeros) ? "%016lx" : "%lx",
	     (unsigned long) offset);
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

static off_t hex2offt(char *hex) {
  if (sizeof(off_t) <= 4) {
    unsigned long rpofft;

    sscanf(hex, "%lx", &rpofft);
    return rpofft;
  } else {
    char		diff;
    off_t	n = 0;

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

static void ovreadhead(OVBUFF *ovbuff) {
  OVBUFFHEAD *x = (OVBUFFHEAD *)ovbuff->bitfield;
  ovbuff->freeblk = x->freeblk;
  ovbuff->usedblk = x->usedblk;
  return;
}

static void ovflushhead(OVBUFF *ovbuff) {
  OVBUFFHEAD	rpx;

  /* skip time consuming data conversion and write call */
  if (ovbuff->dirty < OVBUFF_SYNC_COUNT) {
    OVBUFFHEAD *x = (OVBUFFHEAD *)ovbuff->bitfield;
    x->freeblk = ovbuff->freeblk;
    x->usedblk = ovbuff->usedblk;
    return;
  }
 
  memset(&rpx, 0, sizeof(OVBUFFHEAD));
  ovbuff->updated = time(NULL);
  strncpy(rpx.magic, OVBUFF_MAGIC, strlen(OVBUFF_MAGIC));
  strncpy(rpx.path, ovbuff->path, OVBUFFPASIZ);
  /* Don't use sprintf() directly ... the terminating '\0' causes grief */
  strncpy(rpx.indexa, offt2hex(ovbuff->index, true), OVBUFFLASIZ);
  strncpy(rpx.lena, offt2hex(ovbuff->len, true), OVBUFFLASIZ);
  strncpy(rpx.totala, offt2hex(ovbuff->totalblk, true), OVBUFFLASIZ);
  strncpy(rpx.useda, offt2hex(ovbuff->usedblk, true), OVBUFFLASIZ);
  strncpy(rpx.freea, offt2hex(ovbuff->freeblk, true), OVBUFFLASIZ);
  strncpy(rpx.updateda, offt2hex(ovbuff->updated, true), OVBUFFLASIZ);
  rpx.version = OVBUFF_VERSION;
  rpx.freeblk = ovbuff->freeblk;
  rpx.usedblk = ovbuff->usedblk;
  memcpy(ovbuff->bitfield, &rpx, sizeof(OVBUFFHEAD));

  if (pwrite(ovbuff->fd, ovbuff->bitfield, ovbuff->base, 0) != ovbuff->base)
    syslog(L_ERROR, "%s: ovflushhead: cant flush on %s: %m", LocalLogName,
      ovbuff->path);
  ovbuff->dirty = 0;
  return;
}

static bool ovlock(OVBUFF *ovbuff, enum inn_locktype type) {
  int		ret;
  smcd_t	*smc = ovbuff->smc;
 
  if (type == INN_LOCK_WRITE) {
    ret = smcGetExclusiveLock(smc);
    smc->locktype = (int)INN_LOCK_WRITE;
  } else if (type == INN_LOCK_READ) {
    ret = smcGetSharedLock(smc);
    smc->locktype = (int)INN_LOCK_READ;
  } else if (smc->locktype == (int)INN_LOCK_WRITE) {
    ret = smcReleaseExclusiveLock(smc);
  } else {
    ret = smcReleaseSharedLock(smc);
  }
  return (ret == 0);
}

static bool ovbuffinit_disks(void) {
  OVBUFF	*ovbuff = ovbufftab;
  char		buf[64];
  OVBUFFHEAD	*rpx, dpx;
  int		fd;
  unsigned int	i;
  off_t		tmpo;
  smcd_t	*smc;
  static bool	atexit_registered = false;

  /*
   * Register the exit callback to sync the bitfield to the disk
   */
  if (!atexit_registered) {
    atexit(buffindexed_close);
    atexit_registered = true;
  }

  /*
  ** Discover the state of our ovbuffs.  If any of them are in icky shape,
  ** duck shamelessly & return false.
  */
  for (; ovbuff != (OVBUFF *)NULL; ovbuff = ovbuff->next) {
    if (ovbuff->fd < 0) {
      if ((fd = open(ovbuff->path, ovbuffmode & OV_WRITE ? O_RDWR : O_RDONLY)) < 0) {
	syslog(L_ERROR, "%s: ERROR opening '%s' : %m", LocalLogName, ovbuff->path);
	return false;
      } else {
	close_on_exec(fd, true);
	ovbuff->fd = fd;
      }
    }

    /* get shared memory buffer */
    smc = smcGetShmemBuffer(ovbuff->path, ovbuff->base);
    if (!smc) {
      /* No shared memory exists, create one. */
      smc = smcCreateShmemBuffer(ovbuff->path, ovbuff->base);
      if (!smc) {
	syslog(L_ERROR, "%s: ovinitdisks: cant create shmem for %s len %lu: %m",
	  LocalLogName, ovbuff->path, (unsigned long) ovbuff->base);
	return false;
      }
    }

    ovbuff->smc = smc;
    ovbuff->bitfield = smc->addr;
    rpx = (OVBUFFHEAD *)ovbuff->bitfield;

    /* lock the buffer */
    ovlock(ovbuff, ovbuffmode & OV_WRITE ? INN_LOCK_WRITE : INN_LOCK_READ);

    if (pread(ovbuff->fd, &dpx, sizeof(OVBUFFHEAD), 0) < 0) {
      syslog(L_ERROR, "%s: cant read from %s, %m", LocalLogName, ovbuff->path);
      ovlock(ovbuff, INN_LOCK_UNLOCK);
      return false;
    }

    /*
     * check validity of the disk data
     */
    if (strncmp(dpx.magic, OVBUFF_MAGIC, strlen(OVBUFF_MAGIC)) == 0 &&
      strncmp(dpx.path, ovbuff->path, OVBUFFPASIZ) == 0 ) {
      strncpy(buf, dpx.indexa, OVBUFFLASIZ);
      buf[OVBUFFLASIZ] = '\0';
      i = hex2offt(buf);
      if (i != ovbuff->index) {
	syslog(L_ERROR, "%s: Mismatch: index '%d' for buffindexed %s",
	  LocalLogName, i, ovbuff->path);
	ovlock(ovbuff, INN_LOCK_UNLOCK);
	return false;
      }
      strncpy(buf, dpx.lena, OVBUFFLASIZ);
      buf[OVBUFFLASIZ] = '\0';
      tmpo = hex2offt(buf);
      if (tmpo != ovbuff->len) {
	syslog(L_ERROR, "%s: Mismatch: read 0x%s length for buffindexed %s",
	  LocalLogName, offt2hex(tmpo, false), ovbuff->path);
	ovlock(ovbuff, INN_LOCK_UNLOCK);
	return false;
      }

      /*
       * compare shared memory with disk data.
       */
      if (strncmp(dpx.magic, rpx->magic, strlen(OVBUFF_MAGIC)) != 0 ||
	strncmp(dpx.path, rpx->path, OVBUFFPASIZ) != 0 ||
	strncmp(dpx.indexa, rpx->indexa, OVBUFFLASIZ) != 0 ||
	strncmp(dpx.lena, rpx->lena, OVBUFFLASIZ) != 0 ) {
	/*
	 * Load shared memory with disk data.
	 */
	if (pread(ovbuff->fd, rpx, ovbuff->base, 0) < 0) {
	  syslog(L_ERROR, "%s: cant read from %s, %m",
	    LocalLogName, ovbuff->path);
	  ovlock(ovbuff, INN_LOCK_UNLOCK);
	  return false;
	}
      }
      strncpy(buf, dpx.totala, OVBUFFLASIZ);
      buf[OVBUFFLASIZ] = '\0';
      ovbuff->totalblk = hex2offt(buf);

      if (rpx->version == 0) {
	/* no binary data available. use character data */
	strncpy(buf, rpx->useda, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	ovbuff->usedblk = hex2offt(buf);
	strncpy(buf, rpx->freea, OVBUFFLASIZ);
	buf[OVBUFFLASIZ] = '\0';
	ovbuff->freeblk = hex2offt(buf);
      } else {
	/* use binary data. The first reason is the speed.
	   and the second reason is the other partner is not
	   synced.
	 */
	ovbuff->usedblk = rpx->usedblk;
	ovbuff->freeblk = rpx->freeblk;
      }
      Needunlink = false;
    } else {
      /*
       * Initialize the contents of the shared memory
       */
      memset(rpx, 0, ovbuff->base);

      ovbuff->totalblk = (ovbuff->len - ovbuff->base)/OV_BLOCKSIZE;
      if (ovbuff->totalblk < 1) {
	syslog(L_ERROR, "%s: too small length '%lu' for buffindexed %s",
	    LocalLogName, (unsigned long) ovbuff->len, ovbuff->path);
	ovlock(ovbuff, INN_LOCK_UNLOCK);
	return false;
      }
      ovbuff->usedblk = 0;
      ovbuff->freeblk = 0;
      ovbuff->updated = 0;
      ovbuff->dirty   = OVBUFF_SYNC_COUNT + 1;
      syslog(L_NOTICE,
	"%s: No magic cookie found for buffindexed %d, initializing",
	LocalLogName, ovbuff->index);
      ovflushhead(ovbuff);
    }
#ifdef OV_DEBUG
    ovbuff->trace = xcalloc(ovbuff->totalblk, sizeof(ov_trace_array));
#endif /* OV_DEBUG */
    ovlock(ovbuff, INN_LOCK_UNLOCK);
  }
  return true;
}

static int ovusedblock(OVBUFF *ovbuff, int blocknum, bool set_operation, bool setbitvalue) {
  off_t         longoffset;
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
  if (bitlong & mask) return 1; else return 0;
}

static void ovnextblock(OVBUFF *ovbuff) {
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

static OVBUFF *getovbuff(OV ov) {
  OVBUFF	*ovbuff = ovbufftab;
  for (; ovbuff != NULL; ovbuff = ovbuff->next) {
    if (ovbuff->index == (unsigned int) ov.index)
      return ovbuff;
  }
  return NULL;
}

#ifdef OV_DEBUG
static OV ovblocknew(GROUPENTRY *ge) {
#else
static OV ovblocknew(void) {
#endif /* OV_DEBUG */
  OVBUFF	*ovbuff;
  OV		ov;
  bool          done = false;
#ifdef OV_DEBUG
  int		recno;
  struct ov_trace_array *trace;
#endif /* OV_DEBUG */

  if (ovbuffnext == NULL)
    ovbuffnext = ovbufftab;

  /*
   * We will try to recover broken overview possibly due to unsync.
   * The recovering is inactive for OV_DEBUG mode.
   */

retry:
  for (ovbuff = ovbuffnext ; ovbuff != (OVBUFF *)NULL ; ovbuff = ovbuff->next) {
    ovlock(ovbuff, INN_LOCK_WRITE);
    ovreadhead(ovbuff);
    if (ovbuff->totalblk != ovbuff->usedblk && ovbuff->freeblk == ovbuff->totalblk) {
      ovnextblock(ovbuff);
    }
    if (ovbuff->totalblk == ovbuff->usedblk || ovbuff->freeblk == ovbuff->totalblk) {
      /* no space left for this ovbuff */
      ovlock(ovbuff, INN_LOCK_UNLOCK);
      continue;
    }
    break;
  }
  if (ovbuff == NULL) {
    for (ovbuff = ovbufftab ; ovbuff != ovbuffnext ; ovbuff = ovbuff->next) {
      ovlock(ovbuff, INN_LOCK_WRITE);
      ovreadhead(ovbuff);
      if (ovbuff->totalblk == ovbuff->usedblk || ovbuff->freeblk == ovbuff->totalblk) {
	/* no space left for this ovbuff */
	ovlock(ovbuff, INN_LOCK_UNLOCK);
	continue;
      }
      break;
    }
    if (ovbuff == ovbuffnext) {
      Nospace = true;
      return ovnull;
    }
  }
#ifdef OV_DEBUG
  recno = ((char *)ge - (char *)&GROUPentries[0])/sizeof(GROUPENTRY);
  if (ovusedblock(ovbuff, ovbuff->freeblk, false, true)) {
    syslog(L_FATAL, "%s: 0x%08x trying to occupy new block(%d, %d), but already occupied", LocalLogName, recno, ovbuff->index, ovbuff->freeblk);
    buffindexed_close();
    abort();
  }
  trace = &ovbuff->trace[ovbuff->freeblk];
  if (trace->ov_trace == NULL) {
    trace->ov_trace = xcalloc(OV_TRACENUM, sizeof(struct ov_trace));
    trace->max = OV_TRACENUM;
  } else if (trace->cur + 1 == trace->max) {
    trace->max += OV_TRACENUM;
    trace->ov_trace = xrealloc(trace->ov_trace, trace->max * sizeof(struct ov_trace));
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
 
#ifndef OV_DEBUG
  if (ovusedblock(ovbuff, ovbuff->freeblk, false, true)) {
      syslog(L_NOTICE, "%s: fixing invalid free block(%d, %d).",
	     LocalLogName, ovbuff->index, ovbuff->freeblk);
  } else
      done = true;
#endif /* OV_DEBUG */

  /* mark it as allocated */
  ovusedblock(ovbuff, ov.blocknum, true, true);

  ovnextblock(ovbuff);
  ovbuff->usedblk++;
  ovbuff->dirty++;
  ovflushhead(ovbuff);
  ovlock(ovbuff, INN_LOCK_UNLOCK);
  ovbuffnext = ovbuff->next;
  if (ovbuffnext == NULL)
    ovbuffnext = ovbufftab;

  if (!done)
    goto retry;

  return ov;
}

#ifdef OV_DEBUG
static void ovblockfree(OV ov, GROUPENTRY *ge) {
#else
static void ovblockfree(OV ov) {
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
  ovlock(ovbuff, INN_LOCK_WRITE);
#ifdef OV_DEBUG
  recno = ((char *)ge - (char *)&GROUPentries[0])/sizeof(GROUPENTRY);
  if (!ovusedblock(ovbuff, ov.blocknum, false, false)) {
    syslog(L_FATAL, "%s: 0x%08x trying to free block(%d, %d), but already freed", LocalLogName, recno, ov.index, ov.blocknum);
    buffindexed_close();
    abort();
  }
  trace = &ovbuff->trace[ov.blocknum];
  if (trace->ov_trace == NULL) {
    trace->ov_trace = xcalloc(OV_TRACENUM, sizeof(struct ov_trace));
    trace->max = OV_TRACENUM;
  } else if (trace->cur + 1 == trace->max) {
    trace->max += OV_TRACENUM;
    trace->ov_trace = xrealloc(trace->ov_trace, trace->max * sizeof(struct ov_trace));
    memset(&trace->ov_trace[trace->cur], '\0', sizeof(struct ov_trace) * (trace->max - trace->cur));
  }
  if (trace->ov_trace[trace->cur].freed != 0) {
    trace->cur++;
  }
  trace->ov_trace[trace->cur].freed = time(NULL);
  trace->ov_trace[trace->cur].gloc.recno = recno;
  trace->cur++;
#endif /* OV_DEBUG */

#ifndef OV_DEBUG
  if (!ovusedblock(ovbuff, ov.blocknum, false, false)) {
    syslog(L_NOTICE, "%s: trying to free block(%d, %d), but already freed.",
	   LocalLogName, ov.index, ov.blocknum);
  }
#endif

  ovusedblock(ovbuff, ov.blocknum, true, false);
  ovreadhead(ovbuff);
  if (ovbuff->freeblk == ovbuff->totalblk)
    ovbuff->freeblk = ov.blocknum;
  ovbuff->usedblk--;
  ovbuff->dirty++;
  ovflushhead(ovbuff);
  ovlock(ovbuff, INN_LOCK_UNLOCK);
  return;
}

bool buffindexed_open(int mode) {
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
    pagesize = getpagesize();
    if (pagesize == -1) {
      syslog(L_ERROR, "%s: getpagesize failed: %m", LocalLogName);
      pagesize = 0;
      return false;
    }
    if ((pagesize > OV_HDR_PAGESIZE) || (OV_HDR_PAGESIZE % pagesize)) {
      syslog(L_ERROR, "%s: OV_HDR_PAGESIZE (%d) is not a multiple of pagesize (%ld)", LocalLogName, OV_HDR_PAGESIZE, pagesize);
      return false;
    }
  }
  memset(&groupdatablock, '\0', sizeof(groupdatablock));
  if (!ovbuffread_config()) {
    return false;
  }
  Needunlink = true;
  if (!ovbuffinit_disks()) {
    return false;
  }

  groupfn = concatpath(innconf->pathdb, "group.index");
  if (Needunlink && unlink(groupfn) == 0) {
    syslog(L_NOTICE, "%s: all buffers are brandnew, unlink '%s'", LocalLogName, groupfn);
  }
  GROUPfd = open(groupfn, ovbuffmode & OV_WRITE ? O_RDWR | O_CREAT : O_RDONLY, 0660);
  if (GROUPfd < 0) {
    syslog(L_FATAL, "%s: Could not create %s: %m", LocalLogName, groupfn);
    free(groupfn);
    return false;
  }

  if (fstat(GROUPfd, &sb) < 0) {
    syslog(L_FATAL, "%s: Could not fstat %s: %m", LocalLogName, groupfn);
    free(groupfn);
    close(GROUPfd);
    return false;
  }
  if (sb.st_size > (off_t) sizeof(GROUPHEADER)) {
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
    GROUPheader = mmap(0, GROUPfilesize(GROUPcount), flag, MAP_SHARED,
		       GROUPfd, 0);
    if (GROUPheader == MAP_FAILED) {
      syslog(L_FATAL, "%s: Could not mmap %s in buffindexed_open: %m", LocalLogName, groupfn);
      free(groupfn);
      close(GROUPfd);
      return false;
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
  } else {
    GROUPcount = 0;
    if (!GROUPexpand(mode)) {
      free(groupfn);
      close(GROUPfd);
      return false;
    }
  }
  close_on_exec(GROUPfd, true);

  free(groupfn);
  Cutofflow = false;

  return true;
}

static GROUPLOC GROUPfind(char *group, bool Ignoredeleted) {
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

bool buffindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag) {
  GROUPLOC	gloc;

  gloc = GROUPfind(group, false);
  if (GROUPLOCempty(gloc)) {
    return false;
  }
  GROUPlock(gloc, INN_LOCK_READ);
  if (lo != NULL)
    *lo = GROUPentries[gloc.recno].low;
  if (hi != NULL)
    *hi = GROUPentries[gloc.recno].high;
  if (count != NULL)
    *count = GROUPentries[gloc.recno].count;
  if (flag != NULL)
    *flag = GROUPentries[gloc.recno].flag;
  GROUPlock(gloc, INN_LOCK_UNLOCK);
  return true;
}

static void setinitialge(GROUPENTRY *ge, HASH grouphash, char *flag, GROUPLOC next, ARTNUM lo, ARTNUM hi) {
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

bool buffindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag) {
  unsigned int	i;
  HASH		grouphash;
  GROUPLOC	gloc;
  GROUPENTRY	*ge;
#ifdef OV_DEBUG
  struct ov_name_table	*ntp;
#endif /* OV_DEBUG */

  gloc = GROUPfind(group, true);
  if (!GROUPLOCempty(gloc)) {
    ge = &GROUPentries[gloc.recno];
    if (GROUPentries[gloc.recno].deleted != 0) {
      grouphash = Hash(group, strlen(group));
      setinitialge(ge, grouphash, flag, ge->next, lo, hi);
    } else {
      ge->flag = *flag;
    }
    return true;
  }
  grouphash = Hash(group, strlen(group));
  memcpy(&i, &grouphash, sizeof(i));
  i = i % GROUPHEADERHASHSIZE;
  GROUPlockhash(INN_LOCK_WRITE);
  gloc = GROUPnewnode();
  ge = &GROUPentries[gloc.recno];
  setinitialge(ge, grouphash, flag, GROUPheader->hash[i], lo, hi);
  GROUPheader->hash[i] = gloc;
#ifdef OV_DEBUG
  ntp = xmalloc(sizeof(struct ov_name_table));
  memset(ntp, '\0', sizeof(struct ov_name_table));
  ntp->name = xstrdup(group);
  ntp->recno = gloc.recno;
  if (name_table == NULL)
    name_table = ntp;
  else {
    ntp->next = name_table;
    name_table = ntp;
  }
#endif /* OV_DEBUG */
  GROUPlockhash(INN_LOCK_UNLOCK);
  return true;
}

static off_t GROUPfilesize(int count) {
  return ((off_t) count * sizeof(GROUPENTRY)) + sizeof(GROUPHEADER);
}

/* Check if the given GROUPLOC refers to GROUPENTRY that we don't have mmap'ed,
** if so then see if the file has been grown by another writer and remmap
*/
static bool GROUPremapifneeded(GROUPLOC loc) {
  struct stat	sb;

  if (loc.recno < GROUPcount)
    return true;

  if (fstat(GROUPfd, &sb) < 0)
    return false;

  if (GROUPfilesize(GROUPcount) >= sb.st_size)
    return true;

  if (GROUPheader) {
    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
      syslog(L_FATAL, "%s: Could not munmap group.index in GROUPremapifneeded: %m", LocalLogName);
      return false;
    }
  }

  GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
  GROUPheader = mmap(0, GROUPfilesize(GROUPcount), PROT_READ | PROT_WRITE,
		     MAP_SHARED, GROUPfd, 0);
  if (GROUPheader == MAP_FAILED) {
    syslog(L_FATAL, "%s: Could not mmap group.index in GROUPremapifneeded: %m", LocalLogName);
    return false;
  }
  GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
  return true;
}

/* This function does not need to lock because it's callers are expected to do so */
static bool GROUPexpand(int mode) {
  int	i;
  int	flag = 0;

  if (GROUPheader) {
    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
      syslog(L_FATAL, "%s: Could not munmap group.index in GROUPexpand: %m", LocalLogName);
      return false;
    }
  }
  GROUPcount += 1024;
  if (ftruncate(GROUPfd, GROUPfilesize(GROUPcount)) < 0) {
    syslog(L_FATAL, "%s: Could not extend group.index: %m", LocalLogName);
    return false;
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
  GROUPheader = mmap(0, GROUPfilesize(GROUPcount), flag, MAP_SHARED,
		     GROUPfd, 0);
  if (GROUPheader == MAP_FAILED) {
    syslog(L_FATAL, "%s: Could not mmap group.index in GROUPexpand: %m", LocalLogName);
    return false;
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
  return true;
}

static GROUPLOC GROUPnewnode(void) {
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

bool buffindexed_groupdel(char *group) {
  GROUPLOC	gloc;
  GROUPENTRY	*ge;

  gloc = GROUPfind(group, false);
  if (GROUPLOCempty(gloc)) {
    return true;
  }
  GROUPlock(gloc, INN_LOCK_WRITE);
  ge = &GROUPentries[gloc.recno];
  ge->deleted = time(NULL);
  HashClear(&ge->hash);
  GROUPlock(gloc, INN_LOCK_UNLOCK);
  return true;
}

static void GROUPLOCclear(GROUPLOC *loc) {
  loc->recno = -1;
}

static bool GROUPLOCempty(GROUPLOC loc) {
  return (loc.recno < 0);
}

static bool GROUPlockhash(enum inn_locktype type) {
  return inn_lock_range(GROUPfd, type, true, 0, sizeof(GROUPHEADER));
}

static bool GROUPlock(GROUPLOC gloc, enum inn_locktype type) {
  return inn_lock_range(GROUPfd,
	     type,
	     true,
	     sizeof(GROUPHEADER) + (sizeof(GROUPENTRY) * gloc.recno),
	     sizeof(GROUPENTRY));
}

#ifdef OV_DEBUG
static bool ovsetcurindexblock(GROUPENTRY *ge, GROUPENTRY *georig) {
#else
static bool ovsetcurindexblock(GROUPENTRY *ge) {
#endif /* OV_DEBUG */
  OVBUFF	*ovbuff;
  OV		ov;
  OVINDEXHEAD	ovindexhead;

  /* there is no index */
#ifdef OV_DEBUG
  ov = ovblocknew(georig ? georig : ge);
#else
  ov = ovblocknew();
#endif /* OV_DEBUG */
  if (ov.index == NULLINDEX) {
    syslog(L_ERROR, "%s: ovsetcurindexblock could not get new block", LocalLogName);
    return false;
  }
  if ((ovbuff = getovbuff(ov)) == NULL) {
    syslog(L_ERROR, "%s: ovsetcurindexblock could not get ovbuff block for new, %d, %d", LocalLogName, ov.index, ov.blocknum);
    return false;
  }
  ovindexhead.next = ovnull;
  ovindexhead.low = 0;
  ovindexhead.high = 0;
  if (PWRITE(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ov.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
    syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
    return true;
  }
  if (ge->baseindex.index == NULLINDEX) {
    ge->baseindex = ov;
  } else {
    if ((ovbuff = getovbuff(ge->curindex)) == NULL)
      return false;
    if (!ovusedblock(ovbuff, ge->curindex.blocknum, false, false)) {
      syslog(L_ERROR, "%s: block(%d, %d) not occupied (index)", LocalLogName, ovbuff->index, ge->curindex.blocknum);
#ifdef OV_DEBUG
      abort();
#else	/* OV_DEBUG */
      /* fix it */
      ovusedblock(ovbuff, ge->curindex.blocknum, true, true);
#endif /* OV_DEBUG */
    }
    ovindexhead.next = ov;
    ovindexhead.low = ge->curlow;
    ovindexhead.high = ge->curhigh;
    if (PWRITE(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
      syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
      return false;
    }
  }
  ge->curindex = ov;
  ge->curindexoffset = 0;
  ge->curlow = 0;
  ge->curhigh = 0;
  return true;
}

#ifdef OV_DEBUG
static bool ovaddrec(GROUPENTRY *ge, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires, GROUPENTRY *georig) {
#else
static bool ovaddrec(GROUPENTRY *ge, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
#endif /* OV_DEBUG */
  OV		ov;
  OVINDEX	ie;
  OVBUFF	*ovbuff;
  OVINDEXHEAD	ovindexhead;
  bool		needupdate = false;
#ifdef OV_DEBUG
  int		recno;
#endif /* OV_DEBUG */

  Nospace = false;
  if (OV_BLOCKSIZE < len) {
    syslog(L_ERROR, "%s: overview data must be under %d (%d)", LocalLogName, OV_BLOCKSIZE, len);
    return false;
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
      return false;
    }
    if ((ovbuff = getovbuff(ov)) == NULL) {
      syslog(L_ERROR, "%s: ovaddrec could not get ovbuff block for new, %d, %d, %ld", LocalLogName, ov.index, ov.blocknum, artnum);
      return false;
    }
    ge->curdata = ov;
    ge->curoffset = 0;
  } else if ((ovbuff = getovbuff(ge->curdata)) == NULL)
    return false;
  else if (OV_BLOCKSIZE - ge->curoffset < len) {
    /* too short to store data, allocate new block */
#ifdef OV_DEBUG
    ov = ovblocknew(georig ? georig : ge);
#else
    ov = ovblocknew();
#endif /* OV_DEBUG */
    if (ov.index == NULLINDEX) {
      syslog(L_ERROR, "%s: ovaddrec could not get new block", LocalLogName);
      return false;
    }
    if ((ovbuff = getovbuff(ov)) == NULL) {
      syslog(L_ERROR, "%s: ovaddrec could not get ovbuff block for new, %d, %d, %ld", LocalLogName, ov.index, ov.blocknum, artnum);
      return false;
    }
    ge->curdata = ov;
    ge->curoffset = 0;
  }
  if (!ovusedblock(ovbuff, ge->curdata.blocknum, false, false)) {
    syslog(L_ERROR, "%s: block(%d, %d) not occupied", LocalLogName, ovbuff->index, ge->curdata.blocknum);
#ifdef OV_DEBUG
    buffindexed_close();
    abort();
#else  /* OV_DEBUG */
    /* fix it */
    ovusedblock(ovbuff, ge->curdata.blocknum, true, true);
#endif /* OV_DEBUG */
  }

  if (PWRITE(ovbuff->fd, data, len, ovbuff->base + ge->curdata.blocknum * OV_BLOCKSIZE + ge->curoffset) != len) {
    syslog(L_ERROR, "%s: could not append overview record index '%d', blocknum '%d': %m", LocalLogName, ge->curdata.index, ge->curdata.blocknum);
    return false;
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
      return false;
    }
  }
  if ((ovbuff = getovbuff(ge->curindex)) == NULL)
    return false;
  if (!ovusedblock(ovbuff, ge->curindex.blocknum, false, false)) {
    syslog(L_ERROR, "%s: block(%d, %d) not occupied (index)", LocalLogName, ovbuff->index, ge->curindex.blocknum);
#ifdef OV_DEBUG
    buffindexed_close();
    abort();
#else  /* OV_DEBUG */
    /* fix this */
    ovusedblock(ovbuff, ge->curindex.blocknum, true, true);
#endif /* OV_DEBUG */
  }
  if (PWRITE(ovbuff->fd, &ie, sizeof(ie), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE + sizeof(OVINDEXHEAD) + sizeof(ie) * ge->curindexoffset) != sizeof(ie)) {
    syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
    return true;
  }
  if ((ge->curlow <= 0) || (ge->curlow > artnum)) {
    ge->curlow = artnum;
    needupdate = true;
  }
  if ((ge->curhigh <= 0) || (ge->curhigh < artnum)) {
    ge->curhigh = artnum;
    needupdate = true;
  }
  if (needupdate) {
    ovindexhead.next = ovnull;
    ovindexhead.low = ge->curlow;
    ovindexhead.high = ge->curhigh;
    if (PWRITE(ovbuff->fd, &ovindexhead, sizeof(OVINDEXHEAD), ovbuff->base + ge->curindex.blocknum * OV_BLOCKSIZE) != sizeof(OVINDEXHEAD)) {
      syslog(L_ERROR, "%s: could not write index record index '%d', blocknum '%d': %m", LocalLogName, ge->curindex.index, ge->curindex.blocknum);
      return true;
    }
  }
  if ((ge->low <= 0) || (ge->low > artnum))
    ge->low = artnum;
  if ((ge->high <= 0) || (ge->high < artnum))
    ge->high = artnum;
  ge->curindexoffset++;
  ge->curoffset += len;
  ge->count++;
  return true;
}

bool buffindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
  GROUPLOC	gloc;
  GROUPENTRY	*ge;

  if (len > OV_BLOCKSIZE) {
    syslog(L_ERROR, "%s: overview data is too large %d", LocalLogName, len);
    return true;
  }

  gloc = GROUPfind(group, false);
  if (GROUPLOCempty(gloc)) {
    return true;
  }
  GROUPlock(gloc, INN_LOCK_WRITE);
  /* prepend block(s) if needed. */
  ge = &GROUPentries[gloc.recno];
  if (Cutofflow && ge->low > artnum) {
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    return true;
  }
#ifdef OV_DEBUG
  if (!ovaddrec(ge, artnum, token, data, len, arrived, expires, NULL)) {
#else
  if (!ovaddrec(ge, artnum, token, data, len, arrived, expires)) {
#endif /* OV_DEBUG */
    if (Nospace) {
      GROUPlock(gloc, INN_LOCK_UNLOCK);
      syslog(L_ERROR, "%s: no space left for buffer, adding '%s'", LocalLogName, group);
      return false;
    }
    syslog(L_ERROR, "%s: could not add overview for '%s'", LocalLogName, group);
  }
  GROUPlock(gloc, INN_LOCK_UNLOCK);

  return true;
}

bool buffindexed_cancel(TOKEN token UNUSED) {
    return true;
}

#ifdef OV_DEBUG
static void freegroupblock(GROUPENTRY *ge) {
#else
static void freegroupblock(void) {
#endif /* OV_DEBUG */
  GROUPDATABLOCK	*gdb;
  int			i;
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

static void ovgroupunmap(void) {
  GROUPDATABLOCK	*gdb, *gdbnext;
  int			i;
  GIBLIST		*giblist, *giblistnext;

  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdbnext) {
      gdbnext = gdb->next;
      free(gdb);
    }
    groupdatablock[i] = NULL;
  }
  for (giblist = Giblist ; giblist != NULL ; giblist = giblistnext) {
    giblistnext = giblist->next;
    free(giblist);
  }
  Giblist = NULL;
  if (!Cache && (Gib != NULL)) {
    free(Gib);
    Gib = NULL;
    if (Cachesearch != NULL) {
      free(Cachesearch->group);
      free(Cachesearch);
      Cachesearch = NULL;
    }
  }
}

static void insertgdb(OV *ov, GROUPDATABLOCK *gdb) {
  gdb->next = groupdatablock[(ov->index + ov->blocknum) % GROUPDATAHASHSIZE];
  groupdatablock[(ov->index + ov->blocknum) % GROUPDATAHASHSIZE] = gdb;
  return;
}

static GROUPDATABLOCK *searchgdb(OV *ov) {
  GROUPDATABLOCK	*gdb;

  gdb = groupdatablock[(ov->index + ov->blocknum) % GROUPDATAHASHSIZE];
  for (; gdb != NULL ; gdb = gdb->next) {
    if (ov->index == gdb->datablk.index && ov->blocknum == gdb->datablk.blocknum)
      break;
  }
  return gdb;
}

static int INDEXcompare(const void *p1, const void *p2) {
  const OVINDEX *oi1 = p1;
  const OVINDEX *oi2 = p2;

  return oi1->artnum - oi2->artnum;
}

static bool
ovgroupmmap(GROUPENTRY *ge, ARTNUM low, ARTNUM high, bool needov)
{
  OV			ov = ge->baseindex;
  OVBUFF		*ovbuff;
  GROUPDATABLOCK	*gdb;
  int			pagefudge, limit, i, count, len;
  off_t                 offset, mmapoffset;
  OVBLOCK		*ovblock;
  void *		addr;
  GIBLIST		*giblist;

  if (low > high) {
    Gibcount = 0;
    return true;
  }
  Gibcount = ge->count;
  if (Gibcount == 0)
    return true;
  Gib = xmalloc(Gibcount * sizeof(OVINDEX));
  count = 0;
  while (ov.index != NULLINDEX) {
    ovbuff = getovbuff(ov);
    if (ovbuff == NULL) {
      syslog(L_ERROR, "%s: ovgroupmmap ovbuff is null(ovindex is %d, ovblock is %d", LocalLogName, ov.index, ov.blocknum);
      ovgroupunmap();
      return false;
    }
    offset = ovbuff->base + (ov.blocknum * OV_BLOCKSIZE);
    pagefudge = offset % pagesize;
    mmapoffset = offset - pagefudge;
    len = pagefudge + OV_BLOCKSIZE;
    if ((addr = mmap(NULL, len, PROT_READ, MAP_SHARED, ovbuff->fd, mmapoffset)) == MAP_FAILED) {
      syslog(L_ERROR, "%s: ovgroupmmap could not mmap index block: %m", LocalLogName);
      ovgroupunmap();
      return false;
    }
    ovblock = (OVBLOCK *)((char *)addr + pagefudge);
    if (ov.index == ge->curindex.index && ov.blocknum == ge->curindex.blocknum) {
      limit = ge->curindexoffset;
    } else {
      limit = OVINDEXMAX;
    }
    for (i = 0 ; i < limit ; i++) {
      if (Gibcount == count) {
	Gibcount += OV_FUDGE;
	Gib = xrealloc(Gib, Gibcount * sizeof(OVINDEX));
      }
      Gib[count++] = ovblock->ovindex[i];
    }
    giblist = xmalloc(sizeof(GIBLIST));
    giblist->ov = ov;
    giblist->next = Giblist;
    Giblist = giblist;
    ov = ovblock->ovindexhead.next;
    munmap(addr, len);
  }
  Gibcount = count;
  qsort(Gib, Gibcount, sizeof(OVINDEX), INDEXcompare);
  /* Remove duplicates. */
  for (i = 0; i < Gibcount - 1; i++) {
    if (Gib[i].artnum == Gib[i+1].artnum) {
      /* lower position is removed */
      Gib[i].artnum = 0;
    }
  }
  if (!needov)
    return true;
  count = 0;
  for (i = 0 ; i < Gibcount ; i++) {
    if (Gib[i].artnum == 0 || Gib[i].artnum < low || Gib[i].artnum > high)
      continue;
    ov.index = Gib[i].index;
    ov.blocknum = Gib[i].blocknum;
    gdb = searchgdb(&ov);
    if (gdb != NULL)
      continue;
    ovbuff = getovbuff(ov);
    if (ovbuff == NULL)
      continue;
    gdb = xmalloc(sizeof(GROUPDATABLOCK));
    gdb->datablk = ov;
    gdb->next = NULL;
    gdb->mmapped = false;
    insertgdb(&ov, gdb);
    count++;
  }
  if (count == 0)
    return true;
  if (count * OV_BLOCKSIZE > innconf->keepmmappedthreshold * 1024)
    /* large retrieval, mmap is done in ovsearch() */
    return true;
  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdb->next) {
      ov = gdb->datablk;
      ovbuff = getovbuff(ov);
      offset = ovbuff->base + (ov.blocknum * OV_BLOCKSIZE);
      pagefudge = offset % pagesize;
      mmapoffset = offset - pagefudge;
      gdb->len = pagefudge + OV_BLOCKSIZE;
      if ((gdb->addr = mmap(NULL, gdb->len, PROT_READ, MAP_SHARED, ovbuff->fd, mmapoffset)) == MAP_FAILED) {
	syslog(L_ERROR, "%s: ovgroupmmap could not mmap data block: %m", LocalLogName);
	free(gdb);
	ovgroupunmap();
	return false;
      }
      gdb->data = (char *)gdb->addr + pagefudge;
      gdb->mmapped = true;
    }
  }
  return true;
}

static void *
ovopensearch(char *group, ARTNUM low, ARTNUM high, bool needov)
{
  GROUPLOC		gloc;
  GROUPENTRY		*ge;
  OVSEARCH		*search;

  gloc = GROUPfind(group, false);
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

  search = xmalloc(sizeof(OVSEARCH));
  search->hi = high;
  search->lo = low;
  search->cur = 0;
  search->group = xstrdup(group);
  search->needov = needov;
  search->gloc = gloc;
  search->count = ge->count;
  search->gdb.mmapped = false;
  return (void *)search;
}

void *buffindexed_opensearch(char *group, int low, int high) {
  GROUPLOC		gloc;
  void			*handle;

  if (Gib != NULL) {
    free(Gib);
    Gib = NULL;
    if (Cachesearch != NULL) {
      free(Cachesearch->group);
      free(Cachesearch);
      Cachesearch = NULL;
    }
  }
  gloc = GROUPfind(group, false);
  if (GROUPLOCempty(gloc)) {
    return NULL;
  }
  GROUPlock(gloc, INN_LOCK_WRITE);
  if ((handle = ovopensearch(group, low, high, true)) == NULL)
    GROUPlock(gloc, INN_LOCK_UNLOCK);
  return(handle);
}

static bool ovsearch(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived, time_t *expires) {
  OVSEARCH		*search = (OVSEARCH *)handle;
  OV			srchov;
  GROUPDATABLOCK	*gdb;
  off_t			offset, mmapoffset;
  OVBUFF		*ovbuff;
  int			pagefudge;
  bool			newblock;

  if (search->cur == Gibcount) {
    return false;
  }
  while (Gib[search->cur].artnum == 0 || Gib[search->cur].artnum < search->lo) {
    search->cur++;
    if (search->cur == Gibcount)
      return false;
  }
  if (Gib[search->cur].artnum > search->hi)
      return false;

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
	  return true;
	}
	if (!gdb->mmapped) {
	  /* block needs to be mmapped */
	  if (search->gdb.mmapped) {
	    /* check previous mmapped area */
	    if (search->gdb.datablk.blocknum != srchov.blocknum || search->gdb.datablk.index != srchov.index) {
	      /* different one, release previous one */
	      munmap(search->gdb.addr, search->gdb.len);
	      newblock = true;
	    } else
	      newblock = false;
	  } else
	    newblock = true;
	  if (newblock) {
	    search->gdb.datablk.blocknum = srchov.blocknum;
	    search->gdb.datablk.index = srchov.index;
	    ovbuff = getovbuff(srchov);
	    offset = ovbuff->base + (srchov.blocknum * OV_BLOCKSIZE);
	    pagefudge = offset % pagesize;
	    mmapoffset = offset - pagefudge;
	    search->gdb.len = pagefudge + OV_BLOCKSIZE;
	    if ((search->gdb.addr = mmap(NULL, search->gdb.len, PROT_READ, MAP_SHARED, ovbuff->fd, mmapoffset)) == MAP_FAILED) {
	      syslog(L_ERROR, "%s: ovsearch could not mmap data block: %m", LocalLogName);
	      return false;
	    }
	    gdb->data = search->gdb.data = (char *)search->gdb.addr + pagefudge;
	    search->gdb.mmapped = true;
	  }
	}
	*data = (char *)gdb->data + Gib[search->cur].offset;
      }
    }
  }
  if (token) {
    if (Gib[search->cur].index == NULLINDEX && !search->needov) {
      search->cur++;
      return false;
    }
    *token = Gib[search->cur].token;
  }
  search->cur++;
  return true;
}

bool buffindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived) {
  return(ovsearch(handle, artnum, data, len, token, arrived, NULL));
}

static void ovclosesearch(void *handle, bool freeblock) {
  OVSEARCH		*search = (OVSEARCH *)handle;
  GROUPDATABLOCK	*gdb;
  int			i;
#ifdef OV_DEBUG
  GROUPENTRY	*ge;
  GROUPLOC	gloc;
#endif /* OV_DEBUG */

  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdb->next) {
      if (gdb->mmapped)
	munmap(gdb->addr, gdb->len);
    }
  }
  if (search->gdb.mmapped)
    munmap(search->gdb.addr, search->gdb.len);
  if (freeblock) {
#ifdef OV_DEBUG
    gloc = GROUPfind(search->group, false);
    if (!GROUPLOCempty(gloc)) {
      ge = &GROUPentries[gloc.recno];
      freegroupblock(ge);
    }
#else
    freegroupblock();
#endif /* OV_DEBUG */
  }
  ovgroupunmap();
  if (Cache) {
    Cachesearch = search;
  } else {
    free(search->group);
    free(search);
  }
  return;
}

void buffindexed_closesearch(void *handle) {
  OVSEARCH	*search = (OVSEARCH *)handle;
  GROUPLOC	gloc;

  gloc = search->gloc;
  ovclosesearch(handle, false);
  GROUPlock(gloc, INN_LOCK_UNLOCK);
}

/* get token from sorted index */
static bool gettoken(ARTNUM artnum, TOKEN *token) {
  int	i, j, offset, limit;
  offset = 0;
  limit = Gibcount;
  for (i = (limit - offset) / 2 ; i > 0 ; i = (limit - offset) / 2) {
    if (Gib[offset + i].artnum == artnum) {
      *token = Gib[offset + i].token;
      return true;
    } else if (Gib[offset + i].artnum == 0) {
      /* case for duplicated index */
      for (j = offset + i - 1; j >= offset ; j --) {
	if (Gib[j].artnum != 0)
	  break;
      }
      if (j < offset) {
	/* article not found */
	return false;
      }
      if (Gib[j].artnum == artnum) {
	*token = Gib[j].token;
	return true;
      } else if (Gib[j].artnum < artnum) {
	/* limit is not changed */
	offset += i + 1;
      } else {
	/* offset is not changed */
	limit = j;
      }
    } else if (Gib[offset + i].artnum < artnum) {
      /* limit is unchanged */
      offset += i + 1;
    } else {
      /* offset is unchanged */
      limit = offset + i;
    }
  }
  /* i == 0 */
  if (Gib[offset].artnum != artnum) {
    /* article not found */
    return false;
  }
  *token = Gib[offset].token;
  return true;
}

bool buffindexed_getartinfo(char *group, ARTNUM artnum, TOKEN *token) {
  GROUPLOC	gloc;
  void		*handle;
  bool		retval, grouplocked = false;

  if (Gib != NULL) {
    if (Cachesearch != NULL && strcmp(Cachesearch->group, group) != 0) {
      free(Gib);
      Gib = NULL;
      free(Cachesearch->group);
      free(Cachesearch);
      Cachesearch = NULL;
    } else {
      if (gettoken(artnum, token))
	return true;
      else {
	/* examine to see if overview index are increased */
	gloc = GROUPfind(group, false);
	if (GROUPLOCempty(gloc)) {
	  return false;
	}
	GROUPlock(gloc, INN_LOCK_WRITE);
	if ((Cachesearch != NULL) && (GROUPentries[gloc.recno].count == Cachesearch->count)) {
	  /* no new overview data is stored */
	  GROUPlock(gloc, INN_LOCK_UNLOCK);
	  return false;
	} else {
	  grouplocked = true;
	  free(Gib);
	  Gib = NULL;
	  if (Cachesearch != NULL) {
	    free(Cachesearch->group);
	    free(Cachesearch);
	    Cachesearch = NULL;
	  }
	}
      }
    }
  }
  if (!grouplocked) {
    gloc = GROUPfind(group, false);
    if (GROUPLOCempty(gloc)) {
      return false;
    }
    GROUPlock(gloc, INN_LOCK_WRITE);
  }
  if (!(handle = ovopensearch(group, artnum, artnum, false))) {
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    return false;
  }
  retval = buffindexed_search(handle, NULL, NULL, NULL, token, NULL);
  ovclosesearch(handle, false);
  GROUPlock(gloc, INN_LOCK_UNLOCK);
  return retval;
}

bool buffindexed_expiregroup(char *group, int *lo, struct history *h) {
  void		*handle;
  GROUPENTRY	newge, *ge;
  GROUPLOC	gloc, next;
  char		*data;
  int		i, len;
  TOKEN		token;
  ARTNUM	artnum, low, high;
  ARTHANDLE	*ah;
  char		flag;
  HASH		hash;
  time_t	arrived, expires;
  OVSEARCH	search;

  if (group == NULL) {
    for (i = 0 ; i < GROUPheader->freelist.recno ; i++) {
      gloc.recno = i;
      GROUPlock(gloc, INN_LOCK_WRITE);
      ge = &GROUPentries[gloc.recno];
      if (ge->expired >= OVrealnow || ge->count == 0) {
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	continue;
      }
      if (!ovgroupmmap(ge, ge->low, ge->high, true)) {
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	syslog(L_ERROR, "%s: could not mmap overview for hidden groups(%d)", LocalLogName, i);
	continue;
      }
      search.hi = ge->high;
      search.lo = ge->low;
      search.cur = 0;
      search.needov = true;
      while (ovsearch((void *)&search, NULL, &data, &len, &token, &arrived, &expires)) {
	if (innconf->groupbaseexpiry)
	  /* assuming "." is not real newsgroup */
	  OVgroupbasedexpire(token, ".", data, len, arrived, expires);
      }
#ifdef OV_DEBUG
      freegroupblock(ge);
#else
      freegroupblock();
#endif
      ovgroupunmap();
      ge->expired = time(NULL);
      ge->count = 0;
      GROUPlock(gloc, INN_LOCK_UNLOCK);
    }
    return true;
  }
  gloc = GROUPfind(group, false);
  if (GROUPLOCempty(gloc)) {
    return false;
  }
  GROUPlock(gloc, INN_LOCK_WRITE);
  ge = &GROUPentries[gloc.recno];
  if (ge->count == 0) {
    if (ge->low < ge->high)
      ge->low = ge->high;
    if (lo)
      *lo = ge->low + 1;
    ge->expired = time(NULL);
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    return true;
  }
  flag = ge->flag;
  hash = ge->hash;
  next = ge->next;
  low = ge->low;
  high = ge->high;

  newge.low = 0;
  setinitialge(&newge, hash, &flag, next, 0, high);
  if ((handle = ovopensearch(group, low, high, true)) == NULL) {
    ge->expired = time(NULL);
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    syslog(L_ERROR, "%s: could not open overview for '%s'", LocalLogName, group);
    return false;
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
      if (!OVhisthasmsgid(h, data))
	continue; 
    }
    if (innconf->groupbaseexpiry && OVgroupbasedexpire(token, group, data, len, arrived, expires))
      continue;
#ifdef OV_DEBUG
    if (!ovaddrec(&newge, artnum, token, data, len, arrived, expires, ge)) {
#else
    if (!ovaddrec(&newge, artnum, token, data, len, arrived, expires)) {
#endif /* OV_DEBUG */
      ovclosesearch(handle, true);
      ge->expired = time(NULL);
      GROUPlock(gloc, INN_LOCK_UNLOCK);
      syslog(L_ERROR, "%s: could not add new overview for '%s'", LocalLogName, group);
      return false;
    }
  }
  if (newge.low == 0)
    /* no article for the group */
    newge.low = newge.high;
  *ge = newge;
  if (lo) {
    if (ge->count == 0) {
      if (ge->low < ge->high)
	  ge->low = ge->high;
      /* lomark should be himark + 1, if no article for the group */
      *lo = ge->low + 1;
    } else
      *lo = ge->low;
  }
  ovclosesearch(handle, true);
  ge->expired = time(NULL);
  GROUPlock(gloc, INN_LOCK_UNLOCK);
  return true;
}

bool buffindexed_ctl(OVCTLTYPE type, void *val) {
  int			total, used, *i, j;
  OVBUFF		*ovbuff = ovbufftab;
  OVSORTTYPE		*sorttype;
  bool			*boolval;
  GROUPDATABLOCK	*gdb;

  switch (type) {
  case OVSPACE:
    for (total = used = 0 ; ovbuff != (OVBUFF *)NULL ; ovbuff = ovbuff->next) {
      ovlock(ovbuff, INN_LOCK_READ);
      ovreadhead(ovbuff);
      total += ovbuff->totalblk;
      used += ovbuff->usedblk;
      ovlock(ovbuff, INN_LOCK_UNLOCK);
    }
    i = (int *)val;
    *i = (used * 100) / total;
    return true;
  case OVSORT:
    sorttype = (OVSORTTYPE *)val;
    *sorttype = OVNOSORT;
    return true;
  case OVCUTOFFLOW:
    Cutofflow = *(bool *)val;
    return true;
  case OVSTATICSEARCH:
    i = (int *)val;
    *i = true;
    for (j = 0 ; j < GROUPDATAHASHSIZE ; j++) {
      for (gdb = groupdatablock[j] ; gdb != NULL ; gdb = gdb->next) {
	if  (gdb->mmapped) {
	  *i = false;
	  return true;
	}
      }
    }
    return true;
  case OVCACHEKEEP:
    Cache = *(bool *)val;
    return true;
  case OVCACHEFREE:
    boolval = (bool *)val;
    *boolval = true;
    if (Gib != NULL) {
      free(Gib);
      Gib = NULL;
      if (Cachesearch != NULL) {
	free(Cachesearch->group);
	free(Cachesearch);
	Cachesearch = NULL;
      }
    }
    return true;
  default:
    return false;
  }
}

void buffindexed_close(void) {
  struct stat	sb;
  OVBUFF	*ovbuff = ovbufftab;
#ifdef OV_DEBUG
  FILE		*F = NULL;
  pid_t		pid;
  char		*path = NULL;
  int		i,j;
  struct ov_trace_array *trace;
  struct ov_name_table	*ntp;
  size_t length;
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
	    length = strlen(innconf->pathtmp) + 11;
	    path = xmalloc(length);
	    pid = getpid();
	    snprintf(path, length, "%s/%d", innconf->pathtmp, pid);
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
      length = strlen(innconf->pathtmp) + 11;
      path = xmalloc(length);
      pid = getpid();
      sprintf(path, length, "%s/%d", innconf->pathtmp, pid);
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
    free(path);
#endif /* OV_DEBUG */
  if (Gib != NULL) {
    free(Gib);
    Gib = NULL;
    if (Cachesearch != NULL) {
      free(Cachesearch->group);
      free(Cachesearch);
      Cachesearch = NULL;
    }
  }
  if (fstat(GROUPfd, &sb) < 0)
    return;
  close(GROUPfd);

  if (GROUPheader) {
    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
      syslog(L_FATAL, "%s: could not munmap group.index in buffindexed_close: %m", LocalLogName);
      return;
    }
    GROUPheader = NULL;
  }

  /* sync the bit field */
  ovbuff = ovbufftab;
  for (; ovbuff != (OVBUFF *)NULL; ovbuff = ovbuffnext) {
    if (ovbuff->dirty) {
      ovbuff->dirty = OVBUFF_SYNC_COUNT + 1;
      ovflushhead(ovbuff);
    }
    ovbuffnext = ovbuff->next;
    free(ovbuff);
  }
  ovbufftab = NULL;
  ovbuffnext = NULL;
}

#ifdef DEBUG
static int countgdb(void) {
  int			i, count = 0;
  GROUPDATABLOCK	*gdb;

  for (i = 0 ; i < GROUPDATAHASHSIZE ; i++) {
    for (gdb = groupdatablock[i] ; gdb != NULL ; gdb = gdb->next)
      count++;
  }
  return count;
}

int 
main(int argc, char **argv) {
  char			*group, flag[2], buff[OV_BLOCKSIZE];
  int			lo, hi, count, flags, i;
  OVSEARCH		*search;
  GROUPENTRY		*ge;
  GROUPLOC		gloc;
  GIBLIST		*giblist;

  if (argc != 2) {
    fprintf(stderr, "only one argument can be specified\n");
    exit(1);
  }
  /* if innconf isn't already read in, do so. */
  if (innconf == NULL) {
    if (!innconf_read(NULL)) {
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
    fprintf(stdout, "left articles are %d for %d, last expiry is %ld\n", ge->count, gloc.recno, (long) ge->expired);
    if (ge->count == 0) {
      GROUPlock(gloc, INN_LOCK_UNLOCK);
      exit(0);
    }
    if (!ovgroupmmap(ge, ge->low, ge->high, true)) {
      fprintf(stderr, "ovgroupmmap failed\n");
      GROUPlock(gloc, INN_LOCK_UNLOCK);
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
	fprintf(stdout, "    %lu empty\n", (unsigned long) Gib[i].offset);
      else {
	fprintf(stdout, "    %lu %d\n", (unsigned long) Gib[i].offset, Gib[i].len);
      }
    }
    ovgroupunmap();
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    exit(0);
  }
  gloc = GROUPfind(group, false);
  if (GROUPLOCempty(gloc)) {
    fprintf(stderr, "gloc is null\n");
  }
  GROUPlock(gloc, INN_LOCK_READ);
  ge = &GROUPentries[gloc.recno];
  fprintf(stdout, "base %d(%d), cur %d(%d), expired at %s\n", ge->baseindex.blocknum, ge->baseindex.index, ge->curindex.blocknum, ge->curindex.index, ge->expired == 0 ? "none\n" : ctime(&ge->expired));
  if (!buffindexed_groupstats(group, &lo, &hi, &count, &flags)) {
    fprintf(stderr, "buffindexed_groupstats failed for group %s\n", group);
    exit(1);
  }
  flag[0] = (char)flags;
  flag[1] = '\0';
  fprintf(stdout, "%s: low is %d, high is %d, count is %d, flag is '%s'\n", group, lo, hi, count, flag);
  if ((search = (OVSEARCH *)ovopensearch(group, lo, hi, true)) == NULL) {
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
      fprintf(stdout, "    %lu empty\n", (unsigned long) Gib[i].offset);
    else {
      fprintf(stdout, "    %lu %d\n", (unsigned long) Gib[i].offset, Gib[i].len);
    }
  }
  {
    ARTNUM artnum;
    char *data;
    int len;
    TOKEN token;
    while (buffindexed_search((void *)search, &artnum, &data, &len, &token, NULL)) {
      if (len == 0)
	fprintf(stdout, "%lu: len is 0\n", artnum);
      else {
	memcpy(buff, data, len);
	buff[len] = '\0';
	fprintf(stdout, "%lu: %s\n", artnum, buff);
      }
    }
  }
  return 0;
}
#endif /* DEBUG */
