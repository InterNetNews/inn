/*
  dbz.c  V6.0

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
and replaced it with hashed .pag entries.  This removes the need for
base file access.  Primary bottleneck now appears to be the hash
algorithm and search().  You can change DBZ_INTERNAL_HASH_SIZE in
dbz.h to increase the size of the stored hash.

These routines replace dbm as used by the usenet news software
(it's not a full dbm replacement by any means).  It's fast and
simple.  It contains no AT&T code.

The dbz database exploits the fact that when news stores a <key,value>
tuple, the `value' part is a seek offset into a text file, pointing to
a copy of the `key' part.  This avoids the need to store a copy of
the key in the dbz files.  However, the text file *must* exist and be
consistent with the dbz files, or things will fail.

The basic format of the database is two hash tables, each in it's own
file. One contains the offsets into the history text file , and the
other contains a hash of the message id.  A value is stored by
indexing into the tables using a hash value computed from the key;
collisions are resolved by linear probing (just search forward for an
empty slot, wrapping around to the beginning of the table if
necessary).  Linear probing is a performance disaster when the table
starts to get full, so a complication is introduced.  Each file actually
contains one *or more* tables, stored sequentially in the files, and
the length of the linear-probe sequences is limited.  The search (for
an existing item or an empy slot always starts in the first table of
the hash file, and whenever MAXRUN probes have been done in table N,
probing continues in table N+1.  It is best not to overflow into more
than 1-2 tables or else massive performance degradation may occur.
Choosing the size of the database is extremely important because of this.

The table size is fixed for any particular database, but is determined
dynamically when a database is rebuilt.  The strategy is to try to pick
the size so the first table will be no more than 2/3 full, that being
slightly before the point where performance starts to degrade.  (It is
desirable to be a bit conservative because the overflow strategy tends
to produce files with holes in them, which is a nuisance.)

*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <md5.h>
#include <clibrary.h>
#include <configdata.h>
#include <libinn.h>
#include <macros.h>
#include <dbz.h>

/*
 * "LIA" = "leave it alone unless you know what you're doing".
 *
 * DBZDEBUG	enable debugging
 * DBZTEST      Generate a standalone program for testing and benchmarking
 * DEFSIZE	default table size (not as critical as in old dbz)
 * NMEMORY	number of days of memory for use in sizing new table (LIA)
 * MAXRUN	length of run which shifts to next table (see below) (LIA)
 */

static int dbzversion = 6;	/* for validating .dir file format */

#ifdef MAP_FILE
#define MAP__ARG	(MAP_FILE | MAP_SHARED)
#else
#define MAP__ARG	(MAP_SHARED)
#endif

/* Old dbz used a long as the record type for dbz entries, which became
 * really gross in places because of mixed references.  We define these to
 * make it a bit easier if we want to store more in here.
 */

#ifdef __GNUC__
#define PACKED __attribute__ ((packed))
#endif

#ifdef __SUNPRO_C
#pragma pack(1)
typedef struct {
    char               hash[DBZ_INTERNAL_HASH_SIZE];
} erec;
typedef struct {
    unsigned long      offset;
} idxrec;
#pragma pack()
#else
typedef struct {
    char               hash[DBZ_INTERNAL_HASH_SIZE];
} PACKED erec;
typedef struct {
    unsigned long      offset;
} PACKED idxrec;
#endif

/* A new, from-scratch database, not built as a rebuild of an old one,
 * needs to know table size.  Normally the user supplies this info,
 * but there have to be defaults.
 */
#ifndef DEFSIZE
#define	DEFSIZE        7500000
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
    int fillpercent;            /* fillpercent/100 is the percent full we'll
				   try to keep the .pag file */
} dbzconfig;

static dbzconfig conf;
/* Default to write through off, index from disk, exists mmap'ed and non-blocking writes */
static dbzoptions options = { FALSE, INCORE_NO, INCORE_MMAP, TRUE };

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
    hash_t hash;	        /* the key's hash code */
    unsigned long shorthash;    /* integer version of the hash, used for
				   determining the entries location */
    int aborted;		/* has i/o error aborted search? */
} searcher;
#define	FRESH	((searcher *)NULL)

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
#define DEBUG(args) if (debug) { printf args ; } else
#else
#define	DEBUG(args)	;
#endif

/* Structure for hash tables */
typedef struct {
    FILE *f;                    /* FILE with a small buffer for hash reads */
    int fd;                     /* Non-blocking descriptor for writes */
    int pos;                    /* Current offset into the table */
    int reclen;                 /* Length of records in the table */
    dbz_incore_val incore;      /* What we're using core for */
    void *core;                 /* Pointer to in-core table */
} hash_table;

/* central data structures */
static BOOL opendb = FALSE;     /* Indicates if a database is currently open */
static FILE *dirf;		/* descriptor for .dir file */
static BOOL readonly;		/* database open read-only? */
static hash_table idxtab;       /* index hash table, used for data retrieval */
static hash_table etab;         /* existance hash table, used for existance checks */
static BOOL dirty;		/* has a store() been done? */
static erec empty_rec;          /* empty rec to compare against
				   initalized in dbminit */

/* misc. forwards */
static char *mapcase(char *dest, char *src, size_t size);
static char *enstring(const char *s1, const char *s2);
static BOOL getcore(hash_table *tab);
static BOOL putcore(hash_table *tab);
static BOOL getconf(FILE *df, dbzconfig *cp);
static int putconf(FILE *f, dbzconfig *cp);
static void start(searcher *sp, const datum *kp, searcher *osp);
static BOOL search(searcher *sp);
static BOOL set(searcher *sp, hash_table *tab, void *value);

/* file-naming stuff */
static char dir[] = ".dir";
static char idx[] = ".index";
static char exists[] = ".hash";

/* dbzfresh - set up a new database, no historical info
 * Return 0 for success, -1 for failure
 * name - base name; .dir and .pag must exist
 * size - table size (0 means default)
 * fillpercent - target percentage full
 */
BOOL dbzfresh(const char *name, const long size, const int fillpercent)
{
    char *fn;
    dbzconfig c;
    FILE *f;

    if (opendb) {
	DEBUG(("dbzfresh: database already open\n"));
	return FALSE;
    }
    if (size != 0 && size < 2) {
	DEBUG(("dbzfresh: preposterous size (%ld)\n", size));
	return FALSE;
    }

    /* get default configuration */
    if (!getconf((FILE *)NULL, &c))
	return FALSE;	/* "can't happen" */

    /* set the size as specified, make sure we get at least 2 bytes
       of implicit hash */
    if (size != 0)
	c.tsize = size > (64 * 1024) ? size : 64 * 1024;

    /* write it out */
    if ((fn = enstring(name, dir)) == NULL)
	return FALSE;
    f = fopen(fn, "w");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzfresh: unable to write config\n"));
	return FALSE;
    }
    if (putconf(f, &c) < 0) {
	fclose(f);
	return FALSE;
    }
    if (fclose(f) == EOF) {
	DEBUG(("dbzfresh: fclose failure\n"));
	return FALSE;
    }

    /* create/truncate .index */
    if ((fn = enstring(name, idx)) == NULL)
	return FALSE;
    f = fopen(fn, "w");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzfresh: unable to create/truncate .pag file\n"));
	return FALSE;
    } else
        fclose(f);

    /* create/truncate .hash */
    if ((fn = enstring(name, exists)) == NULL)
	return FALSE;
    f = fopen(fn, "w");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzfresh: unable to create/truncate .pag file\n"));
	return FALSE;
    } else
        fclose(f);
    /* and punt to dbminit for the hard work */
    return dbminit(name);
}

/*
 * dbzsize  - what's a good table size to hold this many entries?
 * contents - size of table (0 means return the default)
 */
long dbzsize(const long contents) {
    long n;

    if (contents <= 0) {	/* foulup or default inquiry */
	DEBUG(("dbzsize: preposterous input (%ld)\n", contents));
	return DEFSIZE;
    }
    if ((conf.fillpercent > 0) && (conf.fillpercent < 100))
	n = (contents * conf.fillpercent) / 100;
    else 
	n = (contents * 3) / 2;	/* try to keep table at most 2/3 full */
    DEBUG(("dbzsize: final size %ld\n", n));

    /* Make sure that we get at least 2 bytes of implicit hash */
    if (n < (64 * 1024))
	n = 64 * 1024;
    
    return n;
}

/* dbzagain - set up a new database to be a rebuild of an old one
 * Returns 0 on success, -1 on failure
 * name - base name; .dir and .pag must exist
 * oldname - basename, all must exist
 */
BOOL dbzagain(const char *name, const char *oldname)
{
    char *fn;
    dbzconfig c;
    BOOL result;
    int i;
    long top;
    FILE *f;
    int newtable;
    off_t newsize;

    if (opendb) {
	DEBUG(("dbzagain: database already open\n"));
	return FALSE;
    }

    /* pick up the old configuration */
    if ((fn = enstring(oldname, dir))== NULL)
	return FALSE;
    f = fopen(fn, "r");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzagain: cannot open old .dir file\n"));
	return FALSE;
    }
    result = getconf(f, &c);
    fclose(f);
    if (!result) {
	DEBUG(("dbzagain: getconf failed\n"));
	return FALSE;
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
	return FALSE;
    f = fopen(fn, "w");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzagain: unable to write new .dir\n"));
	return FALSE;
    }
    i = putconf(f, &c);
    fclose(f);
    if (i < 0) {
	DEBUG(("dbzagain: putconf failed\n"));
	return FALSE;
    }

    /* create/truncate .index */
    fn = enstring(name, idx);
    if (fn == NULL)
	return FALSE;
    f = fopen(fn, "w");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzagain: unable to create/truncate .pag file\n"));
	return FALSE;
    } else
	fclose(f);

    /* create/truncate .hash */
    fn = enstring(name, exists);
    if (fn == NULL)
	return FALSE;
    f = fopen(fn, "w");
    DISPOSE(fn);
    if (f == NULL) {
	DEBUG(("dbzagain: unable to create/truncate .pag file\n"));
	return FALSE;
    } else
	fclose(f);

    /* and let dbminit do the work */
    return dbminit(name);
}

static BOOL openhashtable(const char *name, hash_table *tab,
			  const size_t reclen, const dbz_incore_val incore) {
    if ((tab->f = fopen(name, "rb+")) == NULL) {
	tab->f = fopen(name, "rb");
	if (tab->f == NULL) {
	    DEBUG(("openhashtable: open failed\n"));
	    return FALSE;
	}
	readonly = TRUE;
    } else if (readonly) {
	readonly =TRUE;
    } else 

    if ((tab->fd = open(name, readonly ? O_RDONLY : O_RDWR)) < 0) {
	DEBUG(("openhashtable: could not open raw\n"));
	fclose(tab->f);
	errno = EDOM;
	return FALSE;
    }

    tab->reclen = reclen;
    CloseOnExec(fileno(tab->f), 1);
    CloseOnExec(tab->fd, 1);
    tab->pos = -1;

    /* get first table into core, if it looks desirable and feasible */
    tab->incore = incore;
    if (tab->incore != INCORE_NO) {
	if (!getcore(tab)) {
	    DEBUG(("openhashtable: getcore failure\n"));
	    fclose(tab->f);
	    close(tab->fd);
	    errno = EDOM;
	    return FALSE;
	}
    }

    if (options.nonblock && (SetNonBlocking(tab->fd, TRUE) < 0)) {
	DEBUG(("fcntl: could not set nonblock\n"));
	fclose(tab->f);
	close(tab->fd);
	errno = EDOM;
	return FALSE;
    }
    return TRUE;
}

void closehashtable(hash_table *tab) {
    fclose(tab->f);
    close(tab->fd);
    if (tab->incore == INCORE_MEM)
	DISPOSE(tab->core);
    if (tab->incore == INCORE_MMAP) {
	if (munmap((MMAP_PTR) tab->core, (int)conf.tsize * tab->reclen) == -1) {
	    DEBUG(("closehashtable: munmap failed\n"));
	}
    }
}

/*
 * dbminit - open a database, creating it (using defaults) if necessary
 *
 * We try to leave errno set plausibly, to the extent that underlying
 * functions permit this, since many people consult it if dbminit() fails.
 * return 0 for success, -1 for failure
 */
int dbminit(const char *name) {
    char *fname;

    if (opendb) {
	DEBUG(("dbminit: dbminit already called once\n"));
	errno = 0;
	return FALSE;
    }

    /* open the .dir file */
    if ((fname = enstring(name, dir)) == NULL)
	return FALSE;
    if ((dirf = fopen(fname, "r+")) == NULL) {
	dirf = fopen(fname, "r");
	readonly = TRUE;
    } else
	readonly = FALSE;
    DISPOSE(fname);
    if (dirf == NULL) {
	DEBUG(("dbminit: can't open .dir file\n"));
	return FALSE;
    }
    CloseOnExec(fileno(dirf), 1);

    /* pick up configuration */
    if (!getconf(dirf, &conf)) {
	DEBUG(("dbminit: getconf failure\n"));
	fclose(dirf);
	errno = EDOM;	/* kind of a kludge, but very portable */
	return FALSE;
    }

    /* open the .index file */
    if ((fname = enstring(name, idx)) == NULL) {
	fclose(dirf);
	return FALSE;
    }

    if (!openhashtable(fname, &idxtab, sizeof(idxrec), options.idx_incore)) {
	fclose(dirf);
	DISPOSE(fname);
	return FALSE;
    }
    DISPOSE(fname);
    
    /* open the .hash file */
    if ((fname = enstring(name, exists)) == NULL) {
	fclose(dirf);
	return FALSE;
    }

    if (!openhashtable(fname, &etab, sizeof(erec), options.exists_incore)) {
	fclose(dirf);
	DISPOSE(fname);
	closehashtable(&idxtab);
	return FALSE;
    }
    DISPOSE(fname);

    /* misc. setup */
    dirty = FALSE;
    opendb = TRUE;
    prevp = FRESH;
    memset(&empty_rec, '\0', sizeof(empty_rec));
    DEBUG(("dbminit: succeeded\n"));
    return TRUE;
}

/* enstring - concatenate two strings into newly allocated memory
 * Returns NULL on failure
 */
static char *enstring(const char *s1, const char *s2)
{
    char *p;

    p = NEW(char, strlen(s1) + strlen(s2) + 1);
    strcpy(p, s1);
    strcat(p, s2);
    return p;
}

/* dbmclose - close a database
 */
BOOL dbmclose(void)
{
    BOOL ret = TRUE;

    if (!opendb) {
	DEBUG(("dbmclose: not opened!\n"));
	return FALSE;
    }

    if (dbzsync() < 0)
	ret = FALSE;

    closehashtable(&idxtab);
    closehashtable(&etab);

    if (fclose(dirf) == EOF) {
	DEBUG(("dbmclose: fclose(dirf) failed\n"));
	ret = FALSE;
    }

    DEBUG(("dbmclose: %s\n", (ret == 0) ? "succeeded" : "failed"));
    if (ret)
	opendb = FALSE;
    return ret;
}

/* dbzsync - push all in-core data out to disk
 */
BOOL dbzsync(void)
{
    BOOL ret = TRUE;

    if (!opendb) {
	DEBUG(("dbzsync: not opened!\n"));
	return FALSE;
    }

    if (!dirty)
	return TRUE;;

    if (!options.writethrough) {
	if (!putcore(&idxtab) || !putcore(&etab)) {
	    DEBUG(("dbzsync: putcore failed\n"));
	    ret = FALSE;;
	}
    }

    if (putconf(dirf, &conf) < 0)
	ret = FALSE;

    DEBUG(("dbzsync: %s\n", ret ? "succeeded" : "failed"));
    return ret;
}

/* dbzexists - check if the given message-id is in the database */
BOOL dbzexists(const datum key) {
    datum mappedkey;
    char buffer[DBZMAXKEY + 1];
    
    if (!opendb) {
	DEBUG(("dbzexists: database not open!\n"));
	return FALSE;
    }

    prevp = FRESH;
    mappedkey.dsize = (key.dsize < DBZMAXKEY) ? key.dsize : DBZMAXKEY;
    mappedkey.dptr = mapcase(buffer, key.dptr, mappedkey.dsize);
    start(&srch, &mappedkey, FRESH);
    return search(&srch);
}

/* dbzfetch - fetch() with case mapping built in
 */
datum dbzfetch(const datum key)
{
    char buffer[DBZMAXKEY + 1];
    datum mappedkey;

    DEBUG(("dbzfetch: (%s)\n", key.dptr));

    /* Key is supposed to be less than DBZMAXKEY */
    mappedkey.dsize = (key.dsize < DBZMAXKEY) ? key.dsize : DBZMAXKEY;

    mappedkey.dptr = mapcase(buffer, key.dptr, mappedkey.dsize);
    buffer[mappedkey.dsize] = '\0';	/* just a debug aid */

    return fetch(mappedkey);
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
    static unsigned long offset;
    datum output;

    DEBUG(("fetch: (%s)\n", key.dptr));
    output.dptr = NULL;
    output.dsize = 0;
    prevp = FRESH;

    if (!opendb) {
	DEBUG(("fetch: database not open!\n"));
	return output;
    }

    start(&srch, &key, FRESH);
    if (search(&srch) == TRUE) {
	/* Actually get the data now */
	if ((options.idx_incore != INCORE_NO) && (srch.place < conf.tsize)) {
	    memcpy(&offset, &((idxrec *)idxtab.core)[srch.place], sizeof(idxrec));
	    offset = ntohl(offset);
	} else {
	    if (lseek(idxtab.fd, srch.place * idxtab.reclen, SEEK_SET) < 0) {
		DEBUG(("fetch: seek failed\n"));
		idxtab.pos = -1;
		srch.aborted = 1;
		return output;
	    }
	    if (read(idxtab.fd, &offset, sizeof(offset)) != sizeof(offset)) {
		DEBUG(("fetch: read failed\n"));
		idxtab.pos = -1;
		srch.aborted = 1;
		return output;
	    }
	    offset = ntohl(offset);
	}
	output.dptr = (char *)&offset;
	output.dsize = sizeof(offset); 
	DEBUG(("fetch: successful\n"));
	return output;
    }

    /* we didn't find it */
    DEBUG(("fetch: failed\n"));
    prevp = &srch;			/* remember where we stopped */
    return output;
}

/* dbzstore - store() with case mapping built in
 */
BOOL dbzstore(const datum key, const datum data)
{
    char buffer[DBZMAXKEY + 1];
    datum mappedkey;

    DEBUG(("dbzstore: (%s)\n", key.dptr));

    /* Key is supposed to be less than DBZMAXKEY */
    mappedkey.dsize = (key.dsize < DBZMAXKEY) ? key.dsize : DBZMAXKEY;

    mappedkey.dptr = mapcase(buffer, key.dptr, mappedkey.dsize);
    buffer[mappedkey.dsize] = '\0';	/* just a debug aid */

    return store(mappedkey, data);
}

/*
 * store - add an entry to the database
 * returns 0 for sucess and -1 for failure 
 */
BOOL store(const datum key, const datum data)
{
    idxrec ivalue;
    erec   evalue;
    int offset;

    if (!opendb) {
	DEBUG(("store: database not open!\n"));
	return FALSE;
    }
    if (readonly) {
	DEBUG(("store: database open read-only\n"));
	return FALSE;
    }
    if (data.dsize != sizeof(ivalue)) {
	DEBUG(("store: value size wrong (%d)\n", data.dsize));
	return FALSE;
    }
    if (key.dsize >= DBZMAXKEY) {
	DEBUG(("store: key size too big (%d)\n", key.dsize));
	return FALSE;
    }

    /* find the place, exploiting previous search if possible */
    start(&srch, &key, prevp);
    if (search(&srch) == TRUE)
	return FALSE;

    prevp = FRESH;
    conf.used[0]++;
    DEBUG(("store: used count %ld\n", conf.used[0]));
    dirty = TRUE;

    /* copy the value in to ensure alignment */
    memcpy(&offset, (POINTER)data.dptr, data.dsize);
    offset = htonl(offset);
    memcpy(&ivalue.offset, &offset, sizeof(offset));
    memcpy(&evalue.hash, &srch.hash,
	   sizeof(evalue.hash) < sizeof(srch.hash) ? sizeof(evalue.hash) : sizeof(srch.hash));

    /* Set the value in the index first since we don't care if it's out of date */
    if (!set(&srch, &idxtab, &ivalue))
	return FALSE;
    return (set(&srch, &etab, &evalue));
}

/*
 * getconf - get configuration from .dir file
 *   df    - NULL means just give me the default 
 *   pf    - NULL means don't care about .pag 
 *   returns 0 for success, -1 for failure
 */
static BOOL getconf(FILE *df, dbzconfig *cp) {
    int i;

    if (df == NULL) {		/* empty file, no configuration known */
	cp->tsize = DEFSIZE;
	for (i = 0; i < NUSEDS; i++)
	    cp->used[i] = 0;
	cp->valuesize = sizeof(idxrec) + sizeof(erec);
	cp->fillpercent = 66;
	DEBUG(("getconf: defaults (%ld)\n", cp->tsize));
	return TRUE;
    }

    i = fscanf(df, "dbz 6 %ld %d %d\n", &cp->tsize, &cp->valuesize, &cp->fillpercent);
    if (i != 3) {
	DEBUG(("getconf error"));
	return FALSE;
    }
    
    if (cp->valuesize != (sizeof(idxrec) + sizeof(erec))) {
	DEBUG(("getconf: wrong of_t size (%d)\n", cp->valuesize));
	return FALSE;
    }

    DEBUG(("size %ld\n", cp->tsize));

    /* second line, the usages */
    for (i = 0; i < NUSEDS; i++)
	if (!fscanf(df, "%ld", &cp->used[i])) {
	    DEBUG(("getconf error\n"));
	    return FALSE;
	}
	    

    DEBUG(("used %ld %ld %ld...\n", cp->used[0], cp->used[1], cp->used[2]));

    return TRUE;
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
    fprintf(f, "dbz %d %ld %d %d\n", dbzversion, cp->tsize,
		   cp->valuesize, cp->fillpercent);
    for (i = 0; i < NUSEDS; i++)
	fprintf(f, "%ld%c", cp->used[i], (i < NUSEDS-1) ? ' ' : '\n');

    fflush(f);
    if (ferror(f))
	ret = -1;

    DEBUG(("putconf status %d\n", ret));
    return ret;
}

/* getcore - try to set up an in-core copy of .pag file
 *
 * Returns: pointer to copy of .pag or NULL on errror
 */
static BOOL getcore(hash_table *tab) {
    char *it;
    int nread;
    int i;
    struct stat st;

    if (tab->incore == INCORE_MMAP) {
	if (fstat(tab->fd, &st) == -1) {
	    DEBUG(("getcore: fstat failed\n"));
	    return FALSE;
	}
	if ((conf.tsize * tab->reclen) > st.st_size) {
	    /* file too small; extend it */
	    if (ftruncate(tab->fd, conf.tsize * tab->reclen) == -1) {
		DEBUG(("getcore: ftruncate failed\n"));
		return FALSE;
	    }
	}
	it = mmap((caddr_t)0, (size_t)conf.tsize * tab->reclen,
		   readonly ? PROT_READ : PROT_WRITE | PROT_READ, MAP__ARG,
		   tab->fd, (off_t)0);
	if (it == (char *)-1) {
	    DEBUG(("getcore: mmap failed\n"));
	    return FALSE;
	}
#if defined (MADV_RANDOM)                           
	/* not present in all versions of mmap() */
	madvise(it, (size_t)conf.tsize * sizeof(dbzrec), MADV_RANDOM);
#endif
    } else {
	it = NEW(char, conf.tsize * tab->reclen);
	
	nread = read(tab->fd, (POINTER)it, tab->reclen * conf.tsize);
	if (nread < 0) {
	    DEBUG(("getcore: read failed\n"));
	    DISPOSE(it);
	    return FALSE;
	}
	
	i = (size_t)conf.tsize - nread;
	memset(it, '\0', i * tab->reclen);
    }

    tab->core = it;
    return TRUE;
}

/* putcore - try to rewrite an in-core table
 *
 * Returns TRUE on success, FALSE on failure
 */
static BOOL putcore(hash_table *tab) {
    int size;
    
    if (tab->incore == INCORE_MEM) {
	SetNonBlocking(tab->fd, FALSE);
	if (lseek(tab->fd, (off_t)0, SEEK_SET) != 0) {
	    DEBUG(("fseek failure in putcore\n"));
	    return FALSE;
	}
	size = tab->reclen * conf.tsize;
	if (write(tab->fd, (POINTER)tab->core, size) != size) {
	    SetNonBlocking(tab->fd, options.nonblock);
	    return FALSE;
	}
	SetNonBlocking(tab->fd, options.nonblock);
    }
    return TRUE;
}

/* start - set up to start or restart a search
 * osp == NULL is acceptable
 */
static void start(searcher *sp, const datum *kp, searcher *osp) {
    hash_t h;
    int tocopy;

    h = dbzhash(kp->dptr, kp->dsize);
    if (osp != FRESH && !memcmp(&osp->hash, &h, sizeof(h))) {
	if (sp != osp)
	    *sp = *osp;
	sp->run--;
	DEBUG(("search restarted\n"));
    } else {
	sp->hash = h;
	tocopy = sizeof(h) < sizeof(sp->shorthash) ? sizeof(h) : sizeof(sp->shorthash);
	/* Copy the bottom half of thhe hash into sp->shorthash */
	memcpy(&sp->shorthash, (char *)&h + (sizeof(h) - tocopy), tocopy);
	sp->shorthash >>= 1;
	sp->tabno = 0;
	sp->run = -1;
	sp->aborted = 0;
    }
}

/* search - conduct part of a search
 *
 * return FALSE if we hit vacant rec's or error
 */
static BOOL search(searcher *sp) {
    erec value;
    unsigned long taboffset = 0;
    
    if (sp->aborted)
	return FALSE;

    for (;;) {
	/* go to next location */
	if (sp->run++ == MAXRUN) {
	    sp->tabno++;
	    sp->run = 0;
	    taboffset = sp->tabno * conf.tsize;
	}

	sp->place = ((sp->shorthash + sp->run) % conf.tsize) + taboffset;
	DEBUG(("search @ %ld\n", sp->place));

	/* get the value */
	if ((options.exists_incore != INCORE_NO) && (sp->place < conf.tsize)) {
	    DEBUG(("search: in core\n"));
	    memcpy(&value, &((erec *)etab.core)[sp->place], sizeof(erec)); 
	} else {
	    off_t dest = 0;
	    /* seek, if necessary */
	    dest = sp->place * sizeof(erec);
	    if (etab.pos != dest) {
		if (fseek(etab.f, dest, SEEK_SET) != 0) {
		    DEBUG(("search: seek failed\n"));
		    etab.pos = -1;
		    sp->aborted = 1;
		    return FALSE;
		}
		etab.pos = dest;
	    }

	    /* read it */
	    if (fread((POINTER)&value, sizeof(erec), 1, etab.f) != 1) {
		if (ferror(etab.f)) {
		    DEBUG(("search: read failed\n"));
		    etab.pos = -1;
		    sp->aborted = 1;
		    return FALSE;
		} else {
		    memset(&value, '\0', sizeof(erec));
		}
	    }

	    /* and finish up */
	    etab.pos += sizeof(erec);
	}

	/* Check for an empty record */
	if (!memcmp(&value, &empty_rec, sizeof(erec))) {
	    DEBUG(("search: empty slot\n"));
	    return FALSE;
	}

	/* check the value */
	DEBUG(("got 0x%lx\n", value));
	if (!memcmp(&value.hash, &sp->hash, DBZ_INTERNAL_HASH_SIZE)) {
	    return TRUE;
	}
    }
    /* NOTREACHED */
}

/* set - store a value into a location previously found by search
 *
 * Returns:  TRUE success, FALSE failure
 */
static BOOL set(searcher *sp, hash_table *tab, void *value) {
    if (sp->aborted)
	return FALSE;

    /* If we have the index file in memory, use it */
    if ((tab->incore != INCORE_NO) && (sp->place < conf.tsize)) {
	memcpy(tab->core + (sp->place * tab->reclen), value, tab->reclen);
	DEBUG(("set: incore\n"));
	if (tab->incore == INCORE_MMAP)
	    return TRUE;
	if (!options.writethrough)
	    return TRUE;
    }

    /* seek to spot */
    tab->pos = -1;		/* invalidate position memory */
    if (lseek(tab->fd, (off_t)(sp->place * tab->reclen), SEEK_SET) == -1) {
	DEBUG(("set: seek failed\n"));
	sp->aborted = 1;
	return FALSE;
    }

    /* write in data */
    if (write(tab->fd, (POINTER)value, tab->reclen) != tab->reclen) {
	if (errno == EINTR) {
	    if (lseek(tab->fd, (off_t)(sp->place * tab->reclen), SEEK_SET) == -1) {
		DEBUG(("set: seek failed\n"));
		sp->aborted = 1;
		return FALSE;
	    }
	}
	if (errno == EAGAIN) {
	    fd_set writeset;
	    
	    FD_ZERO(&writeset);
	    FD_SET(tab->fd, &writeset);
	    if (select(tab->fd + 1, NULL, &writeset, NULL, NULL) < 1) {
		DEBUG(("set: select failed\n"));
		sp->aborted = 1;
		return FALSE;
	    }
	    if (lseek(tab->fd, (off_t)(sp->place * tab->reclen), SEEK_SET) == -1) {
		DEBUG(("set: seek failed\n"));
		sp->aborted = 1;
		return FALSE;
	    }
	}
	DEBUG(("set: write failed\n"));
	sp->aborted = 1;
	return FALSE;
    }

    DEBUG(("set: succeeded\n"));
    return TRUE;
}

/* dbzhash - Variant of md5
 *
 * Returns: hash_t with the sizeof(hash_t) bytes of hash
 */
hash_t dbzhash(const char *value, const int size) {
    MD5_CTX context;
    static hash_t hash;

    MD5Init(&context);
    MD5Update(&context, value, size);
    MD5Final(&context);
    memcpy(&hash,
	   &context.digest,
	   (sizeof(hash) < sizeof(context.digest)) ? sizeof(hash) : sizeof(context.digest));
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
	return NULL;		/* assume all local */
    if (!strncasecmp("postmaster", s+1, 10)) {
	/* crazy -- "postmaster" is case-insensitive */
	return s;
    }
    return p;
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
	return src;

    memcpy(dest, src, c-src);
    for (s = c, d = dest + (c-src); s < (src + size); s++, d++)
	*d = tolower(*s);

    return dest;
}

/* dbzsetoptions - set runtime options for the database.
 */
void dbzsetoptions(const dbzoptions o) {
    options = o;
}

/* dbzgetoptions - get runtime options for the database.
 */
void dbzgetoptions(dbzoptions *o) {
    *o = options;
}


/* dbzdebug - control dbz debugging at run time
 * Returns: old value for dbzdebug
 */
#ifdef DBZDEBUG
int dbzdebug(const BOOL value) {
    BOOL old = debug;

    debug = value;
    return old;
}
#endif


#ifdef DBZTEST
#define FULLRATIO 66

int timediffms(struct timeval start, struct timeval end) {
    return (((end.tv_sec - start.tv_sec) * 1000) +
	    ((end.tv_usec - start.tv_usec)) / 1000);
}

void RemoveDBZ(char *filename) {
    char fn[1024];

    sprintf(fn, "%s.exists", filename);
    unlink(fn);
    sprintf(fn, "%s.index", filename);
    unlink(fn);
    sprintf(fn, "%s.dir", filename);
    unlink(fn);
}

int main(int argc, char **argv) {
    datum key, data;
    int *i;
    char msgid[sizeof(int)];
    struct timeval start, end;
    int numiter = 5000000;
    dbzoptions opt;
    
    if (argc < 2) {
	fprintf(stderr, "usage: dbztest dbzfile\n");
	exit(1);
    }

    RemoveDBZ(argv[1]);

    dbzgetoptions(&opt);
    opt.idx_incore = TRUE;
    dbzsetoptions(opt);

    gettimeofday(&start, NULL);
    if (dbzfresh(argv[1], numiter, 0) != 0) {
	perror("dbminit");
	exit(1);
    }
    gettimeofday(&end, NULL);
    printf("dbzfresh(%s, %d): %d ms\n", argv[1], numiter,
	   timediffms(start, end));

    i = (int *)msgid;
    key.dptr = (POINTER)&msgid;
    key.dsize = sizeof(msgid);
    gettimeofday(&start, NULL);
    for (*i = 0; *i < 100000; (*i)++) {
	dbzfetch(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzfetch() off of disk: %0.5f ms\n",
	   timediffms(start, end)/(float)100000);

    gettimeofday(&start, NULL);
    for (*i = 0; *i < 750000; (*i)++) {
	dbzfetch(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzfetch() from memory: %0.5f ms\n",
	   timediffms(start, end)/(float)750000);

    gettimeofday(&start, NULL);
    for (*i = 0; *i < 750000; (*i)++) {
	dbzexists(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzexists() from memory: %0.5f ms\n",
	   timediffms(start, end)/(float)750000);

    gettimeofday(&start, NULL);
    data.dsize = sizeof(msgid);
    data.dptr = (POINTER)&msgid;
    for (*i = 0; *i < ((numiter * FULLRATIO) / 100); (*i)++) {
	dbzstore(key, data);
    }
    gettimeofday(&end, NULL);
    printf("Time to fill database 2/3's full: %0.5f ms\n",
	   timediffms(start, end)/(float) ((numiter * FULLRATIO) / 100));
   
    gettimeofday(&start, NULL);
    for (*i = 0; *i < 100000; (*i)++) {
	dbzfetch(key);
    }
    gettimeofday(&end, NULL);
    printf("dbzfetch() from memory w/data: %0.5f ms\n",
	   timediffms(start, end)/(float)100000);

    printf("Checking dbz integrity\n");
    for (*i = 0; *i < ((numiter * FULLRATIO) / 100); (*i)++) {
	data = dbzfetch(key);
	if (data.dptr == NULL) {
	    printf("Could not find an entry for %d\n", *i);
	    continue;
	}
	if (data.dsize != sizeof(msgid)) {
	    printf("dsize is wrong for %d (%d != %d)\n",
		   *i, data.dsize, sizeof(msgid));
	}
	/* '@' is handled differently by the case mapping, so we avoid
	   checking this case for correctness */
	if (memchr(data.dptr, '@', sizeof(msgid)) != NULL)
	    continue;
	if (memcmp(data.dptr, msgid, sizeof(msgid))) {
	    hash_t hash1, hash2;
	    hash1 = dbzhash((char *)msgid, sizeof(msgid));
	    hash2 = dbzhash((char *)data.dptr, data.dsize);
	    if (memcmp(&hash1, &hash2, sizeof(hash_t))) {
		printf("data is wrong for %d (%d != %d)\n",
		       *i, *i, *(int *)data.dptr);
	    } else {
		printf("hash collision for %d, %d\n",
		       *i, *(int *)data.dptr);
	    }
	}
    }

    dbmclose();
    RemoveDBZ(argv[1]);

    return 0;
}
#endif
