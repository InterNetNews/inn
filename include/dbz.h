#ifndef __DBZ_H__
#define __DBZ_H__

/* This is the number of bytes of the md5 to actually store in
 * the .pag file.  This number directly effects the collision
 * rate and memory usage.  Six bytes is a good number for a
 * five day remember as of 07/97.  If you're going to remember
 * much longer than this or have significantly more articles
 * then you can increase this number as hash as 16.  Performance is
 * only minimally impacted by this. (<10%)
 */
#define DBZ_HASH_SIZE   6

/* for dbm and dbz */
typedef struct {
	char *dptr;
	int dsize;
} datum;

typedef struct {
    unsigned char hash[DBZ_HASH_SIZE];
} hash_t;

/* standard dbm functions */
extern int dbminit(char *name);
extern int dbmclose();		/* in dbz, but not in old dbm */

/* new stuff for dbz */
extern hash_t dbzhash(char *value, int size);
extern int dbzfresh(char *name, long size);
extern int dbzagain();
extern datum dbzfetch();
extern int dbzstore();
extern int dbzsync();
extern long dbzsize();
extern int dbzincore();
extern int dbzdebug();
extern int dbzwritethrough();

/*
 * In principle we could handle unlimited-length keys by operating a chunk
 * at a time, but it's not worth it in practice.  Setting a nice large
 * bound on them simplifies the code and doesn't hurt anything.
 */
#define DBZMAXKEY	255

#endif /* __DBZ_H__ */
