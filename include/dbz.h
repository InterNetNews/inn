#ifndef __DBZ_H__
#define __DBZ_H__

/* This is the number of bytes of the md5 to actually store in
 * the .pag file.  This number directly effects the collision
 * rate and memory usage.  You can probably set this number as
 * low as 5 w/o problems and some sites may want to set it as
 * high as 8.  Anything higher than that is probably not useful.
 * Note at the internal hash size isn't the only factor that
 * effects collision rate.  The table index is used as an implicit
 * part of the hash value stored also.
 */
#ifdef	DO_TAGGED_HASH
#define DBZMAXKEY	255
#define DBZ_INTERNAL_HASH_SIZE   4
#else
#define DBZ_INTERNAL_HASH_SIZE   6
#define HISTOFFSET	0
#define OVEROFFSET	1
#endif

typedef enum {DBZSTORE_OK, DBZSTORE_EXISTS, DBZSTORE_ERROR} DBZSTORE_RESULT;
typedef enum {INCORE_NO, INCORE_MEM, INCORE_MMAP} dbz_incore_val;

typedef struct {
    /* Whether to write to the filesystem in addition to updating the incore
       copy.  This will replace a single large write to disk when dbzsync is
       called.  */
    BOOL             writethrough;
    /* Whether to do hash lookups from disk, memory or a mmap'ed file */
#ifdef	DO_TAGGED_HASH
    dbz_incore_val   pag_incore;
#else
    dbz_incore_val   idx_incore;
#endif
    dbz_incore_val   exists_incore;
    /* Whether dbzstore should update the database async or sync.  This
       is only applicable if you're not mmaping the database */
    BOOL             nonblock;
} dbzoptions;

#ifdef __GNUC__
#define PACKED __attribute__ ((packed))
#endif

#if defined(__SUNPRO_C) || defined(_nec_ews) || defined(sgi) || defined(sun) || defined(_HPUX_SOURCE)
#if !defined(lint) && defined(__SUNPRO_C)
#pragma pack(1)
#endif /* nor lint, nor sgi, nor _nec_ews */
typedef struct {
    char		hash[DBZ_INTERNAL_HASH_SIZE];
} erec;
typedef struct {
    OFFSET_T		offset;
} idxrec;
typedef struct {
    OFFSET_T		offset[2];
    unsigned char	overindex;
    unsigned char	reserved[3];
} idxrecext;
#if !defined(lint) && defined(__SUNPRO_C)
#pragma pack()
#endif /* nor lint, nor sgi, nor _nec_ews */
#else
typedef struct {
    char		hash[DBZ_INTERNAL_HASH_SIZE];
} PACKED erec;
typedef struct {
    OFFSET_T		offset;
} PACKED idxrec;
typedef struct {
    OFFSET_T		offset[2];
    unsigned char	overindex;
    unsigned char	reserved[3];
} PACKED idxrecext;
#endif

/* standard dbm functions */
extern BOOL dbzinit(const char *name);
extern BOOL dbzclose(void);

/* new stuff for dbz */
extern BOOL dbzfresh(const char *name, const long size, const int fillpercent);
extern BOOL dbzagain(const char *name, const char *oldname);
extern BOOL dbzexists(const HASH key);
#ifdef	DO_TAGGED_HASH
extern OFFSET_T dbzfetch(const HASH key);
extern DBZSTORE_RESULT dbzstore(const HASH key, const OFFSET_T data);
#else
extern BOOL dbzfetch(const HASH key, void *ivalue);
extern DBZSTORE_RESULT dbzstore(const HASH key, void *ivalue);
#endif
extern BOOL dbzsync(void);
extern long dbzsize(const long contents);
extern BOOL dbzdebug(const int value);
extern void dbzsetoptions(const dbzoptions options);
extern void dbzgetoptions(dbzoptions *options);

#endif /* __DBZ_H__ */
