/* $Revision$
** Declarations needed for handling CAF (Crunched Article Files)
** Written by Richard Todd (rmtodd@mailhost.ecn.uoknor.edu) 3/24/96
*/


/*
** Format of a crunched article file:
** Header:
*/

typedef struct _CAFHEADER {
    char        Magic[4]; /* Magic Number "CRMT" */
    ARTNUM      Low; /* lowest article in the file */
    ARTNUM      NumSlots; /* number of articles there are room for in the TOC */
    ARTNUM      High; /* last article actually present in the file */
    size_t	Free; /* amount of space currently unused (freed by cancels/expires) */
    off_t       StartDataBlock; /* offset of first article data block. */
    unsigned int BlockSize; /* unit of allocation for CAF files. */
    size_t      FreeZoneTabSize; /* amount of space taken up by the free zone table. */
    size_t      FreeZoneIndexSize; /* size taken up by the "index" part of the free zone table. */
    time_t      LastCleaned; /* note time of last cleaning. */
    int         spare[3];
} CAFHEADER;

#define CAF_MAGIC "CRMT"
#define CAF_MAGIC_LEN 4
#define CAF_DEFAULT_BLOCKSIZE 512

/*
** then the table of free blocks.  The table is FreeZoneTabSize bytes
** long.  First comes a "first-level" or "index" bitmap, taking up the
** space from the end of the CAFHEADER to the end of the first
** block, i.e. FreeZoneIndexBytes. The rest of the table is a big  bitmap
** listing free blocks in the 'data' portion of the CAF file.
**
** In the "index" bitmap: LSB of bitmap byte 0 is 1 if there are any 1s 
** (free blocks) listed in the first block of the big bitmap, and 0 if there
** are no 1s in that block.  The remaining bits of the index bitmap 
** correspond to the remaining blocks of the big bitmap accordingly.
** The idea is that from the index bitmap one can tell which part of the 
** main bitmap is likely to have free blocks w/o having to read the entire 
** main bitmap.
**
** As for the main bitmap, each bit is 1 if the corresponding data
** block (BlockSize bytes) is free.  LSB of bitmap byte 0 corresponds
** to the block @ offset StartDataBlock, and all the rest follow on
** accordingly.  
**
** Note that the main part of the bitmap is *always* FreeZoneIndexByte*8
** blocks long, no matter how big the CAF file is.  The table of free blocks
** is almost always sparse.  Also note that blocks past EOF in the CAF file
** are *not* considered free.  If the CAF article write routines fail to 
** find free space in the fre block bitmaps, they will always attempt to 
** extend the CAF file instead. 
*/

#define CAF_DEFAULT_FZSIZE (512-sizeof(CAFHEADER))

/*
** (Note: the CAFBITMAP structure isn't what's actually stored on disk
** in the free bitmap region, this is just a convenient structure to
** keep the bitmap size and StartBlockOffset together with the bitmap
** w/o having keep passing the original CAFHEADER to every routine
** that wants it.  The bitmap structure contains the first (index) bitmap,
** as well as pointers to structures for each block of the main bitmap that
** has been read into memory.
*/

typedef struct _CAFBITMAP {
    off_t StartDataBlock;
    off_t MaxDataBlock; /* can only handle offsets < this with this bitmap. */
    size_t FreeZoneTabSize;
    size_t FreeZoneIndexSize;
    size_t BytesPerBMB; /* size of chunk, in bytes, that any given BMBLK can map. */
    unsigned int BlockSize;
    unsigned int NumBMB; /* size of Blocks array. */
    struct _CAFBMB **Blocks;
    char *Bits;
} CAFBITMAP;

typedef struct _CAFBMB {
    off_t StartDataBlock;
    off_t MaxDataBlock;
    int Dirty; /* 1 if this BMB has had any bits changed. */
    char *BMBBits;
} CAFBMB;

/* 
** Next in the file are the TOC (Table of Contents) entries.  Each TOC
** entry describes an article. 
*/

typedef struct _CAFTOCENT {
    off_t  Offset;
    size_t Size;
    time_t ModTime;
} CAFTOCENT;

/*
** and then after the NumSlots TOC Entries, the actual articles, one after
** another, always starting at offsets == 0 mod BlockSize
*/

/*
** Number of slots to put in TOC by default.  Can be raised if we ever get 
** more than 256K articles in a newsgroup (frightening thought).
*/

#define CAF_DEFAULT_TOC_SIZE (256 * 1024)

/*
** Default name for CAF file in the news spool dir for a given newsgroup.
*/
#define CAF_NAME "CF"

extern int CAFOpenArtRead(const char *cfpath, ARTNUM art, size_t *len);
extern int CAFOpenArtWrite(char *cfpath, ARTNUM *art, int WaitLock, size_t size);
extern int CAFStartWriteFd(int fd, ARTNUM *art, size_t size);
extern int CAFFinishWriteFd(int fd);
extern int CAFFinishArtWrite(int fd);
extern int CAFCreateCAFFile(char *cfpath, ARTNUM lowart, ARTNUM tocsize, size_t cfsize, int nolink, char *temppath, size_t pathlen);
extern const char *CAFErrorStr(void);
extern CAFTOCENT *CAFReadTOC(char *cfpath, CAFHEADER *ch);
extern int CAFRemoveMultArts(char *cfpath, unsigned int narts, ARTNUM *arts);
extern int CAFStatArticle(char *path, ARTNUM art, struct stat *st);

#ifdef CAF_INNARDS
/* functions used internally by caf.c, and by the cleaner program, and cafls
   but probably aren't useful/desirable to be used by others. */
extern int CAFOpenReadTOC(char *cfpath, CAFHEADER *ch, CAFTOCENT **tocpp);
extern int CAFReadHeader(int fd, CAFHEADER *h);
extern off_t CAFRoundOffsetUp(off_t offt, unsigned int bsize);
extern CAFBITMAP * CAFReadFreeBM(int fd, CAFHEADER *h);
extern void CAFDisposeBitmap(CAFBITMAP *cbm);
/*
** note! CAFIsBlockFree needs the fd, since blocks of the free bitmap may 
** need to be fetched from disk.
*/
extern int CAFIsBlockFree(CAFBITMAP *bm, int fd, off_t block);
#endif

extern int caf_error; /* last error encountered by library. */
extern int caf_errno; /* saved value of errno here if I/O error hit by lib. */

#define CAF_ERR_IO 1 		/* generic I/O error, check caf_errno for details */
#define CAF_ERR_BADFILE 2 	/* corrupt file */
#define CAF_ERR_ARTNOTHERE 3 	/* article not in the database */
#define	CAF_ERR_CANTCREATECAF 4 /* can't create the CAF file, see errno. */
#define CAF_ERR_FILEBUSY 5      /* file locked by someone else. */
#define CAF_ERR_ARTWONTFIT 6 	/* outside the range in the TOC */
#define CAF_ERR_ARTALREADYHERE 7 /* tried to create an article that was already here. */
#define CAF_ERR_BOGUSPATH 8	/* pathname not parseable. */
