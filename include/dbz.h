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
#define DBZ_INTERNAL_HASH_SIZE   6

typedef enum {INCORE_NO, INCORE_MEM, INCORE_MMAP} dbz_incore_val;

typedef struct {
    /* Whether to write to the filesystem in addition to updating the incore
       copy.  This will replace a single large write to disk when dbzsync is
       called.  */
    BOOL             writethrough;
    /* Whether to do hash lookups from disk, memory or a mmap'ed file */
    dbz_incore_val   idx_incore;      
    dbz_incore_val   exists_incore;
    /* Whether dbzstore should update the database async or sync.  This
       is only applicable if you're not mmaping the database */
    BOOL             nonblock;
} dbzoptions;

/* standard dbm functions */
extern BOOL dbminit(const char *name);
extern BOOL dbmclose(void);

/* new stuff for dbz */
extern BOOL dbzfresh(const char *name, const long size, const int fillpercent);
extern BOOL dbzagain(const char *name, const char *oldname);
extern BOOL dbzexists(const HASH key);
extern OFFSET_T dbzfetch(const HASH key);
extern BOOL dbzstore(const HASH key, const off_t data);
extern BOOL dbzsync(void);
extern long dbzsize(const long contents);
extern BOOL dbzdebug(const int value);
extern void dbzsetoptions(const dbzoptions options);
extern void dbzgetoptions(dbzoptions *options);

#endif /* __DBZ_H__ */
