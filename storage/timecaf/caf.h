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
    SIZE_T	Free; /* amount of space currently unused (freed by cancels/expires) */
    OFFSET_T    StartDataBlock; /* offset of first article data block. */
    unsigned int BlockSize; /* unit of allocation for CAF files. */
    SIZE_T      FreeZoneTabSize; /* amount of space taken up by the free zone table. */
    SIZE_T      FreeZoneIndexSize; /* size taken up by the "index" part of the free zone table. */
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
    OFFSET_T StartDataBlock;
    OFFSET_T MaxDataBlock; /* can only handle offsets < this with this bitmap. */
    SIZE_T FreeZoneTabSize;
    SIZE_T FreeZoneIndexSize;
    SIZE_T BytesPerBMB; /* size of chunk, in bytes, that any given BMBLK can map. */
    unsigned int BlockSize;
    unsigned int NumBMB; /* size of Blocks array. */
    struct _CAFBMB **Blocks;
    char *Bits;
} CAFBITMAP;

typedef struct _CAFBMB {
    OFFSET_T StartDataBlock;
    OFFSET_T MaxDataBlock;
    int Dirty; /* 1 if this BMB has had any bits changed. */
    char *BMBBits;
} CAFBMB;

/* 
** Next in the file are the TOC (Table of Contents) entries.  Each TOC
** entry describes an article. 
*/

typedef struct _CAFTOCENT {
    OFFSET_T Offset;
    SIZE_T   Size;
    time_t   ModTime;
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

#ifdef __STDC__
#define PROTO(x) x
#else
#define PROTO(x) ()
#endif

extern int CAFOpenArtRead PROTO((char *cfpath, ARTNUM art, SIZE_T *len));
extern int CAFOpenArtWrite PROTO((char *cfpath, ARTNUM *art, int WaitLock, SIZE_T size));
extern int CAFStartWriteFd PROTO((int fd, ARTNUM *art, SIZE_T size));
extern int CAFFinishWriteFd PROTO((int fd));
extern int CAFFinishArtWrite PROTO((int fd));
extern int CAFCreateCAFFile PROTO((char *cfpath, ARTNUM lowart, ARTNUM tocsize, SIZE_T cfsize, int nolink, char *temppath));
extern char *CAFErrorStr PROTO((void));
extern CAFTOCENT *CAFReadTOC PROTO((char *cfpath, CAFHEADER *ch));
extern int CAFRemoveMultArts PROTO((char *cfpath, unsigned int narts, ARTNUM *arts));
extern int CAFStatArticle PROTO((char *path, ARTNUM art, struct stat *st));

#ifdef CAF_INNARDS
/* functions used internally by caf.c, and by the cleaner program, and cafls
   but probably aren't useful/desirable to be used by others. */
extern int CAFOpenReadTOC PROTO((char *cfpath, CAFHEADER *ch, CAFTOCENT **tocpp));
extern int CAFReadHeader PROTO((int fd, CAFHEADER *h));
extern OFFSET_T CAFRoundOffsetUp PROTO((OFFSET_T offt, unsigned int bsize));
extern CAFBITMAP * CAFReadFreeBM PROTO((int fd, CAFHEADER *h));
extern void CAFDisposeBitmap PROTO((CAFBITMAP *cbm));
/*
** note! CAFIsBlockFree needs the fd, since blocks of the free bitmap may 
** need to be fetched from disk.
*/
extern int CAFIsBlockFree PROTO((CAFBITMAP *bm, int fd, OFFSET_T block));
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
