/*  $Id$
**
**  Interface to history API
*/

#ifndef INN_HISTORY_H
#define INN_HISTORY_H

#include <inn/defines.h>
#include <sys/types.h>

BEGIN_DECLS

/*
**  ensure appropriate scoping; we don't pull inn/storage.h as we
**  don't need; our caller then has the option
*/
struct history;
struct token;

/*
**  structure giving cache statistics returned from HISstats
*/
struct histstats {
    /* number of positive hits */
    int hitpos;
    /* number of negative hits */
    int hitneg;
    /* number of misses (positive hit, but not in cache) */
    int misses;
    /* number of does not exists (negative hit, but not in cache) */
    int dne;
};


/*
**  flags passed to HISopen
*/

/* open database read only */
#define HIS_RDONLY (0)

/* open database read/write */
#define HIS_RDWR (1<<0)

/* create on open */
#define HIS_CREAT (1<<1)

/* hint that the data should be kept on disk */
#define HIS_ONDISK (1<<2)

/* hint that the data should be kept in core */
#define HIS_INCORE (1<<3)

/* hint that the data should be kept mmap()ed */
#define HIS_MMAP (1<<4)

/*
**  values passed to HISctl
*/
enum {
    /* (char **) get history path */
    HISCTLG_PATH,

    /* (char *) set history path */
    HISCTLS_PATH,

    /* (int) how many history writes may be outstanding */
    HISCTLS_SYNCCOUNT,

    /* (size_t) number of pairs for which the database should be sized */
    HISCTLS_NPAIRS,

    /* (bool) Ignore old database during expire */
    HISCTLS_IGNOREOLD,
    
    /* (time_t) interval, in s, between stats of the history database
     * for * detecting a replacement, or 0 to disable (no checks);
     * defaults {hisv6, taggedhash} */
    HISCTLS_STATINTERVAL

};

struct history *        HISopen(const char *, const char *, int);
bool                    HISclose(struct history *);
bool                    HISsync(struct history *);
void                    HISsetcache(struct history *, size_t);
bool                    HISlookup(struct history *, const char *, time_t *,
				  time_t *, time_t *, struct token *);
bool                    HIScheck(struct history *, const char *);
bool                    HISwrite(struct history *, const char *, time_t,
				 time_t, time_t, const struct token *);
bool                    HISremember(struct history *, const char *, time_t);
bool                    HISreplace(struct history *, const char *, time_t,
				   time_t, time_t, const struct token *);
bool                    HISexpire(struct history *, const char *, const char *,
				  bool, void *, time_t,
				  bool (*)(void *, time_t, time_t, time_t,
					   struct token *));
bool                    HISwalk(struct history *, const char *, void *,
				bool (*)(void *, time_t, time_t, time_t,
					 const struct token *));
struct histstats        HISstats(struct history *);
const char *            HISerror(struct history *);
bool                    HISctl(struct history *, int, void *);
void                    HISlogclose(void);
void                    HISlogto(const char *s);

END_DECLS

#endif
