/*  $Id$
**
**  Library routines needed for handling CAF (Crunched Article Files)
**  Written by Richard Todd (rmtodd@mailhost.ecn.uoknor.edu) 3/24/96,
**  modified extensively since then.  Altered to work with storage manager
**  in INN1.8 by rmtodd 3/27/98.
*/

#include "config.h"
#include "clibrary.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "libinn.h"

#define CAF_INNARDS
#include "caf.h"

/* following code lifted from inndf.c */

#ifdef HAVE_STATVFS
#include <sys/statvfs.h>		/* specific includes */
/* XXX is there a 'fstatvfs'? I don't have such a system to check--rmtodd*/
#define STATFUNCT	fstatvfs		/* function call */
#define STATSTRUC	statvfs		/* structure name */
#define STATAVAIL	f_bavail	/* blocks available */
#define STATMULTI	f_frsize	/* fragment size/block size */
#define STATINODE	f_favail	/* inodes available */
#define STATTYPES	u_long		/* type of f_bavail etc */
#define STATFORMT	"%lu"		/* format string to match */
#define STATFORMTPAD	"%*lu"		/* format string to match */
#endif /* HAVE_STATVFS */

#ifdef HAVE_STATFS
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif /* HAVE_SYS_VFS_H */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif /* HAVE_SYS_MOUNT_H */
#define STATFUNCT	fstatfs
#define STATSTRUC	statfs
#define STATAVAIL	f_bavail
#define STATMULTI	f_bsize
#define STATINODE	f_ffree;
#define STATTYPES	long
#define STATFORMT	"%ld"
#define STATFORMTPAD	"%*ld"
#endif /* HAVE_STATFS */

int CAFClean(char *path, int verbose, double PercentFreeThreshold);

int caf_error = 0;
int caf_errno = 0;

/* check assertions in code (lifted from lib/malloc.c) */
#define	ASSERT(p)   do { if (!(p)) botch(__FILE__, __LINE__, #p); } while (0)

static void
botch(const char *f, int l, const char *s)
{

	fprintf(stderr, "assertion botched: %s:%d:%s\n", f,l,s);
	fflush(stderr); /* if stderr writing to file--needed? */
	abort();
}


/* set error code appropriately. */
static void
CAFError(int code)
{
    caf_error = code;
    if (caf_error == CAF_ERR_IO) {
	caf_errno = errno;
    }
}

/*
** Wrapper around read that calls CAFError if needed. 0 for success, -1 for  failure.
*/

static int
OurRead(int fd, void *buf, size_t n)
{
    ssize_t rval;

    rval = read(fd, buf, n);
    if (rval < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }
    if ((size_t) rval < n) {
	/* not enough data! */
	CAFError(CAF_ERR_BADFILE);
	return -1;
    }
    return 0;
}

/* Same as OurRead except for writes. */
static int
OurWrite(int fd, const void *buf, size_t n)
{
    ssize_t rval;

    rval = write(fd, buf, n);
    if (rval < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }
    if ((size_t) rval < n) {
	/* not enough data written */
	CAFError(CAF_ERR_IO);
	return -1;
    }
    return 0;
}

/*
** Given an fd, read in a CAF_HEADER from a file. Ret. 0 on success.
*/

int
CAFReadHeader(int fd, CAFHEADER *h)
{
    /* probably already at start anyway, but paranoia is good. */
    if (lseek(fd, 0L, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }

    if (OurRead(fd, h, sizeof(CAFHEADER)) < 0) return -1;

    if (strncmp(h->Magic, CAF_MAGIC, CAF_MAGIC_LEN) != 0) {
	CAFError(CAF_ERR_BADFILE);
	return -1;
    }
    return 0;
}

/* 
** Seek to the TOC entry for a given article.  As usual, -1 for error, 0 succ.
*/

static int
CAFSeekTOCEnt(int fd, CAFHEADER *head, ARTNUM art)
{
    off_t offset;

    offset = sizeof(CAFHEADER) + head->FreeZoneTabSize;
    offset += (art - head->Low) * sizeof(CAFTOCENT);
    if (lseek(fd, offset, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }
    return 0;
}

/*
** Fetch the TOC entry for a  given article.  As usual -1 for error, 0 success */

static int
CAFGetTOCEnt(int fd, CAFHEADER *head, ARTNUM art, CAFTOCENT *tocp)
{
    if (CAFSeekTOCEnt(fd, head, art) < 0) {
	return -1;
    }

    if (OurRead(fd, tocp, sizeof(CAFTOCENT)) < 0) return -1;

    return 0;
}

/*
** Round an offset up to the next highest block boundary. Needs the CAFHEADER
** to find out what the blocksize is.
*/
off_t
CAFRoundOffsetUp(off_t off, unsigned int blocksize)
{
    off_t off2;

    /* Zero means default blocksize, though we shouldn't need this for long,
       as all new CAF files will have BlockSize set. */
    if (blocksize == 0) {
	blocksize = CAF_DEFAULT_BLOCKSIZE;
    }

    off2 = ((off + blocksize - 1) / blocksize) * blocksize;
    return off2;
}

/*
** Dispose of an already-allocated CAFBITMAP.
*/
void
CAFDisposeBitmap(CAFBITMAP *bm)
{
    unsigned int i;
    CAFBMB *bmb;

    for (i = 0 ; i < bm->NumBMB ; ++i) {
	if (bm->Blocks[i]) {
	    bmb = bm->Blocks[i];
	    if (bmb->BMBBits) free(bmb->BMBBits);
	    free(bmb);
	}
    }
    free(bm->Blocks);
    free(bm->Bits);
    free(bm);
}

/*
** Read the index bitmap from a CAF file, return a CAFBITMAP structure.
*/

/* define this instead of littering all our formulas with semi-mysterious 8s. */
#define BYTEWIDTH 8

CAFBITMAP *
CAFReadFreeBM(int fd, CAFHEADER *h)
{
    size_t i;
    struct stat statbuf;
    CAFBITMAP *bm;

    if (lseek(fd, sizeof(CAFHEADER), SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return NULL;
    }
    bm = xmalloc(sizeof(CAFBITMAP));

    bm->FreeZoneTabSize = h->FreeZoneTabSize;
    bm->FreeZoneIndexSize = h->FreeZoneIndexSize;
    bm->NumBMB = BYTEWIDTH * bm->FreeZoneIndexSize;
    bm->BytesPerBMB = (h->BlockSize) * (h->BlockSize * BYTEWIDTH);
    bm->BlockSize = h->BlockSize;
    
    bm->Blocks = xmalloc(bm->NumBMB * sizeof(CAFBMB *));
    bm->Bits = xmalloc(bm->FreeZoneIndexSize);
    for (i = 0 ; i < bm->NumBMB ; ++i) {
	bm->Blocks[i] = NULL;
    }
	
    if (OurRead(fd, bm->Bits, bm->FreeZoneIndexSize) < 0) {
	CAFDisposeBitmap(bm);
	return NULL;
    }

    bm->StartDataBlock = h->StartDataBlock;

    if (fstat(fd, &statbuf) < 0) {
	/* it'd odd for this to fail, but paranoia is good for the soul. */
	CAFError(CAF_ERR_IO);
	CAFDisposeBitmap(bm);
	return NULL;
    }
    /* round st_size down to a mult. of BlockSize */
    bm->MaxDataBlock = (statbuf.st_size / bm->BlockSize) * bm->BlockSize + bm->BlockSize;
    /* (note: MaxDataBlock points to the block *after* the last block of the file. */
    return bm;
}

/*
** Fetch a given bitmap block into memory, and make the CAFBITMAP point to
** the new BMB appropriately.  Return NULL on failure, and the BMB * on success.
*/
static CAFBMB *
CAFFetchBMB(unsigned int blkno, int fd, CAFBITMAP *bm)
{
    CAFBMB *newbmb;

    ASSERT(blkno < bm->NumBMB);
    /* if already in memory, don't need to do anything. */
    if (bm->Blocks[blkno]) return bm->Blocks[blkno];

    newbmb = xmalloc(sizeof(CAFBMB));

    newbmb->Dirty = 0;
    newbmb->StartDataBlock = bm->StartDataBlock + blkno*(bm->BytesPerBMB);

    newbmb->MaxDataBlock = newbmb->StartDataBlock + bm->BytesPerBMB;
    if (newbmb->MaxDataBlock > bm->MaxDataBlock) {
	/* limit the per-BMB MaxDataBlock to that for the bitmap as a whole */
	newbmb->MaxDataBlock = bm->MaxDataBlock;
    }

    newbmb->BMBBits = xmalloc(bm->BlockSize);

    if (lseek(fd, (blkno + 1) * bm->BlockSize, SEEK_SET) < 0) {
	free(newbmb->BMBBits);
	free(newbmb);
	CAFError(CAF_ERR_IO);
	return NULL;
    }

    if (OurRead(fd, newbmb->BMBBits, bm->BlockSize) < 0) {
	free(newbmb->BMBBits);
        free(newbmb);
	return NULL;
    }

    bm->Blocks[blkno] = newbmb;
    return newbmb;
}

/*
** Flush out (if needed) a BMB to disk.  Return 0 on success, -1 on failure.
*/

static int
CAFFlushBMB(unsigned int blkno, int fd, CAFBITMAP *bm)
{
    CAFBMB *bmb;

    ASSERT(blkno < bm->NumBMB);

    if (bm->Blocks[blkno] == NULL) return 0; /* nothing to do. */

    bmb = bm->Blocks[blkno];
    if (!bmb->Dirty) return 0;

    if (lseek(fd, (blkno + 1) * bm->BlockSize, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }

    if (OurWrite(fd, bmb->BMBBits, bm->BlockSize) < 0) return -1;

    bmb->Dirty = 0; 
    return 0;
}


/*
** Write the free bit map to the CAF file.  Return 0 on success, -1 on failure.
*/
static int
CAFWriteFreeBM(int fd, CAFBITMAP *bm)
{
    size_t blkno;
    
    for (blkno = 0 ; blkno < bm->NumBMB ; ++blkno) {
	if (CAFFlushBMB(blkno, fd, bm) < 0) {
	    return -1;
	}
    }

    if (lseek(fd, sizeof(CAFHEADER), SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }

    if(OurWrite(fd, bm->Bits, bm->FreeZoneIndexSize) < 0) return -1;

    return 0;
}

/* 
** Determine if a block at a given offset is free.  Return 1 if it is, 0 
** otherwise.
*/

int
CAFIsBlockFree(CAFBITMAP *bm, int fd, off_t block)
{
    unsigned int ind;
    char mask;
    int blkno;
    CAFBMB *bmb;

    /* round block down to BlockSize boundary. */
    block = block - (block % bm->BlockSize);

    /* if < Start, always return 0 (should never happen in real usage) */
    if (block < bm->StartDataBlock) return 0;
    
    /* if off the end, also return 0. */
    if (block >= bm->MaxDataBlock) return 0;

    /* find blk # of appropriate BMB */
    blkno = (block - bm->StartDataBlock) / bm->BytesPerBMB;

    bmb = CAFFetchBMB(blkno, fd, bm);
    /* ick. not a lot we can do here if this fails. */ 
    if (bmb == NULL) return 0;

    /* Sanity checking that we have the right BMB. */
    ASSERT(block >= bmb->StartDataBlock);
    ASSERT(block < bmb->MaxDataBlock);

    ind = ((block - bmb->StartDataBlock) / bm->BlockSize) / BYTEWIDTH;
    mask =  1 << (((block - bmb->StartDataBlock) / bm->BlockSize) % BYTEWIDTH);

    ASSERT(ind < bm->BlockSize);
    
    return ((bmb->BMBBits[ind]) & mask) != 0;
}

/*
** Check if a bitmap chunk is all zeros or not.
*/
static int
IsMapAllZero(char *data, int len)
{
    int i;
    for (i = 0 ; i < len ; ++i) {
	if (data[i] != 0) return 0;
    }
    return 1;
}

/* Set the free bitmap entry for a given block to be a given value (1 or 0). */
static void
CAFSetBlockFree(CAFBITMAP *bm, int fd, off_t block, int isfree)
{
    unsigned int ind;
    char mask;
    int blkno;
    CAFBMB *bmb;
    int allzeros;

    /* round block down to BlockSize boundary. */
    block = block - (block % bm->BlockSize);

    /* if < Start, always return (should never happen in real usage) */
    if (block < bm->StartDataBlock) return;
    
    /* if off the end, also return. */
    if (block >= bm->MaxDataBlock) return;
    /* find blk # of appropriate BMB */
    blkno = (block - bm->StartDataBlock) / bm->BytesPerBMB;

    bmb = CAFFetchBMB(blkno, fd, bm);
    /* ick. not a lot we can do here if this fails. */ 
    if (bmb == NULL) return;

    /* Sanity checking that we have the right BMB. */
    ASSERT(block >= bmb->StartDataBlock);
    ASSERT(block < bmb->MaxDataBlock);

    ind = ((block - bmb->StartDataBlock) / bm->BlockSize) / BYTEWIDTH;
    mask =  1 << (((block - bmb->StartDataBlock) / bm->BlockSize) % BYTEWIDTH);

    ASSERT(ind < bm->BlockSize);

    if (isfree) {
	bmb->BMBBits[ind] |= mask; /* set bit */
    } else {
	bmb->BMBBits[ind] &= ~mask; /* clear bit. */
    }

    bmb->Dirty = 1;

    /* now have to set top level (index) bitmap appropriately */
    allzeros = IsMapAllZero(bmb->BMBBits, bm->BlockSize);

    ind = blkno/BYTEWIDTH;
    mask = 1 << (blkno % BYTEWIDTH);

    if (allzeros) {
	bm->Bits[ind] &= ~mask; /* clear bit */
    } else {
	bm->Bits[ind] |= mask;
    }

    return;
}

/* 
** Search a freebitmap to find n contiguous free blocks.  Returns 0 for
** failure, offset of starting block if successful.
** XXX does not attempt to find chunks that span BMB boundaries.  This is 
** messy to fix.
** (Actually I think this case  works, as does the case when it tries to find
** a block bigger than BytesPerBMB.  Testing reveals that it does seem to work, 
** though not optimally (some BMBs will get scanned several times).  
*/
static off_t
CAFFindFreeBlocks(CAFBITMAP *bm, int fd, unsigned int n)
{
    off_t startblk, curblk;
    unsigned int i, ind, blkno, j;
    unsigned int bmblkno, k, l;
    CAFBMB *bmb;

    /* Iterate over all bytes and all bits in the toplevel bitmap. */
    for (k = 0 ; k < bm->FreeZoneIndexSize ; ++k) {
	if (bm->Bits[k] == 0) continue;
	for (l = 0; l < BYTEWIDTH ; ++l) {
	    if ((bm->Bits[k] & (1 << l)) != 0) {
		/* found a bit set! fetch the BMB. */
		bmblkno = k*BYTEWIDTH + l;
		bmb = CAFFetchBMB(bmblkno, fd, bm);
		if (bmb == NULL) return 0;

		curblk = bmb->StartDataBlock;
		while (curblk < bmb->MaxDataBlock) {
		    blkno = (curblk - bmb->StartDataBlock)/(bm->BlockSize);
		    ind = blkno/BYTEWIDTH;
		    if (bmb->BMBBits[ind] == 0) { 
			/* nothing set in this byte, skip this byte and move on. */
			blkno = (ind+1)*BYTEWIDTH;
			curblk = blkno*bm->BlockSize + bmb->StartDataBlock;
			continue;
		    }

		    /* scan rest of current byte for 1 bits */
		    for (j = blkno % BYTEWIDTH ; j < BYTEWIDTH ; j++, curblk += bm->BlockSize) {
			if ((bmb->BMBBits[ind] & (1 << j)) != 0) break; 
		    }
		    if (j == BYTEWIDTH) continue;

		    /* found a 1 bit, set startblk to be locn of corresponding free blk. */
		    startblk = curblk;
		    curblk += bm->BlockSize;

		    /* scan for n blocks in a row. */
		    for (i = 1 ; i < n ; ++i, curblk += bm->BlockSize) {
			if (!CAFIsBlockFree(bm, fd, curblk)) break; 
		    }

		    if (i == n) return startblk;

		    /* otherwise curblk points to a non-free blk, continue searching from there. */
		    continue;
		}
	    }
	}
    }
    return 0;
}

/*
** Open a CAF file for reading and seek to the start of a given article.
** Take as args the CAF file pathname, article #, and a pointer to where
** the art. length can be returned.
*/

int
CAFOpenArtRead(const char *path, ARTNUM art, size_t *len)
{
    CAFHEADER head;
    int fd;
    CAFTOCENT tocent;
    struct stat st;

    if ( (fd = open(path, O_RDONLY)) < 0) {
	/* 
	** if ENOENT (not there), just call this "article not found",
	** otherwise it's a more serious error and stash the errno.
	*/
	if (errno == ENOENT) {
	    CAFError(CAF_ERR_ARTNOTHERE);
	} else {
	    CAFError(CAF_ERR_IO);
	}
	return -1;
    }

    /* Fetch the header */
    if (CAFReadHeader(fd, &head) < 0) {
	close(fd);
	return -1;
    }

    /* Is the requested article even in the file? */
    if (art < head.Low || art > head.High) {
	CAFError(CAF_ERR_ARTNOTHERE);
	close(fd);
	return -1;
    }

    if (CAFGetTOCEnt(fd, &head, art, &tocent) < 0) {
	close(fd);
	return -1;
    }

    if (tocent.Size == 0) {
	/* empty/otherwise not present article */
	CAFError(CAF_ERR_ARTNOTHERE);
	close(fd);
	return -1;
    }

    if (lseek(fd, tocent.Offset, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	close(fd);
	return -1;
    }

    /* I'm not sure if this fstat is worth the speed hit, but unless we check
       here, we may simply segfault when we try to access mmap'd space beyond
       the end of the file.  I think robustness wins. */
    if (fstat(fd, &st) == 0)
        if (tocent.Size > st.st_size - tocent.Offset) {
            CAFError(CAF_ERR_IO);
            close(fd);
            return -1;
        }

    *len = tocent.Size;
    return fd;
}

/*
** variables for keeping track of currently pending write.
** FIXME: assumes only one article open for writing at a time.  
*/

static int CAF_fd_write;
static ARTNUM CAF_artnum_write;
static off_t CAF_startoffset_write;
static CAFHEADER CAF_header_write;
static CAFBITMAP *CAF_free_bitmap_write;
static unsigned int CAF_numblks_write;

/* 
** Given estimated size of CAF file (i.e., the size of the old CAF file found
** by cafclean), find an "optimal" blocksize (one big enough so that the 
** default FreeZoneTabSize can cover the entire
** file so that we don't "lose" free space and not be able to reuse it.
** (Currently only returns CAF_DEFAULT_BLOCKSIZE, as with the new 2-level
** bitmaps, the FreeZoneTabSize that results from a 512-byte blocksize can 
** handle any newsgroup with <7.3G of data.  Yow!)
*/

static unsigned int
CAFFindOptimalBlocksize(ARTNUM tocsize UNUSED, size_t cfsize)
{

    if (cfsize == 0) return CAF_DEFAULT_BLOCKSIZE; /* no size given, use default. */

    return CAF_DEFAULT_BLOCKSIZE;
}

/* 
** Create an empty CAF file.  Used by CAFOpenArtWrite.
** Must be careful here and create the new CAF file under a temp name and then
** link it into place, to avoid possible race conditions.
** Note: CAFCreateCAFFile returns fd locked, also to avoid race conds.
** New args added for benefit of the cleaner program: "nolink", a flag that
** tells it not to bother with the link business, and "temppath", a pointer
** to a buffer that (if non-null) gets the pathname of the temp file copied
** to it. "estcfsize", if nonzero, is an estimate of what the CF filesize will
** be, used to automatically select a good blocksize.
*/
int
CAFCreateCAFFile(char *cfpath, ARTNUM artnum, ARTNUM tocsize,
                 size_t estcfsize, int nolink, char *temppath, size_t pathlen)
{
    CAFHEADER head;
    int fd;
    char path[SPOOLNAMEBUFF];
    char finalpath[SPOOLNAMEBUFF];
    off_t offset;
    char nulls[1];

    strlcpy(finalpath, cfpath, sizeof(finalpath));
    /* create path with PID attached */
    snprintf(path, sizeof(path), "%s.%lu", cfpath, (unsigned long) getpid());
    /* 
    ** Shouldn't be anyone else with our pid trying to write to the temp.
    ** file, but there might be an old one lying around.  Nuke it.
    ** (yeah, I'm probably being overly paranoid.)
    */
    if (unlink(path) < 0 && errno != ENOENT) {
	CAFError(CAF_ERR_IO);
	return -1;
    }
    if ((fd = open(path, O_CREAT|O_EXCL|O_RDWR, 0666)) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }

    /* Initialize the header. */
    strncpy(head.Magic, CAF_MAGIC, CAF_MAGIC_LEN);
    head.Low = artnum;
    head.High = artnum;
    head.NumSlots = tocsize;
    head.Free = 0;
    head.BlockSize = CAFFindOptimalBlocksize(tocsize, estcfsize);
    head.FreeZoneIndexSize = head.BlockSize - sizeof(CAFHEADER);
    head.FreeZoneTabSize = head.FreeZoneIndexSize 
	+ head.BlockSize*head.FreeZoneIndexSize*BYTEWIDTH;
    head.StartDataBlock = CAFRoundOffsetUp(sizeof(CAFHEADER)
					   + head.FreeZoneTabSize + tocsize*sizeof(CAFTOCENT), head.BlockSize);

    head.spare[0] = head.spare[1] = head.spare[2] = 0;
    
    if (OurWrite(fd, &head, sizeof(head)) < 0) {
	close(fd);
	return -1;
    }

    offset = sizeof(CAFHEADER) + head.FreeZoneTabSize +
	sizeof(CAFTOCENT) * tocsize;

    if (lseek(fd, offset, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }
    /* 
    ** put a null after the TOC as a 'placeholder', so that we'll have a sparse
    ** file and that EOF will be at where the articles should start going.
    */
    nulls[0] = 0;
    if (OurWrite(fd, nulls, 1) < 0) {
	close(fd);
	return -1;
    }
    /* shouldn't be anyone else locking our file, since temp file has unique
       PID-based name ... */
    if (!inn_lock_file(fd, INN_LOCK_WRITE, false)) {
	CAFError(CAF_ERR_IO);
	close(fd);
	return -1;
    }	

    if (nolink) {
	if (temppath != NULL) {
	    strlcpy(temppath, path, pathlen);
	}
	return fd;
    }

    /*
    ** Try to link to the real one. NOTE: we may get EEXIST here, which we
    ** will handle specially in OpenArtWrite.
    */
    if (link(path, finalpath) < 0) {
	CAFError(CAF_ERR_IO);
	/* bounced on the link attempt, go ahead and unlink the temp file and return. */
	unlink(path);
	close(fd);
	return -1;
    }
    /*
    ** Unlink the temp. link. Do we really care if this fails? XXX
    ** Not sure what we can do anyway.
    */
    unlink(path);
    return fd;
}

/*
** Try to open a CAF file for writing a given article.  Return an fd to 
** write to (already positioned to the right place to write at) if successful,
** else -1 on error.  if LockFlag is true, we wait for a lock on the file,
** otherwise we fail if we can't lock it.  If size is != 0, we try to allocate
** a chunk from free space in the CAF instead of writing at the end of the
** file.  Artp is a pointer to the article number to use; if the article number
** is zero, the next free article # ("High"+1) will be used, and *artp will
** be set accordingly.   Once the CAF file is open/created, CAFStartWriteFd()
** does the remaining dirty work.
*/

int
CAFOpenArtWrite(char *path, ARTNUM *artp, int waitlock, size_t size)
{
    int fd;

    while (true) {
	/* try to open the file and lock it. */
	if ((fd = open(path, O_RDWR)) < 0) {
	    /* if ENOENT, try creating CAF file, otherwise punt. */
	    if (errno != ENOENT) {
		CAFError(CAF_ERR_IO);
		return -1;
	    } else {
	        /* 
		** the *artp? business is so that if *artp==0, we set initial
		** article # to 1.
		*/
		fd = CAFCreateCAFFile(path, (*artp ? *artp : 1),
                                      CAF_DEFAULT_TOC_SIZE, 0, 0, NULL);
		/*
		** XXX possible race condition here, so we check to see if
		** create failed because of EEXIST.  If so, we go back to top
		** of loop, because someone else was trying to create at the
		** same time. 
		** Is this the best way to solve this? 
		** (Hmm.  this condition should be quite rare, occuring only
		** when two different programs are simultaneously doing 
		** CAFOpenArtWrite()s, and no CF file exists previously.)
		*/
		if (fd < 0) {
		    if (caf_errno == EEXIST) {
			/* ignore the error and try again */
			continue; 
		    }
		    return -1; /* other error, assume caf_errno set properly. */
		}
		/* 
		** break here, because CreateCAFFile does 
		** lock fd, so we don't need to flock it ourselves.  
		*/
		break;
	    }
	}

	/* try a nonblocking lock attempt first. */
	if (inn_lock_file(fd, INN_LOCK_WRITE, false)) break;

	if (!waitlock) {
	    CAFError(CAF_ERR_FILEBUSY);
	    close(fd); /* keep from leaking fds. */
	    return -1;
	}
	/* wait around to try and get a lock. */
	inn_lock_file(fd, INN_LOCK_WRITE, true);
	/*
        ** and then close and reopen the file, in case someone changed the
	** file out from under us.
	*/ 
	close(fd);
    }
    return CAFStartWriteFd(fd, artp, size);
}

/*
** Like CAFOpenArtWrite(), except we assume the CAF file is already 
** open/locked, and we have an open fd to it.
*/
int
CAFStartWriteFd(int fd, ARTNUM *artp, size_t size)
{
    CAFHEADER head;
    CAFTOCENT tocent;
    off_t offset, startoffset;
    unsigned int numblks = 0;
    CAFBITMAP *freebm;
    ARTNUM art;

    /* fd is open to the CAF file, open for write and locked. */
    /* Fetch the header */
    if (CAFReadHeader(fd, &head) < 0) {
	close(fd);
	return -1;
    }

    /* check for zero article number and  handle accordingly. */
    art = *artp;
    if (art == 0) {
	/* assign next highest article number. */
	art = head.High + 1;
	/* and pass to caller. */
	*artp = art;
    }

    /* Is the requested article even in the file? */
    if (art < head.Low || art >= head.Low + head.NumSlots) {
	CAFError(CAF_ERR_ARTWONTFIT);
	close(fd);
	return -1;
    }

    /*
    ** Get the CAFTOCENT for that article, but only if article# is in the range
    ** Low <= art# <= High.  If art# > High, use a zero CAFTOCENT.  This means
    ** that in cases where the CAF file is inconsistent due to a crash ---
    ** the CAFTOCENT shows an article as being existent, but the header 
    ** doesn't show that article as being in the currently valid range ---
    ** the header value "wins" and we assume the article does not exist.
    ** This avoids problems with "half-existent" articles that showed up
    ** in the CAF TOC, but were never picked up by ctlinnd renumber '' . 
    */
    /* (Note: We already checked above that art >= head.Low.) */

    if (art > head.High) {
	/* clear the tocent */
	memset(&tocent, 0, sizeof(tocent));
    } else {
	if (CAFGetTOCEnt(fd, &head, art, &tocent) < 0) {
	    close(fd);
	    return -1;
	}
    }

    if (tocent.Size != 0) {
	/* article is already here */
	CAFError(CAF_ERR_ARTALREADYHERE);
	close(fd);
	return -1;
    }

    startoffset = 0;
    freebm = NULL;

    if (size != 0 && (freebm = CAFReadFreeBM(fd, &head)) != NULL) {
	numblks = (size + head.BlockSize - 1) / head.BlockSize;
	startoffset = CAFFindFreeBlocks(freebm, fd, numblks);
	if (startoffset == 0) {
	    CAFDisposeBitmap(freebm);
	    freebm = NULL;
	}
    }

    if (startoffset == 0) {
	/*
	** No size given or free space not available, so
	** seek to EOF to prepare to start writing article.
	*/

	if ((offset = lseek(fd, 0, SEEK_END)) < 0) {
	    CAFError(CAF_ERR_IO);
	    close(fd);
	    return -1;
	}
	/* and round up offset to a block boundary. */
	startoffset = CAFRoundOffsetUp(offset, head.BlockSize);
    }

    /* Seek to starting offset for the new artiicle. */
    if (lseek(fd, startoffset, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	close(fd);
	return -1;
    }
    
    /* stash data for FinishArtWrite's use. */
    CAF_fd_write = fd;
    CAF_artnum_write = art;
    CAF_startoffset_write = startoffset;
    CAF_header_write = head;
    CAF_free_bitmap_write = freebm;
    CAF_numblks_write = numblks;

    return fd;
}

/* 
** write out TOC entries for the previous article.  Note that we do *not*
** (as was previously done) close the fd; this allows reuse of the fd to write
** another article to this CAF file w/o an (soemwhat expensive) open().
*/

int
CAFFinishArtWrite(int fd)
{
    off_t curpos;
    CAFTOCENT tocentry;
    off_t curblk;
    CAFHEADER *headp;
    unsigned int i;

    /* blah, really should handle multiple pending OpenArtWrites. */
    if (fd != CAF_fd_write) {
	fprintf(stderr, "CAF: fd mismatch in CloseArtWrite.\n");
	abort();
    }

    headp = &CAF_header_write;

    /* Find out where we left off writing in the file. */
    if ((curpos = lseek(fd, 0, SEEK_CUR)) < 0) {
	CAFError(CAF_ERR_IO);
	CAF_fd_write = 0;
	return -1;
    }

    /* Write the new TOC entry. */
    if (CAFSeekTOCEnt(fd, headp, CAF_artnum_write) < 0) {
	CAF_fd_write = 0;
	return -1;
    }
    tocentry.Offset = CAF_startoffset_write;
    tocentry.Size = curpos - CAF_startoffset_write;
    tocentry.ModTime = time((time_t *)NULL);
    if (OurWrite(fd, &tocentry, sizeof(CAFTOCENT)) < 0) {
	CAF_fd_write = 0;
	return -1;
    }

    /* if needed, update free bitmap. */
    if (CAF_free_bitmap_write != NULL) {
	/* Paranoia: check to make sure we didn't write more than we said we would. */
	if (tocentry.Size > CAF_numblks_write * headp->BlockSize) {
	    /*
	    ** for now core dump (might as well, if we've done this the CAF
	    ** file is probably thoroughly hosed anyway.) 
	    */
	    fprintf(stderr, "CAF: article written overran declared size.\n");
	    abort();
	}

	curblk = CAF_startoffset_write;

	for (i = 0 ; i < CAF_numblks_write ; ++i, curblk += headp->BlockSize) {
	    CAFSetBlockFree(CAF_free_bitmap_write, fd,  curblk, 0);
	}
	if (CAFWriteFreeBM(fd, CAF_free_bitmap_write) < 0){
	    CAFError(CAF_ERR_IO);
	    CAF_fd_write = 0;
	    return -1;
	}
	CAFDisposeBitmap(CAF_free_bitmap_write);
	/* and update the Free value in the header. */
	headp->Free -= CAF_numblks_write * headp->BlockSize;
    }	    

    if (CAF_artnum_write > headp->High || CAF_free_bitmap_write) {
	/* need to update header. */
	if (CAF_artnum_write > headp->High) {
	    headp->High = CAF_artnum_write;
	}
	if (lseek(fd, 0, SEEK_SET) < 0) {
	    CAFError(CAF_ERR_IO);
	    CAF_fd_write = 0;
	    return -1;
	}
	if (OurWrite(fd, headp, sizeof(CAFHEADER)) < 0) {
	    CAF_fd_write = 0;
	    return -1;
	}
    }
#if 0
    if (close(fd) < 0) {
	CAFError(CAF_ERR_IO);
	CAF_fd_write =0;
	return -1;
    }
#endif
    CAF_fd_write = 0;
    return 0;
}

/*
** return a string containing a description of the error.
** Warning: uses a static buffer, or possibly a static string.
*/

static char errbuf[512];

const char *
CAFErrorStr(void)
{
    if (caf_error == CAF_ERR_IO || caf_error == CAF_ERR_CANTCREATECAF) {
	snprintf(errbuf, sizeof(errbuf), "%s errno=%s\n",
		(caf_error == CAF_ERR_IO) ? "CAF_ERR_IO" : "CAF_ERR_CANTCREATECAF",
		strerror(errno));
	return errbuf;
    } else {
	switch(caf_error) {
	  case CAF_ERR_BADFILE:
	    return "CAF_ERR_BADFILE";
	  case CAF_ERR_ARTNOTHERE:
	    return "CAF_ERR_ARTNOTHERE";
	  case CAF_ERR_FILEBUSY:
	    return "CAF_ERR_FILEBUSY";
	  case CAF_ERR_ARTWONTFIT:
	    return "CAF_ERR_ARTWONTFIT";
	  case CAF_ERR_ARTALREADYHERE:
	    return "CAF_ERR_ARTALREADYHERE";
	  case CAF_ERR_BOGUSPATH:
	    return "CAF_ERR_BOGUSPATH";
	  default:
	    snprintf(errbuf, sizeof(errbuf), "CAF error %d", caf_error);
	    return errbuf;
	}
    }
}

/*
** Open a CAF file, snarf the TOC entries for all the articles inside,
** and close the file.  NOTE: returns the header for the CAF file in
** the storage pointed to by *ch.  Dynamically allocates storage for
** the TOC entries, which should be freed by the caller when the
** caller's done with it.  Return NULL on failure.
**
** This function calls CAFOpenReadTOC(dir, ch, &tocp), which does most 
** (practically all) of the dirty work.  CAFOpenReadTOC leaves the fd open
** (and returns it); this is needed by cafls.   CAFReadTOC() closes the fd 
** after CAFOpenReadTOC() is done with it.
*/

CAFTOCENT *
CAFReadTOC(char *path, CAFHEADER *ch)
{
    CAFTOCENT *tocp;
    int fd;

    if ((fd = CAFOpenReadTOC(path, ch, &tocp)) < 0) {
	return NULL; /* some sort of error happened */
    }

    close(fd);
    return tocp;
}

int
CAFOpenReadTOC(char *path, CAFHEADER *ch, CAFTOCENT **tocpp)
{
    int fd;
    int nb;
    CAFTOCENT *tocp;
    off_t offset;

    if ( (fd = open(path, O_RDONLY)) < 0) {
	/* 
	** if ENOENT (not there), just call this "article not found",
	** otherwise it's a more serious error and stash the errno.
	*/
	if (errno == ENOENT) {
	    CAFError(CAF_ERR_ARTNOTHERE);
	} else {
	    CAFError(CAF_ERR_IO);
	}
	return -1;
    }

    /* Fetch the header */
    if (CAFReadHeader(fd, ch) < 0) {
	close(fd);
	return -1;
    }

    /* Allocate memory for TOC. */
    tocp = xmalloc((ch->High - ch->Low + 1) * sizeof(CAFTOCENT));
    nb = (sizeof(CAFTOCENT))*(ch->High - ch->Low + 1); /* # bytes to read for toc. */

    /* seek to beginning of TOC */
    offset = sizeof(CAFHEADER) + ch->FreeZoneTabSize;

    if (lseek(fd, offset, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }

    if (OurRead(fd, tocp, nb) < 0) {
	return -1;
    }

    /* read TOC successfully, return fd and stash tocp where we were told to */
    *tocpp = tocp;
    return fd;
}


/*
** Cancel/expire articles from a CAF file.  This involves zeroing the Size
** field of the TOC entry, and updating the Free field of the CAF header.
** note that no disk space is actually freed by this process; space will only
** be returned to the OS when the cleaner daemon runs on the CAF file.
*/

int
CAFRemoveMultArts(char *path, unsigned int narts, ARTNUM *artnums)
{
    int fd;
    CAFHEADER head;
    CAFTOCENT tocent;
    CAFBITMAP *freebitmap;
    ARTNUM art;
    unsigned int numblksfreed, i, j;
    off_t curblk;
    int errorfound = false;

    while (true) {
	/* try to open the file and lock it */
	if ((fd = open(path, O_RDWR)) < 0) {
	    /* if ENOENT, CAF file isn't there, so return ARTNOTHERE, otherwise it's an I/O error. */
	    if (errno != ENOENT) {
		CAFError(CAF_ERR_IO);
		return -1;
	    } else {
		CAFError(CAF_ERR_ARTNOTHERE);
		return -1;
	    }
	}
	/* try a nonblocking lock attempt first. */
	if (inn_lock_file(fd, INN_LOCK_WRITE, false)) break;

	/* wait around to try and get a lock. */
	inn_lock_file(fd, INN_LOCK_WRITE, true);
	/*
        ** and then close and reopen the file, in case someone changed the
	** file out from under us.
	*/ 
	close(fd);
    }
    /* got the file, open for write and locked. */
    /* Fetch the header */
    if (CAFReadHeader(fd, &head) < 0) {
	close(fd);
	return -1;
    }

    if ((freebitmap = CAFReadFreeBM(fd, &head)) == NULL) {
	close(fd);
	return -1;
    }

    for (j = 0 ; j < narts ; ++j) {
	art = artnums[j];

	/* Is the requested article even in the file? */
	if (art < head.Low || art > head.High) {
	    CAFError(CAF_ERR_ARTNOTHERE);
	    errorfound = true; 
	    continue; /* don't abandon the whole remove if just one art is missing */
	}

	if (CAFGetTOCEnt(fd, &head, art, &tocent) < 0) {
	    close(fd);
	    CAFDisposeBitmap(freebitmap);
	    return -1;
	}

	if (tocent.Size == 0) {
	    CAFError(CAF_ERR_ARTNOTHERE);
	    errorfound = true; 
	    continue; /* don't abandon the whole remove if just one art is missing */
	}

	numblksfreed = (tocent.Size + head.BlockSize - 1) / head.BlockSize;

	/* Mark all the blocks as free. */
	for (curblk = tocent.Offset, i = 0 ; i < numblksfreed; ++i, curblk += head.BlockSize) {
	    CAFSetBlockFree(freebitmap, fd, curblk, 1);
	}
	/* Note the amount of free space added. */
	head.Free += numblksfreed * head.BlockSize;
	/* and mark the tocent as a deleted entry. */
	tocent.Size = 0;

	if (CAFSeekTOCEnt(fd, &head, art) < 0) {
	    close(fd);
	    CAFDisposeBitmap(freebitmap);
	    return -1;
	}

	if (OurWrite(fd, &tocent, sizeof(CAFTOCENT)) < 0) {
	    close(fd);
	    CAFDisposeBitmap(freebitmap);
	    return -1;
	}
    }

    if (CAFWriteFreeBM(fd, freebitmap) < 0) {
	close(fd);
	CAFDisposeBitmap(freebitmap);
	return -1;
    }
    /* dispose of bitmap storage. */
    CAFDisposeBitmap(freebitmap);

    /* need to update header. */
    if (lseek(fd, 0, SEEK_SET) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }
    if (OurWrite(fd, &head, sizeof(CAFHEADER)) < 0) {
	return -1;
    }

    if (close(fd) < 0) {
	CAFError(CAF_ERR_IO);
	return -1;
    }

    if (CAFClean(path, 0, 10.0) < 0) errorfound=true;

    return errorfound ? -1 : 0;
}

/* 
** Do a fake stat() of a CAF-stored article.  Both 'inpaths' and 'innfeed'
** find this functionality useful, so we've added a function to do this.
** Caveats: not all of the stat structure is filled in, only these items:
**  st_mode, st_size, st_atime, st_ctime, st_mtime.  (Note: 
**  atime==ctime==mtime always, as we don't track times of CAF reads.)
*/

int
CAFStatArticle(char *path, ARTNUM art, struct stat *stbuf)
{
    CAFHEADER head;
    int fd;
    CAFTOCENT tocent;

    if ( (fd = open(path, O_RDONLY)) < 0) {
	/* 
	** if ENOENT (not there), just call this "article not found",
	** otherwise it's a more serious error and stash the errno.
	*/
	if (errno == ENOENT) {
	    CAFError(CAF_ERR_ARTNOTHERE);
	} else {
	    CAFError(CAF_ERR_IO);
	}
	return -1;
    }

    /* Fetch the header */
    if (CAFReadHeader(fd, &head) < 0) {
	close(fd);
	return -1;
    }

    /* Is the requested article even in the file? */
    if (art < head.Low || art > head.High) {
	CAFError(CAF_ERR_ARTNOTHERE);
	close(fd);
	return -1;
    }

    if (CAFGetTOCEnt(fd, &head, art, &tocent) < 0) {
	close(fd);
	return -1;
    }

    if (tocent.Size == 0) {
	/* empty/otherwise not present article */
	CAFError(CAF_ERR_ARTNOTHERE);
	close(fd);
	return -1;
    }

    /* done with file, can close it. */
    close(fd);

    memset(stbuf, 0, sizeof(struct stat));
    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_size = tocent.Size;
    stbuf->st_atime = stbuf->st_ctime = stbuf->st_mtime = tocent.ModTime;
    return 0;
}

/* 
** Taken from the old 'cafclean' program.
** Function to clean a single CAF file. 
** Possibly the ugliest function I've ever written in my life. 
*/
/*
** We try to keep the total TOC size this many times larger than the actual 
** amount of TOC data in use so as not to have to reclean or compact the TOC
** so often.
*/
#define TOC_CLEAN_RATIO 10
/*
** ditto, but for compacting, we want to force a compacting if the High art#
** wanders into the top nth of the TOC slots.
*/
#define TOC_COMPACT_RATIO 5

int
CAFClean(char *path, int verbose, double PercentFreeThreshold)
{
    char *newpath;
    size_t pathlen;
    CAFHEADER head, newhead;
    int fdin, fdout;
    ARTNUM newlow;
    ARTNUM i;
    CAFTOCENT *tocarray, *tocp;
    CAFTOCENT *newtocarray, *newtocp;
    size_t newtocsize;
    FILE *infile, *outfile;
    off_t startoffset, newstartoffset;
    char  buf[BUFSIZ];
    int   nbytes;
    size_t ncur;
    int   n;
    unsigned int blocksize;
    char *zerobuff;
    struct stat statbuf;
    size_t datasize;
    double percentfree;
    int toc_needs_expansion;
    int toc_needs_compacting;

#ifdef STATFUNCT
    struct STATSTRUC fsinfo;
    unsigned long num_diskblocks_needed;
#endif

    /* allocate buffer for newpath */
    pathlen = strlen(path) + 10;
    newpath = xmalloc(pathlen);
    while (true) {
	/* try to open the file and lock it. */
	if ((fdin = open(path, O_RDWR)) < 0) {
	    /*
	    ** if ENOENT, obviously no CAF file is here, so just return,
	    ** otherwise report an error.
	    */
	    if (errno != ENOENT) {
	        CAFError(CAF_ERR_IO);
		return -1;
	    } else {
		return 0;
	    }
	}

	/* try a nonblocking lock attempt first. */
	if (inn_lock_file(fdin, INN_LOCK_WRITE, false)) break;

	/* wait around to try and get a lock. */
	inn_lock_file(fdin, INN_LOCK_WRITE, true);
	/*
        ** and then close and reopen the file, in case someone changed the
	** file out from under us.
	*/ 
	close(fdin);
    }

    /* got the file, open for write and locked. */
    /* Fetch the header */
    if (CAFReadHeader(fdin, &head) < 0) {
	close(fdin);
	return -1;
    }

    /* Stat the file to see how big it is */
    if (fstat(fdin, &statbuf) < 0) {
	close(fdin);
	CAFError(CAF_ERR_IO);
	perror(path);
	return -1;
    }
    
    /* compute amount of  actual data in file. */
    datasize = statbuf.st_size - head.StartDataBlock;
    if (datasize <= 0) {
	/* nothing in the file, set percentfree==0 so won't bother cleaning */
	percentfree = 0;
    } else {
	percentfree = (100.0 * head.Free) / datasize;
    }

    /*
    ** Grumble, we need to read the TOC now even before we clean, just so 
    ** we can decide if a clean or a compaction is needed.
    */

    lseek(fdin, 0L, SEEK_SET);
	
    /* make input file stdio-buffered. */
    if ((infile = fdopen(fdin, "r+")) == NULL) {
	CAFError(CAF_ERR_IO);
	close(fdin);
	return -1;
    }

    /* Allocate memory for TOC. */
    tocarray = xmalloc((head.High - head.Low + 1) * sizeof(CAFTOCENT));

    fseeko(infile, (off_t) (sizeof(CAFHEADER) + head.FreeZoneTabSize),
           SEEK_SET);

    n = fread(tocarray, sizeof(CAFTOCENT), (head.High - head.Low + 1), infile);
    if (n < 0) {
	CAFError(CAF_ERR_IO);
	fclose(infile);
	free(tocarray);
	free(newpath);
	return -1;
    }

    if ((unsigned long) n < (head.High - head.Low +1)) {
	CAFError(CAF_ERR_BADFILE);
	fclose(infile);
	free(tocarray);
	free(newpath);
	return -1;
    }
    
    /* Scan to see what the new lower bound for CAF file should be. */
    newlow = head.High + 1;

    for (tocp = tocarray, i = head.Low; i <= head.High; ++tocp, ++i) {
	if (tocp->Size != 0) {
	    newlow = i;
	    break;
	}
    }

    /* 
    ** if newlow is head.High+1, the TOC is completely empty and we can
    ** just remove the entire file. 
    */
    if (newlow == head.High + 1) {
        unlink(path);
	fclose(infile);
	free(tocarray);
	free(newpath);
	return 0;
    }

    /*
    ** Ah. NOW we get to decide if we need a clean!
    ** Clean if either 
    **   1) the absolute freespace threshold is crossed
    **   2) the percent free threshold is crossed.
    **   3) The CAF TOC is over 10% full (assume it needs to be expanded,
    **      so we force a clean)
    ** Note that even if we do not need a clean, we may need a compaction 
    ** if the high article number is in the top nth of the TOC.
    */

    toc_needs_expansion = 0;
    if ( (head.High - newlow) >= head.NumSlots/TOC_CLEAN_RATIO) {
	toc_needs_expansion = 1;
    }

    toc_needs_compacting = 0;
    if ( (head.Low + head.NumSlots - head.NumSlots/TOC_COMPACT_RATIO) <= head.High) {
	toc_needs_compacting = 1;
    }

    if ( (percentfree < PercentFreeThreshold)
	&& (!toc_needs_expansion) ) {
	/* no cleaning, but do we need a TOC compaction ? */
	if (toc_needs_compacting) {
	    int delta;
	    CAFTOCENT *tocp2;

	    if (verbose) {
		printf("Compacting   %s: Free=%lu (%f%%)\n", path,
                       (unsigned long) head.Free, percentfree);
	    }

	    delta = newlow - head.Low;

	    /* slide TOC array down delta units. */
	    for (i = newlow, tocp = tocarray, tocp2 = tocarray+delta;
		 i <= head.High ;  ++i) {
		*tocp++ = *tocp2++;
	    }

	    head.Low = newlow;
	    /* note we don't set LastCleaned, this doesn't count a a clean. */
	    /* (XXX: do we need a LastCompacted as well? might be nice.) */

	    /*  write new header  on top of old */
	    fseeko(infile, 0, SEEK_SET);
	    if (fwrite(&head, sizeof(CAFHEADER), 1, infile) < 1) {
		CAFError(CAF_ERR_IO);
		free(tocarray);
		free(newpath);
		fclose(infile);
		return -1;
	    }
	    /*
	    ** this next fseeko might actually fail, because we have buffered
	    ** stuff that might fail on write.
	    */
	    if (fseeko(infile, sizeof(CAFHEADER) + head.FreeZoneTabSize,
		       SEEK_SET) < 0) {
		perror(path);
		free(tocarray);
		free(newpath);
		fclose(infile);
		unlink(newpath);
		return -1;
	    }
	    if (fwrite(tocarray, sizeof(CAFTOCENT), head.High - newlow + 1, infile) < head.High - newlow + 1
		|| fflush(infile) < 0) {
		CAFError(CAF_ERR_IO);
		free(tocarray);
		free(newpath);
		fclose(infile);
		return -1;
	    }
	    /* all done, return. */
	    fclose(infile);
	    free(tocarray);
	    free(newpath);
	    return 0;
	} else {
	    /* need neither full cleaning nor compaction, so return. */
	    if (verbose) {
		printf("Not cleaning %s: Free=%lu (%f%%)\n", path,
                       (unsigned long) head.Free, percentfree);
	    }
	    fclose(infile);
	    free(tocarray);
	    free(newpath);
	    return 0;
	}
    }

    /*
    ** If OS supports it, try to check for free space and skip this file if
    ** not enough free space on this filesystem.
    */
#ifdef STATFUNCT
    if (STATFUNCT(fdin, &fsinfo) >= 0) {
	/* compare avail # blocks to # blocks needed for current file. 
	** # blocks needed is approximately 
	** datasize/blocksize + (size of the TOC)/blocksize 
	** + Head.BlockSize/blocksize, but we need to take rounding 
	** into account. 
	*/
#define RoundIt(n) (CAFRoundOffsetUp((n), fsinfo.STATMULTI) / fsinfo.STATMULTI)

	num_diskblocks_needed = RoundIt((head.High - head.Low + 1)*sizeof(CAFTOCENT)) 
	    + RoundIt(datasize - head.Free) + RoundIt(head.BlockSize);
	if (num_diskblocks_needed > fsinfo.STATAVAIL) {
	    if (verbose) {
		printf("CANNOT clean %s: needs %lu blocks, only %lu avail.\n",
		       path, num_diskblocks_needed,
                       (unsigned long) fsinfo.f_bavail);
	    }
	    fclose(infile);
	    free(tocarray);
	    free(newpath);
	    return 0;
	}
    }
#endif    

    if (verbose) {
	printf("Am  cleaning %s: Free=%d (%f%%) %s\n", path, head.Free,
	       percentfree, toc_needs_expansion ? "(Expanding TOC)" : "");
    }

    /* decide on proper size for new TOC */
    newtocsize = CAF_DEFAULT_TOC_SIZE;
    if (head.High - newlow > newtocsize/TOC_CLEAN_RATIO) {
	newtocsize = TOC_CLEAN_RATIO*(head.High - newlow);
    }

    /* try to create new CAF file with some temp. pathname */
    /* note: new CAF file is created in flocked state. */
    if ((fdout = CAFCreateCAFFile(path, newlow, newtocsize,
                                  statbuf.st_size, 1, newpath, pathlen)) < 0) {
	fclose(infile);
	free(tocarray);
	free(newpath);
	return -1;
    }

    if ((outfile = fdopen(fdout, "w+")) == NULL) {
	CAFError(CAF_ERR_IO);
	fclose(infile);
	free(tocarray);
	unlink(newpath);
	free(newpath);
	return -1;
    }

    newtocarray = xcalloc((head.High - newlow + 1), sizeof(CAFTOCENT));

    if (fseeko(outfile, 0, SEEK_SET) < 0) {
	perror(newpath);
	free(tocarray);
	free(newtocarray);
	fclose(infile);
	fclose(outfile);
	unlink(newpath);
	free(newpath);
	return -1;
    }

    /* read in the CAFheader from the new file. */
    if (fread(&newhead, sizeof(CAFHEADER), 1, outfile) < 1) {
	perror(newpath);
	free(tocarray);
	free(newtocarray);
	fclose(infile);
	fclose(outfile);
	unlink(newpath);
	free(newpath);
	return -1;
    }

    /* initialize blocksize, zeroes buffer. */
    blocksize = newhead.BlockSize;
    if (blocksize == 0) blocksize=CAF_DEFAULT_BLOCKSIZE;

    zerobuff = xcalloc(blocksize, 1);

    /* seek to end of output file/place to start writing new articles */
    fseeko(outfile, 0, SEEK_END);
    startoffset = ftello(outfile);
    startoffset = CAFRoundOffsetUp(startoffset, blocksize);
    fseeko(outfile, (off_t) startoffset, SEEK_SET);

    /*
    ** Note: startoffset will always give the start offset of the next
    ** art to be written to the outfile.
    */

    /*
    ** Loop over all arts in old TOC, copy arts that are still here to new
    ** file and new TOC. 
    */

    for (tocp = tocarray, i = head.Low; i <= head.High; ++tocp, ++i) {
	if (tocp->Size != 0) {
	    newtocp = &newtocarray[i - newlow];
	    newtocp->Offset = startoffset;
	    newtocp->Size = tocp->Size;
	    newtocp->ModTime = tocp->ModTime;

	    /* seek to right place in input. */
	    fseeko(infile, (off_t) tocp->Offset, SEEK_SET); 

	    nbytes = tocp->Size;
	    while (nbytes > 0) {
		ncur = (nbytes > BUFSIZ) ? BUFSIZ : nbytes;
		if (fread(buf, sizeof(char), ncur, infile) < ncur 
		    || fwrite(buf, sizeof(char), ncur, outfile) < ncur) {
		    if (feof(infile)) {
			CAFError(CAF_ERR_BADFILE);
		    } else {
		        CAFError(CAF_ERR_IO);
		    }

		    errorexit:
		    fclose(infile);
		    fclose(outfile);
		    free(tocarray);
		    free(newtocarray);
		    free(zerobuff);
		    unlink(newpath);
		    free(newpath);
		    return -1;
		}
		nbytes -= ncur;
	    }
	    /* startoffset = ftello(outfile); */
	    startoffset += tocp->Size;
	    newstartoffset = CAFRoundOffsetUp(startoffset, blocksize);
	    /* fseeko(outfile, (off_t) startoffset, SEEK_SET); */
	    /* but we don't want to call fseeko, since that seems to always 
	       force a write(2) syscall, even when the new location would
	       still be inside stdio's buffer. */
	    if (newstartoffset - startoffset > 0) {
		ncur = newstartoffset - startoffset;
		if (fwrite(zerobuff, sizeof(char), ncur, outfile) < ncur) {
		    /* write failed, must be disk error of some sort. */
		    perror(newpath);
		    goto errorexit; /* yeah, it's a goto.  eurggh. */
		}
	    }
	    startoffset = newstartoffset;
	}
    }

    free(tocarray); /* don't need this guy anymore. */
    free(zerobuff);

    /* 
    ** set up new file header, TOC.
    ** this next fseeko might actually fail, because we have buffered stuff
    ** that might fail on write.
    */
    if (fseeko(outfile, 0, SEEK_SET) < 0) {
	perror(newpath);
	free(newtocarray);
	fclose(infile);
	fclose(outfile);
	unlink(newpath);
	free(newpath);
	return -1;
    }

    /* Change what we need in new file's header. */
    newhead.Low = newlow;
    newhead.High = head.High;
    newhead.LastCleaned = time((time_t *) NULL);
/*    newhead.NumSlots = newtocsize; */
/*    newhead.Free = 0; */

    if (fwrite(&newhead, sizeof(CAFHEADER), 1, outfile) < 1) {
	CAFError(CAF_ERR_IO);
	free(newtocarray);
	fclose(infile);
	fclose(outfile);
	unlink(newpath);
	free(newpath);
	return -1;
    }

    /*
    ** this next fseeko might actually fail, because we have buffered stuff
    ** that might fail on write.
    */
    if (fseeko(outfile, sizeof(CAFHEADER) + newhead.FreeZoneTabSize,
	       SEEK_SET) < 0) {
	perror(newpath);
	free(newtocarray);
	fclose(infile);
	fclose(outfile);
	unlink(newpath);
	free(newpath);
	return -1;
    }

    if (fwrite(newtocarray, sizeof(CAFTOCENT), head.High - newlow + 1, outfile) < head.High - newlow + 1
	|| fflush(outfile) < 0) {
	CAFError(CAF_ERR_IO);
	free(newtocarray);
	fclose(infile);
	fclose(outfile);
	unlink(newpath);
	free(newpath);
	return -1;
    }
    
    if (rename(newpath, path) < 0) {
	CAFError(CAF_ERR_IO);
	free(newtocarray);
	free(newpath);
	fclose(infile);
	fclose(outfile);
	/* if can't rename, probably no point in trying to unlink newpath, is there? */
	return -1;
    }
    /* written and flushed newtocarray, can safely fclose and get out of
       here! */
    free(newtocarray);
    free(newpath);
    fclose(outfile);
    fclose(infile);
    return 0;
}
