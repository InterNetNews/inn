/*
  dbz.c  V4.0

Copyright 1988 Jon Zeeff (zeeff@b-tech.ann-arbor.mi.us)
You can use this code in any manner, as long as you leave my name on it
and don't hold me responsible for any problems with it.

Hacked on by gdb@ninja.UUCP (David Butler); Sun Jun  5 00:27:08 CDT 1988

Various improvments + INCORE by moraes@ai.toronto.edu (Mark Moraes)

Major reworking by Henry Spencer as part of the C News project.

Minor lint and CodeCenter (Saber) fluff removal by Rich $alz (March, 1991).
Non-portable CloseOnExec() calls added by Rich $alz (September, 1991).
Added "writethrough" and tagmask calculation code from
<rob@violet.berkeley.edu> and <leres@ee.lbl.gov> by Rich $alz (December, 1992).
Merged in MMAP code by David Robinson, formerly <david@elroy.jpl.nasa.gov>
now <david.robinson@sun.com> (January, 1993).

Major reworking by Clayton O'Neill (coneill@oneill.net).  Removed all the
C News and backwards compatible cruft.  Ripped out all the tagmask stuff
and replaced it with hashed .pag entries.  This totally removes the need
for base file access.  Primary bottleneck now appears to be the hash
algorithm and search().  With 5 million entries in the .pag file and a 6
byte hash, the chance of collision is about 1 in 40 million.  You can
change DBZ_HASH_SIZE in dbz.h to increase the size of the stored hash

These routines replace dbm as used by the usenet news software
(it's not a full dbm replacement by any means).  It's fast and
simple.  It contains no AT&T code.

In general, dbz's files are 1/20 the size of dbm's.  Lookup performance
is somewhat better, while file creation is spectacularly faster, especially
if the incore facility is used.

*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <dbz.h>
#include <md5.h>
#include <clibrary.h>

/*
 * "LIA" = "leave it alone unless you know what you're doing".
 *
 * NMEMORY	number of days of memory for use in sizing new table (LIA)
 * INCORE	backward compatibility with old dbz; use dbzincore() instead
 * NONBLOCK     enable non-blocking writes for non-mmapped i/o
 * DBZDEBUG	enable debugging
 * DEFSIZE	default table size (not as critical as in old dbz)
 * NPAGBUF	size of .pag buffer, in longs (LIA)
 * MAXRUN	length of run which shifts to next table (see below) (LIA)
 * MMAP		Use SunOS style mmap() for efficient incore
 * DBZTEST      Generate a standalone program for testing and benchmarking
 */

static int dbzversion = 5;	/* for validating .dir file format */

/*
 * The dbz database exploits the fact that when news stores a <key,value>
 * tuple, the `value' part is a seek offset into a text file, pointing to
 * a copy of the `key' part.  This avoids the need to store a copy of
 * the key in the dbz files.  However, the text file *must* exist and be
 * consistent with the dbz files, or things will fail.
 *
 * The basic format of the database is a simple hash table containing the
 * values.  A value is stored by indexing into the table using a hash value
 * computed from the key; collisions are resolved by linear probing (just
 * search forward for an empty slot, wrapping around to the beginning of
 * the table if necessary).  Linear probing is a performance disaster when
 * the table starts to get full, so a complication is introduced.  The
 * database is actually one *or more* tables, stored sequentially in the
 * .pag file, and the length of linear-probe sequences is limited.  The
 * search (for an existing item or an empty slot) always starts in the
 * first table, and whenever MAXRUN probes have been done in table N,
 * probing continues in table N+1.  This behaves reasonably well even in
 * cases of massive overflow.  There are some other small complications
 * added, see comments below.
 *
 * The table size is fixed for any particular database, but is determined
 * dynamically when a database is rebuilt.  The strategy is to try to pick
 * the size so the first table will be no more than 2/3 full, that being
 * slightly before the point where performance starts to degrade.  (It is
 * desirable to be a bit conservative because the overflow strategy tends
 * to produce files with holes in them, which is a nuisance.)
 */

/* Old dbz used a long as the record type for dbz entries, which became
 * really gross in places because of mixed references.  We define this to
 * make it a bit clearer.
 */

#ifdef __GNUC__
#define PACKED __attribute__ ((packed))
#else
#define PACKED 
#endif

typedef struct {
    unsigned long      offset;
    hash_t             hash;
} PACKED dbzrec;

static dbzrec empty_hash_t = { 0, { { 0, 0, 0, 0, 0, 0 } } };
#define	VACANT(x)	((x.offset == 0) && !memcmp(&empty_hash_t, &x.hash, sizeof(hash_t)))

/* A new, from-scratch database, not built as a rebuild of an old one,
 * needs to know table size.  Normally the user supplies this info,
 * but there have to be defaults.
 */
#ifndef DEFSIZE
#define	DEFSIZE	120011		/* 300007 might be better */
#endif

/*
 * We read configuration info from the .dir file into this structure,
 * so we can avoid wired-in assumptions for an existing database.
 *
 * Among the info is a record of recent peak usages, so that a new table
 * size can be chosen intelligently when rebuilding.  10 is a good
 * number of usages to keep, since news displays marked fluctuations
 * in volume on a 7-day cycle.
 */
#ifndef NMEMORY
#define	NMEMORY	10	/* # days of use info to remember */
#endif
#define	NUSEDS	(1+NMEMORY)

typedef struct {
    long tsize;		        /* table size */
    long used[NUSEDS];          /* entries used today, yesterday, ... */
    int valuesize;		/* size of table values, == sizeof(dbzrec) */
} dbzconfig;

static dbzconfig conf;
static int getconf(FILE *df, FILE *pf, dbzconfig *cp);
static long getno(FILE *f, int *ep);
static int putconf(FILE *f, dbzconfig *cp);
static datum fetch(datum key);

/*
 * Using mmap() is a more efficent way of keeping the .pag file incore.  On
 * average, it cuts the number of system calls and buffer copies in half.
 * It also allows one copy to be shared among many processes without
 * consuming any extra resources.
 */
#ifdef MMAP
#include <sys/mman.h>
#ifdef MAP_FILE
#define MAP__ARG	(MAP_FILE | MAP_SHARED)
#else
#define MAP__ARG	(MAP_SHARED)
#endif
#ifndef INCORE
#define INCORE
#endif
#endif

/* 
 * For a program that makes many, many references to the database, it
 * is a large performance win to keep the table in core, if it will fit.
 * Note that this does hurt robustness in the event of crashes, and
 * dbmclose() *must* be called to flush the in-core database to disk.
 * The code is prepared to deal with the possibility that there isn't
 * enough memory.  There *is* an assumption that a size_t is big enough
 * to hold the size (in bytes) of one table, so dbminit() tries to figure
 * out whether this is possible first.
 *
 * The preferred way to ask for an in-core table is to do dbzincore(1)
 * before dbminit().  The default is not to do it, although -DINCORE
 * overrides this for backward compatibility with old dbz.
 *
 * We keep only the first table in core.  This greatly simplifies the
 * code, and bounds memory demand.  Furthermore, doing this is a large
 * performance win even in the event of massive overflow.
 */
#ifdef INCORE
static int incore = 1;
#else
static int incore = 0;
#endif

/*
 * Write to filesystem even if incore?  This replaces a single multi-
 * megabyte write when doing a dbzsync with a multi-byte write each
 * time an article is added.  On most systems, this will give an overall
 * performance boost.
 */
static int writethrough = 0;

/*
 * Stdio buffer for .pag reads.  Buffering more than about 16 does not help
 * significantly at the densities we try to maintain, and the much larger
 * buffers that most stdios default to are much more expensive to fill.
 * With small buffers, stdio is performance-competitive with raw read(),
 * and it's much more portable.
 */
#ifndef NPAGBUF
#define	NPAGBUF	16
#endif

#ifdef _IOFBF
static char pagbuf[NPAGBUF*sizeof(dbzrec)];
#endif

/*
 * Data structure for recording info about searches.
 */
typedef struct {
    off_t place;		/* current location in file */
    int tabno;		        /* which table we're in */
    int run;		        /* how long we'll stay in this table */
#		ifndef MAXRUN
#		define	MAXRUN	100
#		endif
    hash_t hash;	        /* the key's hash code (for optimization) */
    unsigned int shorthash;     /* integer version of the hash */
    int aborted;		/* has i/o error aborted search? */
} searcher;
static void start(searcher *sp, datum *kp, searcher *osp);
#define	FRESH	((searcher *)NULL)
static dbzrec *search();
#define	NOTFOUND	(NULL)
static int set();

/*
 * Arguably the searcher struct for a given routine ought to be local to
 * it, but a fetch() is very often immediately followed by a store(), and
 * in some circumstances it is a useful performance win to remember where
 * the fetch() completed.  So we use a global struct and remember whether
 * it is current.
 */
static searcher srch;
static searcher *prevp;	/* &srch or FRESH */

/*
 * The double parentheses needed to make this work are ugly, but the
 * alternative (under most compilers) is to pack around 2K of unused
 * strings -- there's just no way to get rid of them.
 */
#ifdef DBZDEBUG
static int debug;			/* controlled by dbzdebug() */
#define DEBUG(args) if (debug) { (void) printf args ; } else
#else
#define	DEBUG(args)	;
#endif

/* externals used */
extern void CloseOnExec();

/* misc. forwards */
static char *mapcase(char *dest, char *src, size_t size);

/* file-naming stuff */
static char dir[] = ".dir";
static char pag[] = ".pag";
static char *enstring();

/* central data structures */
static FILE *dirf;		/* descriptor for .dir file */
static int dirronly;		/* dirf open read-only? */
static FILE *pagf = NULL;	/* descriptor for .pag file */
static int pagfd = -1;          /* non-blocking fd for .pag writes */
static off_t pagpos;		/* posn in pagf; only search may set != -1 */
static int pagronly;		/* pagf open read-only? */
static dbzrec *corepag;		/* incore version of .pag file, if any */
static FILE *bufpagf;		/* well-buffered pagf, for incore rewrite */
static dbzrec *getcore();
#ifndef MMAP
static int putcore();
#endif
static int written;		/* has a store() been done? */

/* dbzfresh - set up a new database, no historical info
 * Return 0 for success, -1 for failure
 * name - base name; .dir and .pag must exist
 * size - table size (0 means default)
 */
int dbzfresh(char *name, long size)
{
    char *fn;
    dbzconfig c;
    FILE *f;

    if (pagf != NULL) {
	DEBUG(("dbzfresh: database already open\n"));
	return(-1);
    }
    if (size != 0 && size < 2) {
	DEBUG(("dbzfresh: preposterous size (%ld)\n", size));
	return(-1);
    }

    /* get default configuration */
    if (getconf((FILE *)NULL, (FILE *)NULL, &c) < 0)
	return(-1);	/* "can't happen" */

    /* and mess with it as specified */
    if (size != 0)
	c.tsize = size;

    /* write it out */
    fn = enstring(name, dir);
    if (fn == NULL)
	return(-1);
    f = fopen(fn, "w");
    free((POINTER)fn);
    if (f == NULL) {
	DEBUG(("dbzfresh: unable to write config\n"));
	return(-1);
    }
    if (putconf(f, &c) < 0) {
	(void) fclose(f);
	return(-1);
    }
    if (fclose(f) == EOF) {
	DEBUG(("dbzfresh: fclose failure\n"));
	return(-1);
    }

    /* create/truncate .pag */
    fn = enstring(name, pag);
    if (fn == NULL)
	return(-1);
    f = fopen(fn, "w");
    free((POINTER)fn);
    if (f == NULL) {
	DEBUG(("dbzfresh: unable to create/truncate .pag file\n"));
	return(-1);
    } else
	(void) fclose(f);

    /* and punt to dbminit for the hard work */
    return(dbminit(name));
}

/*
 * dbzsize  - what's a good table size to hold this many entries?
 * contents - size of table (0 means return the default)
 */
long dbzsize(long contents) {
    long n;

    if (contents <= 0) {	/* foulup or default inquiry */
	DEBUG(("dbzsize: preposterous input (%ld)\n", contents));
	return(DEFSIZE);
    }
    n = (contents/2)*3;	/* try to keep table at most 2/3 full */
    DEBUG(("dbzsize: final size %ld\n", n));

    return(n);
}

/* dbzagain - set up a new database to be a rebuild of an old one
 * Returns 0 on success, -1 on failure
 * name - base name; .dir and .pag must exist
 * oldname - basename, all must exist
 */
int dbzagain(char *name, char *oldname)
{
    char *fn;
    dbzconfig c;
    int i;
    long top;
    FILE *f;
    int newtable;
    off_t newsize;

    if (pagf != NULL) {
	DEBUG(("dbzagain: database already open\n"));
	return(-1);
    }

    /* pick up the old configuration */
    fn = enstring(oldname, dir);
    if (fn == NULL)
	return(-1);
    f = fopen(fn, "r");
    free((POINTER)fn);
    if (f == NULL) {
	DEBUG(("dbzagain: cannot open old .dir file\n"));
	return(-1);
    }
    i = getconf(f, (FILE *)NULL, &c);
    (void) fclose(f);
    if (i < 0) {
	DEBUG(("dbzagain: getconf failed\n"));
	return(-1);
    }

    /* tinker with it */
    top = 0;
    newtable = 0;
    for (i = 0; i < NUSEDS; i++) {
	if (top < c.used[i])
	    top = c.used[i];
	if (c.used[i] == 0)
	    newtable = 1;	/* hasn't got full usage history yet */
    }
    if (top == 0) {
	DEBUG(("dbzagain: old table has no contents!\n"));
	newtable = 1;
    }
    for (i = NUSEDS-1; i > 0; i--)
	c.used[i] = c.used[i-1];
    c.used[0] = 0;
    newsize = dbzsize(top);
    if (!newtable || newsize > c.tsize)	/* don't shrink new table */
	c.tsize = newsize;

    /* write it out */
    fn = enstring(name, dir);
    if (fn == NULL)
	return(-1);
    f = fopen(fn, "w");
    free((POINTER)fn);
    if (f == NULL) {
	DEBUG(("dbzagain: unable to write new .dir\n"));
	return(-1);
    }
    i = putconf(f, &c);
    (void) fclose(f);
    if (i < 0) {
	DEBUG(("dbzagain: putconf failed\n"));
	return(-1);
    }

    /* create/truncate .pag */
    fn = enstring(name, pag);
    if (fn == NULL)
	return(-1);
    f = fopen(fn, "w");
    free((POINTER)fn);
    if (f == NULL) {
	DEBUG(("dbzagain: unable to create/truncate .pag file\n"));
	return(-1);
    } else
	(void) fclose(f);

    /* and let dbminit do the work */
    return(dbminit(name));
}

/*
 * dbminit - open a database, creating it (using defaults) if necessary
 *
 * We try to leave errno set plausibly, to the extent that underlying
 * functions permit this, since many people consult it if dbminit() fails.
 * return 0 for success, -1 for failure
 */
int dbminit(char *name) {
    size_t s;
    char *dirfname;
    char *pagfname;

    if (pagf != NULL) {
	DEBUG(("dbminit: dbminit already called once\n"));
	errno = 0;
	return(-1);
    }

    /* open the .dir file */
    dirfname = enstring(name, dir);
    if (dirfname == NULL)
	return(-1);
    dirf = fopen(dirfname, "r+");
    if (dirf == NULL) {
	dirf = fopen(dirfname, "r");
	dirronly = 1;
    } else
	dirronly = 0;
    free((POINTER)dirfname);
    if (dirf == NULL) {
	DEBUG(("dbminit: can't open .dir file\n"));
	return(-1);
    }
    CloseOnExec((int)fileno(dirf), 1);

    /* open the .pag file */
    pagfname = enstring(name, pag);
    if (pagfname == NULL) {
	(void) fclose(dirf);
	return(-1);
    }
    pagf = fopen(pagfname, "r+b");
    if (pagf == NULL) {
	pagf = fopen(pagfname, "rb");
	if (pagf == NULL) {
	    DEBUG(("dbminit: .pag open failed\n"));
	    (void) fclose(dirf);
	    free((POINTER)pagfname);
	    return(-1);
	}
	pagronly = 1;
    } else if (dirronly) {
	pagronly = 1;
    } else {
	pagronly = 0;
	if ((pagfd = open(pagfname, O_WRONLY)) < 0) {
	    DEBUG(("dbminit: could not open pagf\n"));
	    fclose(pagf);
	    fclose(dirf);
	    free((POINTER)pagfname);
	    pagf = NULL;
	    errno = EDOM;
	    return (-1);
	}
#ifdef NONBLOCK
	if (fcntl(pagfd, F_SETFL, O_NONBLOCK) < 0) {
	    DEBUG(("fcntl: could not set nonblock\n"));
	    fclose(pagf);
	    fclose(dirf);
	    free((POINTER)pagfname);
	    pagf = NULL;
	    errno = EDOM;
	    return (-1);
	}
#endif
    }
    

    if (pagf != NULL)
	CloseOnExec((int)fileno(pagf), 1);
#ifdef _IOFBF
    (void) setvbuf(pagf, (char *)pagbuf, _IOFBF, sizeof(pagbuf));
#endif
    pagpos = -1;
    /* don't free pagfname, need it below */

    /* pick up configuration */
    if (getconf(dirf, pagf, &conf) < 0) {
	DEBUG(("dbminit: getconf failure\n"));
	(void) fclose(pagf);
	(void) fclose(dirf);
	free((POINTER)pagfname);
	pagf = NULL;
	errno = EDOM;	/* kind of a kludge, but very portable */
	return(-1);
    }

    /* get first table into core, if it looks desirable and feasible */
    s = (size_t)conf.tsize * sizeof(dbzrec);
    if (incore && (off_t)(s / sizeof(dbzrec)) == conf.tsize) {
	bufpagf = fopen(pagfname, (pagronly) ? "rb" : "r+b");
	if (bufpagf != NULL) {
	    corepag = getcore(bufpagf);
	    CloseOnExec(fileno(bufpagf), 1);
	}
    } else {
	bufpagf = NULL;
	corepag = NULL;
    }
    free((POINTER)pagfname);

    /* misc. setup */
    written = 0;
    prevp = FRESH;
    DEBUG(("dbminit: succeeded\n"));
    return(0);
}

/* enstring - concatenate two strings into a malloced area
 * Returns NULL on failure
 */
static char *enstring(char *s1, char *s2)
{
    char *p;

    p = malloc((size_t)strlen(s1) + (size_t)strlen(s2) + 1);
    if (p != NULL) {
	(void) strcpy(p, s1);
	(void) strcat(p, s2);
    } else {
	DEBUG(("enstring(%s, %s) out of memory\n", s1, s2));
    }
    return(p);
}

/* dbmclose - close a database
 */
int dbmclose(void)
{
    int ret = 0;

    if (pagf == NULL) {
	DEBUG(("dbmclose: not opened!\n"));
	return(-1);
    }

    if (fclose(pagf) == EOF) {
	DEBUG(("dbmclose: fclose(pagf) failed\n"));
	ret = -1;
    }
    if (dbzsync() < 0)
	ret = -1;
    if (bufpagf != NULL && fclose(bufpagf) == EOF) {
	DEBUG(("dbmclose: fclose(bufpagf) failed\n"));
	ret = -1;
    }
    if (corepag != NULL)
#ifdef MMAP
	if (munmap((MMAP_PTR) corepag, (int)conf.tsize * sizeof(dbzrec)) == -1) {
	    DEBUG(("dbmclose: munmap failed\n"));
	    ret = -1;
	}
#else
    free((POINTER)corepag);
#endif
    corepag = NULL;
    pagf = NULL;
    if (fclose(dirf) == EOF) {
	DEBUG(("dbmclose: fclose(dirf) failed\n"));
	ret = -1;
    }

    DEBUG(("dbmclose: %s\n", (ret == 0) ? "succeeded" : "failed"));
    return(ret);
}

/* dbzsync - push all in-core data out to disk
 */
int dbzsync(void)
{
    int ret = 0;

    if (pagf == NULL) {
	DEBUG(("dbzsync: not opened!\n"));
	return(-1);
    }
    if (pagfd != -1)
	fsync(pagfd);
    if (!written)
	return(0);

#ifndef MMAP
    if (corepag != NULL && !writethrough) {
	if (putcore(corepag, bufpagf) < 0) {
	    DEBUG(("dbzsync: putcore failed\n"));
	    ret = -1;
	}
    }
#endif
    if (putconf(dirf, &conf) < 0)
	ret = -1;

    DEBUG(("dbzsync: %s\n", (ret == 0) ? "succeeded" : "failed"));
    return(ret);
}

/* dbzfetch - fetch() with case mapping built in
 */
datum dbzfetch(datum key)
{
    char buffer[DBZMAXKEY + 1];
    datum mappedkey;

    DEBUG(("dbzfetch: (%s)\n", key.dptr));

    /* Key is supposed to be less than DBZMAXKEY */
    mappedkey.dsize = (key.dsize < DBZMAXKEY) ? key.dsize : DBZMAXKEY;

    mappedkey.dptr = mapcase(buffer, key.dptr, mappedkey.dsize);
    buffer[mappedkey.dsize] = '\0';	/* just a debug aid */

    return(fetch(mappedkey));
}

/* fetch - get an entry from the database
 *
 * Disgusting fine point, in the name of backward compatibility:  if the
 * last character of "key" is a NUL, that character is (effectively) not
 * part of the comparison against the stored keys.
 *
 * Returns: dtr == NULL && dsize == 0 on failure.
 */
datum fetch(datum key) {
    char buffer[DBZMAXKEY + 1];
    static dbzrec *key_ptr;		/* return value points here */
    datum output;

    DEBUG(("fetch: (%s)\n", key.dptr));
    output.dptr = NULL;
    output.dsize = 0;
    prevp = FRESH;

    if (pagf == NULL) {
	DEBUG(("fetch: database not open!\n"));
	return(output);
    }

    start(&srch, &key, FRESH);
    if ((key_ptr = search(&srch)) != NOTFOUND) {
	output.dptr = (char *)&key_ptr->offset;
	output.dsize = sizeof(key_ptr->offset);
	DEBUG(("fetch: successful\n"));
	return(output);
    }

    /* we didn't find it */
    DEBUG(("fetch: failed\n"));
    prevp = &srch;			/* remember where we stopped */
    return(output);
}

/* dbzstore - store() with case mapping built in
 */
int dbzstore(datum key, datum data)
{
    char buffer[DBZMAXKEY + 1];
    datum mappedkey;

    DEBUG(("dbzstore: (%s)\n", key.dptr));

    /* Key is supposed to be less than DBZMAXKEY */
    mappedkey.dsize = (key.dsize < DBZMAXKEY) ? key.dsize : DBZMAXKEY;

    mappedkey.dptr = mapcase(buffer, key.dptr, mappedkey.dsize);
    buffer[mappedkey.dsize] = '\0';	/* just a debug aid */

    return(store(mappedkey, data));
}

/*
 * store - add an entry to the database
 * returns 0 for sucess and -1 for failure 
 */
int store(datum key, datum data)
{
    dbzrec value;

    if (pagf == NULL) {
	DEBUG(("store: database not open!\n"));
	return(-1);
    }
    if (pagronly) {
	DEBUG(("store: database open read-only\n"));
	return(-1);
    }
    if (data.dsize != sizeof(value.offset)) {
	DEBUG(("store: value size wrong (%d)\n", data.dsize));
	return(-1);
    }
    if (key.dsize >= DBZMAXKEY) {
	DEBUG(("store: key size too big (%d)\n", key.dsize));
	return(-1);
    }

    /* find the place, exploiting previous search if possible */
    start(&srch, &key, prevp);
    while (search(&srch) != NOTFOUND)
	continue;

    prevp = FRESH;
    conf.used[0]++;
    DEBUG(("store: used count %ld\n", conf.used[0]));
    written = 1;

    /* copy the value in to ensure alignment */
    memcpy((POINTER)&value.offset, (POINTER)data.dptr, data.dsize);
    DEBUG(("store: (%s, %ld)\n", key.dptr, (long)value));
    value.offset = htonl(value.offset);
    value.hash = srch.hash;
    
    return(set(&srch, value));
}

/* dbzincore - control attempts to keep .pag file in core
 * Return the old setting.
 */
int dbzincore(int value) {
    int old = incore;

#ifndef MMAP
    incore = value;
#endif
    return(old);
}

/* dbzwritethrough - write through the pag file in core
 * Returns the old setting
 */
int dbzwritethrough(int value) {
    int old = writethrough;

    writethrough = value;
    return(old);
}

/*
 * getconf - get configuration from .dir file
 *   df    - NULL means just give me the default 
 *   pf    - NULL means don't care about .pag 
 *   returns 0 for success, -1 for failure
 */
static int getconf(FILE *df, FILE *pf, dbzconfig *cp) {
    int c;
    int i;
    int err = 0;

    c = (df != NULL) ? getc(df) : EOF;
    if (c == EOF) {		/* empty file, no configuration known */
	cp->tsize = DEFSIZE;
	for (i = 0; i < NUSEDS; i++)
	    cp->used[i] = 0;
	cp->valuesize = sizeof(dbzrec);
	DEBUG(("getconf: defaults (%ld)\n", cp->tsize));
	return(0);
    }
    (void) ungetc(c, df);

    /* first line, the vital stuff */
    if (getc(df) != 'd' || getc(df) != 'b' || getc(df) != 'z')
	err = -1;
    if (getno(df, &err) != dbzversion)
	err = -1;
    cp->tsize = getno(df, &err);
    cp->valuesize = getno(df, &err);
    if (cp->valuesize != sizeof(dbzrec)) {
	DEBUG(("getconf: wrong of_t size (%d)\n", cp->valuesize));
	err = -1;
	cp->valuesize = sizeof(dbzrec);	/* to protect the loops below */
    }
    if (getc(df) != '\n')
	err = -1;
#ifdef DBZDEBUG
    DEBUG(("size %ld\n", cp->tsize));
#endif

    /* second line, the usages */
    for (i = 0; i < NUSEDS; i++)
	cp->used[i] = getno(df, &err);
    if (getc(df) != '\n')
	err = -1;
    DEBUG(("used %ld %ld %ld...\n", cp->used[0], cp->used[1], cp->used[2]));

    if (err < 0) {
	DEBUG(("getconf error\n"));
	return(-1);
    }
    return(0);
}

/*
 * getno - get a long
 */
static long getno(FILE *f, int *ep) {
    char *p;
#	define	MAXN	50
    char getbuf[MAXN];
    int c;

    while ((c = getc(f)) == ' ')
	continue;
    if (c == EOF || c == '\n') {
	DEBUG(("getno: missing number\n"));
	*ep = -1;
	return(0);
    }
    p = getbuf;
    *p++ = c;
    while ((c = getc(f)) != EOF && c != '\n' && !isspace(c))
	if (p < &getbuf[MAXN-1])
	    *p++ = c;
    if (c == EOF) {
	DEBUG(("getno: EOF\n"));
	*ep = -1;
    } else
	(void) ungetc(c, f);
    *p = '\0';

    if (strspn(getbuf, "-1234567890") != strlen(getbuf)) {
	DEBUG(("getno: `%s' non-numeric\n", getbuf));
	*ep = -1;
    }
    return(atol(getbuf));
}

/* putconf - write configuration to .dir file
 * Returns: 0 for success, -1 for failure
 */
static int putconf(FILE *f, dbzconfig *cp) {
    int i;
    int ret = 0;

    if (fseek(f, (off_t)0, SEEK_SET) != 0) {
	DEBUG(("fseek failure in putconf\n"));
	ret = -1;
    }
    fprintf(f, "dbz %d %ld %d\n", dbzversion, cp->tsize,
		   cp->valuesize);
    for (i = 0; i < NUSEDS; i++)
	fprintf(f, "%ld%c", cp->used[i], (i < NUSEDS-1) ? ' ' : '\n');

    fflush(f);
    if (ferror(f))
	ret = -1;

    DEBUG(("putconf status %d\n", ret));
    return(ret);
}

/* getcore - try to set up an in-core copy of .pag file
 *
 * Returns: pointer to copy of .pag or NULL on errror
 */
static dbzrec *getcore(FILE *f) {
    dbzrec *p;
    size_t i;
    size_t nread;
    char *it;
#ifdef MMAP
    struct stat st;

    if (fstat(fileno(f), &st) == -1) {
	DEBUG(("getcore: fstat failed\n"));
	return(NULL);
    }
    if (((size_t)conf.tsize * sizeof(dbzrec)) > st.st_size) {
	/* file too small; extend it */
	if (ftruncate((int)fileno(f), conf.tsize * sizeof(dbzrec)) == -1) {
	    DEBUG(("getcore: ftruncate failed\n"));
	    return(NULL);
	}
    }
    it = mmap((caddr_t)0, (size_t)conf.tsize * sizeof(dbzrec), 
	      pagronly ? PROT_READ : PROT_WRITE | PROT_READ, MAP__ARG,
	      (int)fileno(f), (off_t)0);
    if (it == (char *)-1) {
	DEBUG(("getcore: mmap failed\n"));
	return(NULL);
    }
#if defined (MADV_RANDOM)                           
    /* not present in all versions of mmap() */
    madvise(it, (size_t)conf.tsize * sizeof(dbzrec), MADV_RANDOM);
#endif
#else
    it = malloc((size_t)conf.tsize * sizeof(dbzrec));
    if (it == NULL) {
	DEBUG(("getcore: malloc failed\n"));
	return(NULL);
    }

    nread = fread((POINTER)it, sizeof(dbzrec), conf.tsize, f);
    if (ferror(f)) {
	DEBUG(("getcore: read failed\n"));
	free((POINTER)it);
	return(NULL);
    }

    p = (dbzrec *)it + nread;
    i = (size_t)conf.tsize - nread;
    memset(p, '\0', i * sizeof(dbzrec));
#endif
    return((dbzrec *)it);
}

#ifndef MMAP
/* putcore - try to rewrite an in-core table
 *
 * Returns 0 on success, -1 on failure
 */
static int putcore(dbzrec *tab, FILE *f) {
    if (fseek(f, (off_t)0, SEEK_SET) != 0) {
	DEBUG(("fseek failure in putcore\n"));
	return(-1);
    }
    fwrite((POINTER)tab, sizeof(dbzrec), (size_t)conf.tsize, f);
    fflush(f);
    return((ferror(f)) ? -1 : 0);
}
#endif

/* start - set up to start or restart a search
 * osp == NULL is acceptable
 */
static void start(searcher *sp, datum *kp, searcher *osp) {
    hash_t h;

    h = dbzhash(kp->dptr, kp->dsize);
    if (osp != FRESH && !memcmp(&osp->hash, &h, sizeof(h))) {
	if (sp != osp)
	    *sp = *osp;
	sp->run--;
	DEBUG(("search restarted\n"));
    } else {
	sp->hash = h;
	memcpy(&sp->shorthash, &h,
	       (sizeof(h) < sizeof(sp->shorthash) ? sizeof(h) : sizeof(sp->shorthash)));
	sp->tabno = 0;
	sp->run = -1;
	sp->aborted = 0;
    }
}

/* search - conduct part of a search
 *
 * return NOTFOUND if we hit VACANT or error
 */
static dbzrec *search(searcher *sp) {
    static dbzrec value;

    if (sp->aborted)
	return(NOTFOUND); 

    for (;;) {
	/* go to next location */
	if (sp->run++ == MAXRUN) {
	    sp->tabno++;
	    sp->run = 0;
	}

	sp->place = ((sp->shorthash + sp->run) % conf.tsize) + (sp->tabno * conf.tsize);  
	DEBUG(("search @ %ld\n", sp->place));

	/* get the value */
	if ((corepag != NULL) && (sp->place < conf.tsize)) {
	    DEBUG(("search: in core\n"));
	    memcpy(&value, &corepag[sp->place], sizeof(value)); 
	} else {
	    off_t dest = 0;
	    /* seek, if necessary */
	    dest = sp->place * sizeof(dbzrec);
	    if (pagpos != dest) {
		if (fseek(pagf, dest, SEEK_SET) != 0) {
		    DEBUG(("search: seek failed\n"));
		    pagpos = -1;
		    sp->aborted = 1;
		    return(NOTFOUND);
		}
		pagpos = dest;
	    }

	    /* read it */
	    if (fread((POINTER)&value, sizeof(value), 1, pagf) != 1) {
		if (ferror(pagf)) {
		    DEBUG(("search: read failed\n"));
		    pagpos = -1;
		    sp->aborted = 1;
		    return(NOTFOUND);
		} else {
		    memset(&value, '\0', sizeof(value));
		}
	    }

	    /* and finish up */
	    pagpos += sizeof(value);
	}

	if (VACANT(value)) {
	    DEBUG(("search: empty slot\n"));
	    return(NOTFOUND);
	}

	/* check the value */
	DEBUG(("got 0x%lx\n", value));
	if (!memcmp(&value.hash, &sp->hash, sizeof(hash_t))) {
	    value.offset = ntohl(value.offset);
	    return(&value);
	}
    }
    /* NOTREACHED */
}

/* set - store a value into a location previously found by search
 *
 * Returns:  0 success, -1 failure
 */
static int set(searcher *sp, dbzrec value) {
    if (sp->aborted)
	return(-1);

    DEBUG(("value is 0x%lx\n", value));

    /* If we have the index file in memory, use it */
    if (corepag != NULL && sp->place < conf.tsize) {
	memcpy(&corepag[sp->place], &value, sizeof(value));
	DEBUG(("set: incore\n"));
#ifdef MMAP	
	return(0);
#else
	if (!writethrough)
	    return(0);
#endif
    }

    /* seek to spot */
    pagpos = -1;		/* invalidate position memory */
    if (lseek(pagfd, (off_t)(sp->place * sizeof(dbzrec)), SEEK_SET) == -1) {
	DEBUG(("set: seek failed\n"));
	sp->aborted = 1;
	return(-1);
    }

    /* write in data */
    while (write(pagfd, (POINTER)&value, sizeof(dbzrec)) != sizeof(dbzrec)) {
	if (errno == EINTR) {
	    if (lseek(pagfd, (off_t)(sp->place * sizeof(dbzrec)), SEEK_SET) == -1) {
		DEBUG(("set: seek failed\n"));
		sp->aborted = 1;
		return(-1);
	    }
	}
	if (errno == EAGAIN) {
	    fd_set writeset;
	    int result;
	    
	    FD_ZERO(&writeset);
	    FD_SET(pagfd, &writeset);
	    if (select(pagfd + 1, NULL, &writeset, NULL, NULL) < 1) {
		DEBUG(("set: select failed\n"));
		sp->aborted = 1;
		return(-1);
	    }
	    if (lseek(pagfd, (off_t)(sp->place * sizeof(dbzrec)), SEEK_SET) == -1) {
		DEBUG(("set: seek failed\n"));
		sp->aborted = 1;
		return(-1);
	    }
	}
	DEBUG(("set: write failed\n"));
	sp->aborted = 1;
	return(-1);
    }

    DEBUG(("set: succeeded\n"));
    return(0);
}

/* dbzhash - Variant of md5
 *
 * Returns: hash_t with the sizeof(hash_t) bytes of hash
 */
hash_t dbzhash(char *value, int size) {
    MD5_CTX context;
    static hash_t hash;

    MD5Init(&context);
    MD5Update(&context, value, size);
    MD5Final(&context);
    memcpy(&hash,
	   &context.buf,
	   (sizeof(hash) < sizeof(context.buf)) ? sizeof(hash) : sizeof(context.buf));
    return hash;
}

/* cipoint - where in this message-ID does it become case-insensitive?
 *
 * The RFC822 code is not quite complete.  Absolute, total, full RFC822
 * compliance requires a horrible parsing job, because of the arcane
 * quoting conventions -- abc"def"ghi is not equivalent to abc"DEF"ghi,
 * for example.  There are three or four things that might occur in the
 * domain part of a message-id that are case-sensitive.  They don't seem
 * to ever occur in real news, thank Cthulhu.  (What?  You were expecting
 * a merciful and forgiving deity to be invoked in connection with RFC822?
 * Forget it; none of them would come near it.)
 *
 * Returns: pointer into s, or NULL for "nowhere"
 */
static char *cipoint(char *s, size_t size) {
    char *p;

    if ((p = memchr(s, '@', size))== NULL)			/* no local/domain split */
	return(NULL);		/* assume all local */
    if (!strncasecmp("postmaster", s+1, 10)) {
	/* crazy -- "postmaster" is case-insensitive */
	return(s);
    }
    return(p);
}

/* mapcase - do case-mapped copy
 * dest    - Destination for copy
 * src     - Source for copy (src == dest is legal)
 *
 * May return either src or dest
 */
static char *mapcase(char *dest, char *src, size_t size) {
    char *s;
    char *d;
    char *c;

    if ((c = cipoint(src, size)) == NULL)
	return(src);

    memcpy(dest, src, c-src);
    for (s = c, d = dest + (c-src); s < (src + size); s++, d++)
	*d = tolower(*s);

    return(dest);
}


/* dbzdebug - control dbz debugging at run time
 *
 * Returns: old value for dbzdebug
 *
 */
#ifdef DBZDEBUG
int dbzdebug(int value) {
    int old = debug;

    debug = value;
    return(old);
}
#endif


#ifdef DBZTEST
void CloseOnExec(int i, int j) {
}

int timediffms(struct timeval start, struct timeval end) {
    return (((end.tv_sec - start.tv_sec) * 1000) +
	    ((end.tv_usec - start.tv_usec)) / 1000);
}

void RemoveDBZ(char *filename) {
    char fn[1024];

    sprintf(fn, "%s.pag", filename);
    unlink(fn);
    sprintf(fn, "%s.dir", filename);
    unlink(fn);
}

int main(int argc, char **argv) {
    char msgid[DBZMAXKEY];
    datum key, data;
    int i;
    struct timeval start, end;
    
    if (argc < 2) {
	fprintf(stderr, "usage: dbztest dbzfile\n");
	exit(1);
    }

    RemoveDBZ(argv[1]);

    gettimeofday(&start, NULL);
    if (dbzfresh(argv[1], 5000000) != 0) {
	perror("dbminit");
	exit(1);
    }
    gettimeofday(&end, NULL);
    printf("dbzfresh(%s, 5000000): %d ms\n", argv[1], timediffms(start, end));

    key.dptr = (POINTER)&i;
    key.dsize = sizeof(i);
    gettimeofday(&start, NULL);
    for (i = 0; i < 100000; i++) {
	dbzfetch(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzfetch() off of disk: %0.5f ms\n",
	   timediffms(start, end)/(float)100000);

    gettimeofday(&start, NULL);
    for (i = 0; i < 100000; i++) {
	dbzfetch(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzfetch() from memory: %0.5f ms\n",
	   timediffms(start, end)/(float)100000);

    gettimeofday(&start, NULL);
    data.dsize = sizeof(i);
    data.dptr = (POINTER)&i;
    for (i = 0; i < 333333; i++) {
	dbzstore(key, data);
    }
    gettimeofday(&end, NULL);
    printf("Time to fill database 2/3's full: %0.5f ms\n",
	   timediffms(start, end)/(float)333333);
   
    gettimeofday(&start, NULL);
    for (i = 0; i < 100000; i++) {
	dbzfetch(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzfetch() from memory w/data: %0.5f ms\n",
	   timediffms(start, end)/(float)100000);

    printf("Checking dbz integrity\n");
    for (i = 0; i < 333; i++) {
	data = dbzfetch(key);
	if (data.dptr == NULL) {
	    printf("Could not find an entry for %d\n", i);
	    continue;
	}
	if (data.dsize != sizeof(i)) {
	    printf("dsize is wrong for %d (%d != %d)\n",
		   i, data.dsize, sizeof(i));
	}
	/* '@' is handled differently by the case mapping, so we avoid
	   checking this case for correctness */
	if (memchr(data.dptr, '@', sizeof(i)) != NULL)
	    continue;
	if (memcmp(data.dptr, &i, sizeof(i))) {
	    hash_t hash1, hash2;
	    hash1 = dbzhash((char *)&i, sizeof(i));
	    hash2 = dbzhash((char *)data.dptr, data.dsize);
	    if (memcmp(&hash1, &hash2, sizeof(hash_t))) {
		printf("data is wrong for %d (%d != %d)\n",
		       i, i, *(int *)data.dptr);
	    } else {
		printf("hash collision for %d, %d\n",
		       i, *(int *)data.dptr);
	    }
	}
    }

    
    dbmclose();
    RemoveDBZ(argv[1]);

    return 0;
}
#endif
