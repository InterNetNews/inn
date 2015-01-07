#ifndef INN_DBZ_H
#define INN_DBZ_H 1

/* Need the definition of HASH. */
#include "inn/libinn.h"
#include "inn/portable-macros.h"

BEGIN_DECLS

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
#endif

typedef enum {DBZSTORE_OK, DBZSTORE_EXISTS, DBZSTORE_ERROR} DBZSTORE_RESULT;
typedef enum {INCORE_NO, INCORE_MEM, INCORE_MMAP} dbz_incore_val;

typedef struct {
    /* Whether to write to the filesystem in addition to updating the incore
       copy.  This will replace a single large write to disk when dbzsync is
       called.  */
    bool             writethrough;
    /* Whether to do hash lookups from disk, memory or a mmap'ed file */
    dbz_incore_val   pag_incore;
    dbz_incore_val   exists_incore;
    /* Whether dbzstore should update the database async or sync.  This
       is only applicable if you're not mmaping the database */
    bool             nonblock;
} dbzoptions;

#if !defined(lint) && (defined(__SUNPRO_C) || defined(_nec_ews))
#pragma pack(1)
#endif /* nor lint, nor __SUNPRO_C, nor sgi, nor _nec_ews */
/* Leave the __attribute__ ((__packed__)) on there because removing it
 * might change the layout of the data structure on disk on some platform,
 * thus invalidating old history files.
 * It is fairly unlikely that this is a problem, though.
 */
typedef struct {
    char		hash[DBZ_INTERNAL_HASH_SIZE];
} __attribute__ ((__packed__)) erec;
#if !defined(lint) && (defined(__SUNPRO_C) || defined(_nec_ews))
#pragma pack()
#endif /* nor lint, nor__SUNPRO_C, nor _nec_ews */

/* standard dbm functions */
extern bool dbzinit(const char *name);
extern bool dbzclose(void);

/* new stuff for dbz */
extern bool dbzfresh(const char *name, off_t size);
extern bool dbzagain(const char *name, const char *oldname);
extern bool dbzexists(const HASH key);
extern bool dbzfetch(const HASH key, off_t *value);
extern DBZSTORE_RESULT dbzstore(const HASH key, off_t data);
extern bool dbzsync(void);
extern long dbzsize(off_t contents);
extern void dbzsetoptions(const dbzoptions options);
extern void dbzgetoptions(dbzoptions *options);

END_DECLS

#endif /* INN_DBZ_H */
