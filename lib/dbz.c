/*
  dbz.c  V6.1.1

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
the key in the dbz files. 

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

Tagged hash + offset fuzzy technique merged by Sang-yong Suh (Nov, 1997)

Fixed a bug handling larger than 1Gb history offset by Sang-yong Suh(1998)
Similar fix was suggested by Mike Hucka <hucka@umich.edu> (Jan, 1998) for
dbz-3.3.2.

Limited can't tag warnings once per dbzinit() by Sang-yong Suh (May, 1998)

*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "dbz.h"
#include "inn/innconf.h"
#include "inn/mmap.h"
#include "libinn.h"

/* Needed on AIX 4.1 to get fd_set and friends. */
#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

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

#ifdef DO_TAGGED_HASH
/* assume that for tagged hash, we don't want more than 4byte of_t even if
 * off_t is 8 bytes -- people who use tagged-hash are usually short on 
 * RAM.
 */
#define of_t long
#else
#define of_t		off_t
#endif
#define	SOF		(sizeof(of_t))
#define	NOTFOUND	((of_t)-1)
#ifdef	DO_TAGGED_HASH

#define OVERFLOW
#ifdef OVERFLOW
#include <limits.h>
#endif

/* MAXDROPBITS is the maximum number of bits dropped from the offset value.
   The least significant bits are dropped.  The space is used to
   store hash additional bits, thereby increasing the possibility of the
   hash detection */
#define MAXDROPBITS	4	/* max # of bits to drop from the offset */

/* MAXFUZZYLENGTH is the maximum in the offset value due to the MAXDROPBITS */
#define MAXFUZZYLENGTH ((1 << MAXDROPBITS) - 1)

/*
 * We assume that unused areas of a binary file are zeros, and that the
 * bit pattern of `(of_t)0' is all zeros.  The alternative is rather
 * painful file initialization.  Note that okayvalue(), if OVERFLOW is
 * defined, knows what value of an offset would cause overflow.
 */
#define VACANT		((of_t)0)
#define BIAS(o)		((o)+1)		/* make any valid of_t non-VACANT */
#define UNBIAS(o)	((o)-1)		/* reverse BIAS() effect */

#define HASTAG(o)	((o)&taghere)
#define TAG(o)		((o)&tagbits)
#define NOTAG(o)	((o)&~tagboth)
#define CANTAG(o)	(((o)&tagboth) == 0)
#define MKTAG(v)	(((v)<<conf.tagshift)&tagbits)

#ifndef NOTAGS
#define TAGENB		0x80	/* tag enable is top bit, tag is next 7 */
#define TAGMASK 	0x7f
#define TAGSHIFT	24
#else
#define TAGENB		0	/* no tags */
#define TAGMASK 	0
#define TAGSHIFT	0
#endif

/*
 * Stdio buffer for base-file reads.  Message-IDs (all news ever needs to
 * read) are essentially never longer than 64 bytes, and the typical stdio
 * buffer is so much larger that it is much more expensive to fill.
 */

static of_t tagbits;		/* pre-shifted tag mask */
static of_t taghere;		/* pre-shifted tag-enable bit */
static of_t tagboth;		/* tagbits|taghere */
static int canttag_warned;		/* flag to control can't tag warning */

#endif	/* DO_TAGGED_HASH */

/* Old dbz used a long as the record type for dbz entries, which became
 * really gross in places because of mixed references.  We define these to
 * make it a bit easier if we want to store more in here.
 */

/* A new, from-scratch database, not built as a rebuild of an old one, needs
 * to know table size.  Normally the user supplies this info, but there have
 * to be defaults.  Making this too small can have devestating effects on
 * history speed for the current history implementation whereas making it too
 * big just wastes disk space, so err on the side of caution.  This may still
 * be a bit too small.  Assume people using tagged hash are running somewhat
 * smaller servers.
 */
#ifndef DEFSIZE

#ifdef	DO_TAGGED_HASH
#define DEFSIZE		1000003		/* I need a prime number */
#else
#define	DEFSIZE		10000000
#endif

#endif	/* DEFSIZE */
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
    long vused[NUSEDS];	        /* ditto for text size */
    int valuesize;		/* size of table values, == sizeof(dbzrec) */
    int fillpercent;            /* fillpercent/100 is the percent full we'll
				   try to keep the .pag file */
    of_t tagenb;		/* unshifted tag-enable bit */
    of_t tagmask;		/* unshifted tag mask */
    int tagshift;		/* shift count for tagmask and tagenb */
    int dropbits;		/* number of bits to discard from offset */
    int lenfuzzy;		/* num of fuzzy characters in offset */
} dbzconfig;
static dbzconfig conf;

/*
 * Default dbzoptions to
 */
static dbzoptions options = {
    false,		/* write through off */
    INCORE_NO,		/* index/pag from disk */
#ifdef HAVE_MMAP
    INCORE_MMAP,	/* exists mmap'ed. ignored in tagged hash mode */
#else
    INCORE_NO,		/* exists from disk. ignored in tagged hash mode */
#endif
    true		/* non-blocking writes */
};

/*
 * Data structure for recording info about searches.
 */
typedef struct {
    of_t place;		/* current location in file */
    int tabno;		        /* which table we're in */
    int run;		        /* how long we'll stay in this table */
#		ifndef MAXRUN
#		define	MAXRUN	100
#		endif
    HASH hash;			/* the key's hash code */
    unsigned long shorthash;    /* integer version of the hash, used for
				   determining the entries location.
				   Tagged_hash stores the 31-bit hash here */
    of_t tag;		/* tag we are looking for */
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
    int fd;                     /* Non-blocking descriptor for writes */
    off_t pos;                  /* Current offset into the table */
    int reclen;                 /* Length of records in the table */
    dbz_incore_val incore;      /* What we're using core for */
    void *core;                 /* Pointer to in-core table */
} hash_table;

/* central data structures */
static bool opendb = false;     /* Indicates if a database is currently open */
static FILE *dirf;		/* descriptor for .dir file */
static bool readonly;		/* database open read-only? */
#ifdef	DO_TAGGED_HASH
static FILE *basef;		/* descriptor for base file */
static char *basefname;		/* name for not-yet-opened base file */
static hash_table pagtab;       /* pag hash table, stores hash + offset */
#else
static hash_table idxtab;       /* index hash table, used for data retrieval */
static hash_table etab;         /* existance hash table, used for existance checks */
#endif
static bool dirty;		/* has a store() been done? */
static erec empty_rec;          /* empty rec to compare against
				   initalized in dbzinit */

/* misc. forwards */
static bool getcore(hash_table *tab);
static bool putcore(hash_table *tab);
static bool getconf(FILE *df, dbzconfig *cp);
static int  putconf(FILE *f, dbzconfig *cp);
static void start(searcher *sp, const HASH hash, searcher *osp);
#ifdef	DO_TAGGED_HASH
static of_t search(searcher *sp);
static bool set_pag(searcher *sp, of_t value);
#else
static bool search(searcher *sp);
#endif
static bool set(searcher *sp, hash_table *tab, void *value);

/* file-naming stuff */
static char dir[] = ".dir";
#ifdef	DO_TAGGED_HASH
static char pag[] = ".pag";
#else
static char idx[] = ".index";
static char exists[] = ".hash";
#endif

int dbzneedfilecount(void) {
#ifdef	DO_TAGGED_HASH
    return 2; /* basef and dirf are fopen()'ed and kept */
#else
    return 1; /* dirf is fopen()'ed and kept */
#endif
}

#ifdef	DO_TAGGED_HASH
/*
 - dbzconfbase - reconfigure dbzconf from base file size.
 */
static void
config_by_text_size(dbzconfig *c, of_t basesize)
{
    int			i;
    unsigned long	m;

    /* if no tag requested, just return. */
    if ((c->tagmask | c->tagenb) == 0)
	return;

    /* Use 10 % larger base file size.  Sometimes the offset overflows */
    basesize += basesize / 10;

    /* calculate tagging from old file */
    for (m = 1, i = 0; m < (unsigned long)basesize; i++, m <<= 1)
	continue;
 
    /* if we had more tags than the default, use the new data */
    c->dropbits = 0;
    while (m > (1 << TAGSHIFT)) {
	if (c->dropbits >= MAXDROPBITS)
	    break;
	c->dropbits++;
	m >>= 1;
	i--;
    }
    c->tagenb = TAGENB;
    c->tagmask = TAGMASK;
    c->tagshift = TAGSHIFT;
    if ((c->tagmask | c->tagenb) && m > (1 << TAGSHIFT)) {
	c->tagshift = i;
	c->tagmask = (~(unsigned long)0) >> (i + 1);
	c->tagenb = (c->tagmask << 1) & ~c->tagmask;
    }
    c->lenfuzzy = (int)(1 << c->dropbits) - 1;

    m = (c->tagmask | c->tagenb) << c->tagshift;
    if (m & (basesize >> c->dropbits)) {
	fprintf(stderr, "m 0x%lx size 0x%lx\n", m, basesize);
	exit(1);
    }
}
#endif	/* DO_TAGGED_HASH */

/*
 - create and truncate .pag, .idx, or .hash files
 - return false on error
 */
static bool
create_truncate(const char *name, const char *pag1)
{
    char *fn;
    FILE *f;

    fn = concat(name, pag1, (char *) 0);
    f = Fopen(fn, "w", TEMPORARYOPEN);
    free(fn);
    if (f == NULL) {
	DEBUG(("dbz.c create_truncate: unable to create/truncate %s\n", pag1));
	return false;
    } else
        Fclose(f);
    return true;
}

/* dbzfresh - set up a new database, no historical info
 * Return true for success, false for failure
 * name - base name; .dir and .pag must exist
 * size - table size (0 means default)
 */
bool
dbzfresh(const char *name, off_t size)
{
    char *fn;
    dbzconfig c;
    FILE *f;
#ifdef	DO_TAGGED_HASH
    struct stat sb;
    of_t m;
#endif

    if (opendb) {
	DEBUG(("dbzfresh: database already open\n"));
	return false;
    }
    if (size != 0 && size < 2) {
	DEBUG(("dbzfresh: preposterous size (%ld)\n", size));
	return false;
    }

    /* get default configuration */
    if (!getconf((FILE *)NULL, &c))
	return false;	/* "can't happen" */

#ifdef	DO_TAGGED_HASH
    /* and mess with it as specified */
    if (size != 0)
	c.tsize = size;
    m = c.tagmask;
    c.tagshift = 0;
    while (!(m & 1)) {
	m >>= 1;
	c.tagshift++;
    }
    c.tagmask = m;
    c.tagenb = (m << 1) & ~m;
    c.dropbits = 0;
    c.lenfuzzy = 0;

    /* if big enough basb file exists, update config */
    if (stat(name, &sb) != -1)
	config_by_text_size(&c, sb.st_size);
#else
    /* set the size as specified, make sure we get at least 2 bytes
       of implicit hash */
    if (size != 0)
	c.tsize = size > (64 * 1024) ? size : 64 * 1024;
#endif

    /* write it out */
    fn = concat(name, dir, (char *) 0);
    f = Fopen(fn, "w", TEMPORARYOPEN);
    free(fn);
    if (f == NULL) {
	DEBUG(("dbzfresh: unable to write config\n"));
	return false;
    }
    if (putconf(f, &c) < 0) {
	Fclose(f);
	return false;
    }
    if (Fclose(f) == EOF) {
	DEBUG(("dbzfresh: fclose failure\n"));
	return false;
    }

    /* create and truncate .pag, or .index/.hash files */
#ifdef	DO_TAGGED_HASH
    if (!create_truncate(name, pag))
	return false;
#else
    if (!create_truncate(name, idx))
	return false;
    if (!create_truncate(name, exists))
	return false;
#endif	/* DO_TAGGED_HASH */

    /* and punt to dbzinit for the hard work */
    return dbzinit(name);
}

#ifdef	DO_TAGGED_HASH
/*
 - isprime - is a number prime?
 */
static bool
isprime(long x)
{
    static int quick[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 0 };
    int *ip;
    long div1, stop;

    /* hit the first few primes quickly to eliminate easy ones */
    /* this incidentally prevents ridiculously small tables */
    for (ip = quick; (div1 = *ip) != 0; ip++)
	if (x % div1 == 0) {
	    DEBUG(("isprime: quick result on %ld\n", (long)x));
	    return false;
	}

    /* approximate square root of x */
    for (stop = x; x/stop < stop; stop >>= 1)
	continue;
    stop <<= 1;

    /* try odd numbers up to stop */
    for (div1 = *--ip; div1 < stop; div1 += 2)
	if (x%div1 == 0)
	    return false;

    return true;
}
#endif

/*
 * dbzsize  - what's a good table size to hold this many entries?
 * contents - size of table (0 means return the default)
 */
long
dbzsize(off_t contents)
{
    of_t            n;

    if (contents <= 0) {	/* foulup or default inquiry */
	DEBUG(("dbzsize: preposterous input (%ld)\n", contents));
	return DEFSIZE;
    }

    if ((conf.fillpercent > 0) && (conf.fillpercent < 100))
	n = (contents / conf.fillpercent) * 100;
    else 
	n = (contents * 3) / 2;	/* try to keep table at most 2/3's full */

    /* Make sure that we get at least 2 bytes of implicit hash */
    if (n < (64 * 1024))
	n = 64 * 1024;

#ifdef	DO_TAGGED_HASH
    if (!(n & 1))
        n += 1;			/* make it odd */
    DEBUG(("dbzsize: tentative size %ld\n", n));
    while (!isprime(n))		/* look for a prime */
	n += 2;
#endif

    DEBUG(("dbzsize: final size %ld\n", n));
    return n;
}

/* dbzagain - set up a new database to be a rebuild of an old one
 * Returns true on success, false on failure
 * name - base name; .dir and .pag must exist
 * oldname - basename, all must exist
 */
bool
dbzagain(const char *name, const char *oldname)
{
    char *fn;
    dbzconfig c;
    bool result;
    int i;
    long top;
    FILE *f;
    int newtable;
    of_t newsize;
#ifdef	DO_TAGGED_HASH
    long vtop;
    struct stat sb;
#endif

    if (opendb) {
	DEBUG(("dbzagain: database already open\n"));
	return false;
    }

    /* pick up the old configuration */
    fn = concat(oldname, dir, (char *) 0);
    f = Fopen(fn, "r", TEMPORARYOPEN);
    free(fn);
    if (f == NULL) {
	DEBUG(("dbzagain: cannot open old .dir file\n"));
	return false;
    }
    result = getconf(f, &c);
    Fclose(f);
    if (!result) {
	DEBUG(("dbzagain: getconf failed\n"));
	return false;
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

#ifdef	DO_TAGGED_HASH
    vtop = 0;
    for (i = 0; i < NUSEDS; i++) {
	if (vtop < c.vused[i])
	    vtop = c.vused[i];
	if (c.vused[i] == 0)
	    newtable = 1;	/* hasn't got full usage history yet */
    }
    if (top != 0 && vtop == 0) {
	DEBUG(("dbzagain: old table has no contents!\n"));
	newtable = 1;
    }
    for (i = NUSEDS-1; i > 0; i--)
	c.vused[i] = c.vused[i-1];
    c.vused[0] = 0;

    /* calculate tagging from old file */
    if (stat(oldname, &sb) != -1 && vtop < sb.st_size)
	vtop = sb.st_size;
    config_by_text_size(&c, vtop);
#endif

    newsize = dbzsize(top);
    if (!newtable || newsize > c.tsize)	/* don't shrink new table */
	c.tsize = newsize;

    /* write it out */
    fn = concat(name, dir, (char *) 0);
    f = Fopen(fn, "w", TEMPORARYOPEN);
    free(fn);
    if (f == NULL) {
	DEBUG(("dbzagain: unable to write new .dir\n"));
	return false;
    }
    i = putconf(f, &c);
    Fclose(f);
    if (i < 0) {
	DEBUG(("dbzagain: putconf failed\n"));
	return false;
    }

    /* create and truncate .pag, or .index/.hash files */
#ifdef	DO_TAGGED_HASH
    if (!create_truncate(name, pag))
	return false;
#else
    if (!create_truncate(name, idx))
	return false;
    if (!create_truncate(name, exists))
	return false;
#endif

    /* and let dbzinit do the work */
    return dbzinit(name);
}

static bool
openhashtable(const char *base, const char *ext, hash_table *tab,
	      const size_t reclen, const dbz_incore_val incore)
{
    char *name;

    name = concat(base, ext, (char *) 0);
    if ((tab->fd = open(name, readonly ? O_RDONLY : O_RDWR)) < 0) {
	DEBUG(("openhashtable: could not open raw\n"));
	free(name);
	errno = EDOM;
	return false;
    }
    free(name);

    tab->reclen = reclen;
    close_on_exec(tab->fd, true);
    tab->pos = -1;

    /* get first table into core, if it looks desirable and feasible */
    tab->incore = incore;
    if (tab->incore != INCORE_NO) {
	if (!getcore(tab)) {
	    DEBUG(("openhashtable: getcore failure\n"));
	    close(tab->fd);
	    errno = EDOM;
	    return false;
	}
    }

    if (options.nonblock && nonblocking(tab->fd, true) < 0) {
	DEBUG(("fcntl: could not set nonblock\n"));
	close(tab->fd);
	errno = EDOM;
	return false;
    }
    return true;
}

static void closehashtable(hash_table *tab) {
    close(tab->fd);
    if (tab->incore == INCORE_MEM)
	free(tab->core);
    if (tab->incore == INCORE_MMAP) {
#if defined(HAVE_MMAP)
	if (munmap(tab->core, (int)conf.tsize * tab->reclen) == -1) {
	    DEBUG(("closehashtable: munmap failed\n"));
	}
#else
	DEBUG(("closehashtable: can't mmap files\n"));
#endif
    }
}

#ifdef	DO_TAGGED_HASH
static bool
openbasefile(const char *name)
{
    basef = Fopen(name, "r", DBZ_BASE);
    if (basef == NULL) {
	DEBUG(("dbzinit: basefile open failed\n"));
        basefname = xstrdup(name);
    } else
	basefname = NULL;
    if (basef != NULL)
	close_on_exec(fileno(basef), true);
    if (basef != NULL)
	 setvbuf(basef, NULL, _IOFBF, 64);
    return true;
}
#endif	/* DO_TAGGED_HASH */

/*
 * dbzinit - open a database, creating it (using defaults) if necessary
 *
 * We try to leave errno set plausibly, to the extent that underlying
 * functions permit this, since many people consult it if dbzinit() fails.
 * return true for success, false for failure
 */
bool
dbzinit(const char *name)
{
    char *fname;

    if (opendb) {
	DEBUG(("dbzinit: dbzinit already called once\n"));
	errno = 0;
	return false;
    }

    /* open the .dir file */
    fname = concat(name, dir, (char *) 0);
    if ((dirf = Fopen(fname, "r+", DBZ_DIR)) == NULL) {
	dirf = Fopen(fname, "r", DBZ_DIR);
	readonly = true;
    } else
	readonly = false;
    free(fname);
    if (dirf == NULL) {
	DEBUG(("dbzinit: can't open .dir file\n"));
	return false;
    }
    close_on_exec(fileno(dirf), true);

    /* pick up configuration */
    if (!getconf(dirf, &conf)) {
	DEBUG(("dbzinit: getconf failure\n"));
	Fclose(dirf);
	errno = EDOM;	/* kind of a kludge, but very portable */
	return false;
    }

    /* open pag or idx/exists file */
#ifdef	DO_TAGGED_HASH
    if (!openhashtable(name, pag, &pagtab, SOF, options.pag_incore)) {
	Fclose(dirf);
	return false;
    }
    if (!openbasefile(name)) {
	close(pagtab.fd);
	Fclose(dirf);
	return false;
    }
    tagbits = conf.tagmask << conf.tagshift;
    taghere = conf.tagenb << conf.tagshift;
    tagboth = tagbits | taghere;
    canttag_warned = 0;
#else
    if (!openhashtable(name, idx, &idxtab, sizeof(of_t), options.pag_incore)) {
	Fclose(dirf);
	return false;
    }
    if (!openhashtable(name, exists, &etab, sizeof(erec),
		options.exists_incore)) {
	Fclose(dirf);
	return false;
    }
#endif

    /* misc. setup */
    dirty = false;
    opendb = true;
    prevp = FRESH;
    memset(&empty_rec, '\0', sizeof(empty_rec));
    DEBUG(("dbzinit: succeeded\n"));
    return true;
}

/* dbzclose - close a database
 */
bool
dbzclose(void)
{
    bool ret = true;

    if (!opendb) {
	DEBUG(("dbzclose: not opened!\n"));
	return false;
    }

    if (!dbzsync())
	ret = false;

#ifdef	DO_TAGGED_HASH
    closehashtable(&pagtab);
    if (Fclose(basef) == EOF) {
	fprintf(stderr, "dbzclose: fclose(basef) failed\n");
	ret = false;
    }
    if (basefname != NULL)
	free(basefname);
    basef = NULL;
#else
    closehashtable(&idxtab);
    closehashtable(&etab);
#endif

    if (Fclose(dirf) == EOF) {
	DEBUG(("dbzclose: fclose(dirf) failed\n"));
	ret = false;
    }

    DEBUG(("dbzclose: %s\n", (ret == 0) ? "succeeded" : "failed"));
    if (ret)
	opendb = false;
    return ret;
}

/* dbzsync - push all in-core data out to disk
 */
bool
dbzsync(void)
{
    bool ret = true;

    if (!opendb) {
	DEBUG(("dbzsync: not opened!\n"));
	return false;
    }

    if (!dirty)
	return true;;

    if (!options.writethrough) {
#ifdef	DO_TAGGED_HASH
	if (!putcore(&pagtab)) {
#else
	if (!putcore(&idxtab) || !putcore(&etab)) {
#endif
	    DEBUG(("dbzsync: putcore failed\n"));
	    ret = false;;
	}
    }

    if (putconf(dirf, &conf) < 0)
	ret = false;

    DEBUG(("dbzsync: %s\n", ret ? "succeeded" : "failed"));
    return ret;
}

#ifdef	DO_TAGGED_HASH
/*
 - okayvalue - check that a value can be stored
 */
static int
okayvalue(of_t value)
{
    if (HASTAG(value))
	return(0);
#ifdef OVERFLOW
    if (value == LONG_MAX)	/* BIAS() and UNBIAS() will overflow */
	return(0);
#endif
    return(1);
}
#endif

/* dbzexists - check if the given message-id is in the database */
bool
dbzexists(const HASH key)
{
#ifdef	DO_TAGGED_HASH
    off_t value;

    return (dbzfetch(key, &value) != 0);
#else
    
    if (!opendb) {
	DEBUG(("dbzexists: database not open!\n"));
	return false;
    }

    prevp = FRESH;
    start(&srch, key, FRESH);
    return search(&srch);
#endif
}

/*
 * dbzfetch - get offset of an entry from the database
 *
 * Returns the offset of the text file for input key,
 * or -1 if NOTFOUND or error occurs.
 */
bool
dbzfetch(const HASH key, off_t *value)
{
#ifdef	DO_TAGGED_HASH
#define	MAX_NB2RD	(DBZMAXKEY + MAXFUZZYLENGTH + 2)
#define MIN_KEY_LENGTH	6	/* strlen("<1@a>") + strlen("\t") */
    char *bp, buffer[MAX_NB2RD];
    int keylen, j, nb2r;
    HASH hishash;
    char *keytext = NULL;
    of_t offset = NOTFOUND;
#endif

    prevp = FRESH;

    if (!opendb) {
	DEBUG(("dbzfetch: database not open!\n"));
	return false;
    }

    start(&srch, key, FRESH);
#ifdef	DO_TAGGED_HASH
    /*
     * nb2r: number of bytes to read from history file.
     *       It can be reduced if history text is written as hashes only.
     */
    nb2r = sizeof(buffer) - 1;

    while ((offset = search(&srch)) != NOTFOUND) {
	DEBUG(("got 0x%lx\n", key_ptr));

	/* fetch the key */
	offset <<= conf.dropbits;
	if (offset)		/* backspace 1 character to read '\n' */
	    offset--;
	if (fseeko(basef, offset, SEEK_SET) != 0) {
	    DEBUG(("fetch: seek failed\n"));
	    return false;
	}
	keylen = fread(buffer, 1, nb2r, basef);
	if (keylen < MIN_KEY_LENGTH) {
	    DEBUG(("fetch: read failed\n"));
	    return false;
	}
	buffer[keylen] = '\0';	/* terminate the string */

	if (offset) {		/* find the '\n', the previous EOL */
	    if (keylen > conf.lenfuzzy)
		keylen = conf.lenfuzzy; /* keylen is fuzzy distance now */
	    for (j=0,bp=buffer; j<keylen; j++,bp++)
		if (*bp == '\n')
		    break;
	    if (*bp != '\n') {
		DEBUG(("dbzfetch: can't locate EOL\n"));
		/* pag entry should be deleted, but I'm lazy... */
		continue;
	    }
	    offset++;
	    bp++;	/* now *bp is the start of key */
	} else {
	    j = 0;	/* We are looking for the first history line. */
	    bp = buffer;
	}

	/* Does this history line really have same key? */
	if (*bp == '[') {
	    if (!keytext)
		keytext = HashToText(key);
	    if (memcmp(keytext, bp+1, sizeof(HASH)*2) != 0)
		continue;
	} else if (*bp == '<') {
	    char *p = strchr(bp+1, '\t');
	    if (!p)		/* history has a corrupted line */
		continue;
	    *p = '\0';
	    hishash = HashMessageID(bp);
	    if (memcmp(&key, &hishash, sizeof(HASH)) != 0)
		continue;
	} else
	    continue;

	/* we found it */
	offset += j;
	DEBUG(("fetch: successful\n"));
	*value = offset;
	return true;
    }

    /* we didn't find it */
    DEBUG(("fetch: failed\n"));
    prevp = &srch;			/* remember where we stopped */
    return false;
#else	/* DO_TAGGED_HASH */
    if (search(&srch) == true) {
	/* Actually get the data now */
	if ((options.pag_incore != INCORE_NO) && (srch.place < conf.tsize)) {
	    memcpy(value, &((of_t *)idxtab.core)[srch.place], sizeof(of_t));
	} else {
	    if (pread(idxtab.fd, value, sizeof(of_t), srch.place * idxtab.reclen) != sizeof(of_t)) {
		DEBUG(("fetch: read failed\n"));
		idxtab.pos = -1;
		srch.aborted = 1;
		return false;
	    }
	}
	DEBUG(("fetch: successful\n"));
	return true;
    }

    /* we didn't find it */
    DEBUG(("fetch: failed\n"));
    prevp = &srch;			/* remember where we stopped */
    return false;
#endif
}

/*
 * dbzstore - add an entry to the database
 * returns true for success and false for failure
 */
DBZSTORE_RESULT
dbzstore(const HASH key, off_t data)
{
#ifdef	DO_TAGGED_HASH
    of_t value;
#else
    erec     evalue;
#endif

    if (!opendb) {
	DEBUG(("dbzstore: database not open!\n"));
	return DBZSTORE_ERROR;
    }
    if (readonly) {
	DEBUG(("dbzstore: database open read-only\n"));
	return DBZSTORE_ERROR;
    }

#ifdef	DO_TAGGED_HASH
    /* copy the value in to ensure alignment */
    memcpy(&value, &data, SOF);

    /* update maximum offset value if necessary */
    if (value > conf.vused[0])
	conf.vused[0] = value;

    /* now value is in fuzzy format */
    value >>= conf.dropbits;
    DEBUG(("dbzstore: (%ld)\n", (long)value));

    if (!okayvalue(value)) {
	DEBUG(("dbzstore: reserved bit or overflow in 0x%lx\n", value));
	return DBZSTORE_ERROR;
    }

    /* find the place, exploiting previous search if possible */
    start(&srch, key, prevp);
    while (search(&srch) != NOTFOUND)
	continue;

    prevp = FRESH;
    conf.used[0]++;
    DEBUG(("store: used count %ld\n", conf.used[0]));
    dirty = 1;
    if (!set_pag(&srch, value))
	return DBZSTORE_ERROR;
    return DBZSTORE_OK;
#else	/* DO_TAGGED_HASH */

    /* find the place, exploiting previous search if possible */
    start(&srch, key, prevp);
    if (search(&srch) == true)
	return DBZSTORE_EXISTS;

    prevp = FRESH;
    conf.used[0]++;
    DEBUG(("store: used count %ld\n", conf.used[0]));
    dirty = true;

    memcpy(&evalue.hash, &srch.hash,
	   sizeof(evalue.hash) < sizeof(srch.hash) ? sizeof(evalue.hash) : sizeof(srch.hash));

    /* Set the value in the index first since we don't care if it's out of date */
    if (!set(&srch, &idxtab, (void *)&data))
	return DBZSTORE_ERROR;
    if (!set(&srch, &etab, &evalue))
	return DBZSTORE_ERROR;
    return DBZSTORE_OK;
#endif	/* DO_TAGGED_HASH */
}

/*
 * getconf - get configuration from .dir file
 *   df    - NULL means just give me the default 
 *   pf    - NULL means don't care about .pag 
 *   returns true for success, false for failure
 */
static bool
getconf(FILE *df, dbzconfig *cp)
{
    int		i;

    /* empty file, no configuration known */
#ifdef	DO_TAGGED_HASH
    if (df == NULL) {
	cp->tsize = DEFSIZE;
	for (i = 0; i < NUSEDS; i++)
	    cp->used[i] = 0;
	for (i = 0; i < NUSEDS; i++)
	    cp->vused[i] = 0;
	cp->valuesize = sizeof(of_t);
	cp->fillpercent = 50;
	cp->tagenb = TAGENB;
	cp->tagmask = TAGMASK;
	cp->tagshift = TAGSHIFT;
	cp->dropbits = 0;
	cp->lenfuzzy = 0;
	DEBUG(("getconf: defaults (%ld, %c, (0x%lx/0x%lx<<%d %d))\n",
	cp->tsize, cp->casemap, cp->tagenb, 
	cp->tagmask, cp->tagshift, cp->dropbits));
	return true;
    }

    i = fscanf(df, "dbz 6 %ld %d %d %ld %ld %d %d\n", &cp->tsize,
		&cp->valuesize, &cp->fillpercent, &cp->tagenb,
		&cp->tagmask, &cp->tagshift, &cp->dropbits);
    if (i != 7) {
	DEBUG(("getconf error"));
	return false;
    }
    if (cp->valuesize != sizeof(of_t)) {
	DEBUG(("getconf: wrong of_t size (%d)\n", cp->valuesize));
	return false;
    }
    cp->lenfuzzy = (int)(1 << cp->dropbits) - 1;
#else	/* DO_TAGGED_HASH */
    if (df == NULL) {
	cp->tsize = DEFSIZE;
	for (i = 0; i < NUSEDS; i++)
	    cp->used[i] = 0;
	cp->valuesize = sizeof(of_t) + sizeof(erec);
	cp->fillpercent = 66;
	DEBUG(("getconf: defaults (%ld)\n", cp->tsize));
	return true;
    }

    i = fscanf(df, "dbz 6 %ld %d %d\n", &cp->tsize,
		&cp->valuesize, &cp->fillpercent);
    if (i != 3) {
	DEBUG(("getconf error"));
	return false;
    }
    if (cp->valuesize != (sizeof(of_t) + sizeof(erec))) {
	DEBUG(("getconf: wrong of_t size (%d)\n", cp->valuesize));
	return false;
    }
#endif	/* DO_TAGGED_HASH */
    DEBUG(("size %ld\n", cp->tsize));

    /* second line, the usages */
    for (i = 0; i < NUSEDS; i++)
	if (!fscanf(df, "%ld", &cp->used[i])) {
	    DEBUG(("getconf error\n"));
	    return false;
	}
    DEBUG(("used %ld %ld %ld...\n", cp->used[0], cp->used[1], cp->used[2]));

#ifdef	DO_TAGGED_HASH
    /* third line, the text usages */
    for (i = 0; i < NUSEDS; i++)
	if (!fscanf(df, "%ld", &cp->vused[i])) {
	    DEBUG(("getconf error\n"));
	    return false;
	}
    DEBUG(("vused %ld %ld %ld...\n", cp->vused[0], cp->vused[1], cp->vused[2]));
#endif	/* DO_TAGGED_HASH */

    return true;
}

/* putconf - write configuration to .dir file
 * Returns: 0 for success, -1 for failure
 */
static int
putconf(FILE *f, dbzconfig *cp)
{
    int i;
    int ret = 0;

    if (fseeko(f, 0, SEEK_SET) != 0) {
	DEBUG(("fseeko failure in putconf\n"));
	ret = -1;
    }

#ifdef	DO_TAGGED_HASH
    fprintf(f, "dbz %d %ld %d %d %ld %ld %d %d\n", dbzversion, cp->tsize,
		cp->valuesize, cp->fillpercent, cp->tagenb,
		cp->tagmask, cp->tagshift, cp->dropbits);
#else	/* DO_TAGGED_HASH */
    fprintf(f, "dbz %d %ld %d %d\n", dbzversion, cp->tsize,
		   cp->valuesize, cp->fillpercent);
#endif	/* DO_TAGGED_HASH */

    for (i = 0; i < NUSEDS; i++)
	fprintf(f, "%ld%c", cp->used[i], (i < NUSEDS-1) ? ' ' : '\n');

#ifdef	DO_TAGGED_HASH
    for (i = 0; i < NUSEDS; i++)
	fprintf(f, "%ld%c", cp->vused[i], (i < NUSEDS-1) ? ' ' : '\n');
#endif

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
static bool
getcore(hash_table *tab)
{
    char *it;
    int nread;
    int i;
    struct stat st;

    if (tab->incore == INCORE_MMAP) {
#if defined(HAVE_MMAP)
	if (fstat(tab->fd, &st) == -1) {
	    DEBUG(("getcore: fstat failed\n"));
	    return false;
	}
	if ((conf.tsize * tab->reclen) > st.st_size) {
	    /* file too small; extend it */
	    if (ftruncate(tab->fd, conf.tsize * tab->reclen) == -1) {
		DEBUG(("getcore: ftruncate failed\n"));
		return false;
	    }
	}
	it = mmap(NULL, (size_t)conf.tsize * tab->reclen,
                  readonly ? PROT_READ : PROT_WRITE | PROT_READ, MAP_SHARED,
                  tab->fd, 0);
	if (it == (char *)-1) {
	    DEBUG(("getcore: mmap failed\n"));
	    return false;
	}
#if defined (MADV_RANDOM) && defined(HAVE_MADVISE)
	/* not present in all versions of mmap() */
	madvise(it, (size_t)conf.tsize * sizeof(tab->reclen), MADV_RANDOM);
#endif
#else
	DEBUG(("getcore: can't mmap files\n"));
	return false;
#endif
    } else {
	it = xmalloc(conf.tsize * tab->reclen);
	
	nread = read(tab->fd, it, tab->reclen * conf.tsize);
	if (nread < 0) {
	    DEBUG(("getcore: read failed\n"));
	    free(it);
	    return false;
	}
	
	i = (size_t)(conf.tsize * tab->reclen) - nread;
	memset(it + nread, '\0', i);
    }

    tab->core = it;
    return true;
}

/* putcore - try to rewrite an in-core table
 *
 * Returns true on success, false on failure
 */
static bool
putcore(hash_table *tab)
{
    int size;
    
    if (tab->incore == INCORE_MEM) {
	nonblocking(tab->fd, false);
	size = tab->reclen * conf.tsize;
	if (pwrite(tab->fd, tab->core, size, 0) != size) {
	    nonblocking(tab->fd, options.nonblock);
	    return false;
	}
	nonblocking(tab->fd, options.nonblock);
    }
    return true;
}

#ifdef	DO_TAGGED_HASH
/*
 - makehash31 : make 31-bit hash from HASH
 */
static unsigned int
makehash31(const HASH *hash)
{
    unsigned int h;
    memcpy(&h, hash, sizeof(h));
    return (h >> 1);
}
#endif

/* start - set up to start or restart a search
 * osp == NULL is acceptable
 */
static void
start(searcher *sp, const HASH hash, searcher *osp)
{
#ifdef	DO_TAGGED_HASH
    unsigned int	h;

    h = makehash31(&hash);
    if (osp != FRESH && osp->shorthash == h) {
	if (sp != osp)
	    *sp = *osp;
	sp->run--;
	DEBUG(("search restarted\n"));
    } else {
	sp->shorthash = h;
	sp->tag = MKTAG(h / conf.tsize);
	sp->place = h % conf.tsize;
	DEBUG(("hash %8.8lx tag %8.8lx place %ld\n",
	    sp->shorthash, sp->tag, sp->place));
	sp->tabno = 0;
	sp->run = -1;
	sp->aborted = 0;
    }

#else	/* DO_TAGGED_HASH */
    int tocopy;

    if (osp != FRESH && !memcmp(&osp->hash, &hash, sizeof(hash))) {
	if (sp != osp)
	    *sp = *osp;
	sp->run--;
	DEBUG(("search restarted\n"));
    } else {
	sp->hash = hash;
	tocopy = sizeof(hash) < sizeof(sp->shorthash) ? sizeof(hash) : sizeof(sp->shorthash);
	/* Copy the bottom half of thhe hash into sp->shorthash */
	memcpy(&sp->shorthash, (const char *)&hash + (sizeof(hash) - tocopy),
               tocopy);
	sp->shorthash >>= 1;
	sp->tabno = 0;
	sp->run = -1;
	sp->aborted = 0;
    }
#endif	/* DO_TAGGED_HASH */
}

#ifdef	DO_TAGGED_HASH
/*
 - search - conduct part of a search
 */
static of_t			/* NOTFOUND if we hit VACANT or error */
search(searcher *sp)
{
    of_t value;
    unsigned long taboffset = sp->tabno * conf.tsize;

    if (sp->aborted)
	return(NOTFOUND);

    for (;;) {
	/* go to next location */
	if (sp->run++ == MAXRUN) {
	    sp->tabno++;
	    sp->run = 0;
	    taboffset = sp->tabno * conf.tsize;
	}
	sp->place = ((sp->shorthash + sp->run) % conf.tsize) + taboffset;
	DEBUG(("search @ %ld\n", place));

	/* get the tagged value */
	if ((options.pag_incore != INCORE_NO) && (sp->place < conf.tsize)) {
	    DEBUG(("search: in core\n"));
	    value = ((of_t *)pagtab.core)[sp->place];
	} else {
	    off_t dest;
	    dest = sp->place * SOF;

	    /* read it */
	    errno = 0;
	    if (pread(pagtab.fd, &value, sizeof(value), dest) != sizeof(value)) {
		if (errno != 0) {
		    DEBUG(("search: read failed\n"));
		    pagtab.pos = -1;
		    sp->aborted = 1;
		    return(NOTFOUND);
		} else
		    value = VACANT;
                pagtab.pos = -1;
	    } else
                pagtab.pos += sizeof(value);
	}

	/* vacant slot is always cause to return */
	if (value == VACANT) {
	    DEBUG(("search: empty slot\n"));
	    return(NOTFOUND);
	};

	/* check the tag */
	value = UNBIAS(value);
	DEBUG(("got 0x%lx\n", value));
	if (!HASTAG(value)) {
	    DEBUG(("tagless\n"));
	    return(value);
	} else if (TAG(value) == sp->tag) {
	    DEBUG(("match\n"));
	    return(NOTAG(value));
	} else {
	    DEBUG(("mismatch 0x%lx\n", TAG(value)));
	}
    }
    /* NOTREACHED */
}

#else	/* DO_TAGGED_HASH */

/* search - conduct part of a search
 *
 * return false if we hit vacant rec's or error
 */
static bool
search(searcher *sp)
{
    erec value;
    unsigned long taboffset = 0;

    if (sp->aborted)
	return false;

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
	    off_t dest;
	    dest = sp->place * sizeof(erec);

	    /* read it */
	    errno = 0;
	    if (pread(etab.fd, &value, sizeof(erec), dest) != sizeof(erec)) {
		if (errno != 0) {
		    DEBUG(("search: read failed\n"));
		    etab.pos = -1;
		    sp->aborted = 1;
		    return false;
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
	    return false;
	}

	/* check the value */
	DEBUG(("got 0x%lx\n", value));
	if (!memcmp(&value.hash, &sp->hash, DBZ_INTERNAL_HASH_SIZE)) {
	    return true;
	}
    }
    /* NOTREACHED */
}
#endif	/* DO_TAGGED_HASH */

/* set - store a value into a location previously found by search
 *
 * Returns:  true success, false failure
 */
static bool
set(searcher *sp, hash_table *tab, void *value)
{
    off_t offset;
    
    if (sp->aborted)
	return false;

    /* If we have the index file in memory, use it */
    if ((tab->incore != INCORE_NO) && (sp->place < conf.tsize)) {
	void *where = (char *)tab->core + (sp->place * tab->reclen);

	memcpy(where, value, tab->reclen);
	DEBUG(("set: incore\n"));
	if (tab->incore == INCORE_MMAP) {
	    if (innconf->nfswriter) {
		mapcntl(where, tab->reclen, MS_ASYNC);
	    }
	    return true;
	}
	if (!options.writethrough)
	    return true;
    }

    /* seek to spot */
    tab->pos = -1;		/* invalidate position memory */
    offset = sp->place * tab->reclen;

    /* write in data */
    while (pwrite(tab->fd, value, tab->reclen, offset) != tab->reclen) {
	if (errno == EAGAIN) {
	    fd_set writeset;
	    
	    FD_ZERO(&writeset);
	    FD_SET(tab->fd, &writeset);
	    if (select(tab->fd + 1, NULL, &writeset, NULL, NULL) < 1) {
		DEBUG(("set: select failed\n"));
		sp->aborted = 1;
		return false;
	    }
	    continue;
	}
	DEBUG(("set: write failed\n"));
	sp->aborted = 1;
	return false;
    }

    DEBUG(("set: succeeded\n"));
    return true;
}

#ifdef	DO_TAGGED_HASH
/*
 - set_pag - store a value into a location previously found by search
 -       on the pag table.
 - Returns: true success, false failure
 */
static bool
set_pag(searcher *sp, of_t value)
{
    of_t v = value;

    if (CANTAG(v)) {
	v |= sp->tag | taghere;
	if (v != UNBIAS(VACANT))	/* BIAS(v) won't look VACANT */
#ifdef OVERFLOW
	    if (v != LONG_MAX)		/* and it won't overflow */
#endif
		value = v;
    } else if (canttag_warned == 0) {
	fprintf(stderr, "dbz.c(set): can't tag value 0x%lx", v);
	fprintf(stderr, " tagboth = 0x%lx\n", tagboth);
	canttag_warned = 1;
    }
    DEBUG(("tagged value is 0x%lx\n", value));
    value = BIAS(value);

    return set(sp, &pagtab, &value);
}
#endif	/* DO_TAGGED_HASH */

/* dbzsetoptions - set runtime options for the database.
 */
void
dbzsetoptions(const dbzoptions o)
{
    options = o;
#ifndef HAVE_MMAP
    /* Without a working mmap on files, we should avoid it. */
    if (options.pag_incore == INCORE_MMAP) options.pag_incore = INCORE_NO;
    if (options.exists_incore == INCORE_MMAP) options.exists_incore = INCORE_NO;
#endif
}

/* dbzgetoptions - get runtime options for the database.
 */
void
dbzgetoptions(dbzoptions *o)
{
    *o = options;
}


/* dbzdebug - control dbz debugging at run time
 * Returns: old value for dbzdebug
 */
#ifdef DBZDEBUG
int dbzdebug(bool value) {
    bool old = debug;

    debug = value;
    return old;
}
#endif

#ifdef DBZTEST

int
timediffms(struct timeval start, struct timeval end)
{
    return (((end.tv_sec - start.tv_sec) * 1000) +
	    ((end.tv_usec - start.tv_usec)) / 1000);
}

void
RemoveDBZ(char *filename)
{
    char fn[1024];

#ifdef	DO_TAGGED_HASH
    snprintf(fn, sizeof(fn), "%s.pag", filename);
    unlink(fn);
#else
    snprintf(fn, sizeof(fn), "%s.exists", filename);
    unlink(fn);
    snprintf(fn, sizeof(fn), "%s.index", filename);
    unlink(fn);
#endif
    snprintf(fn, sizeof(fn), "%s.dir", filename);
    unlink(fn);
}

static void
usage(void)
{
    fprintf(stderr, "usage: dbztest [-i] [-n|m] [-s size] <history>\n");
    fprintf(stderr, "  -i       initialize history. deletes .pag files\n");
    fprintf(stderr, "  -n or m  use INCORE_NO, INCORE_/MMAP. default = INCORE_MEM\n");
    fprintf(stderr, "  -s size  number of history lines[2500000]\n");
    fprintf(stderr, "  history  history text file\n");
    exit(1);
}

int
main(int argc, char *argv[])
{
    int  i, line;
    FILE *fpi;
    char ibuf[2048], *p;
    HASH key;
    off_t where;
    int  initialize = 0, size = 2500000;
    char *history = NULL;
    dbzoptions opt;
    dbz_incore_val incore = INCORE_MEM;
    struct timeval start, end;
    off_t ivalue;

    for (i=1; i<argc; i++)
	if (strcmp(argv[i], "-i") == 0)
	    initialize = 1;
	else if (strcmp(argv[i], "-n") == 0)
	    incore = INCORE_NO;
	else if (strcmp(argv[i], "-m") == 0)
#if defined(HAVE_MMAP)
	    incore = INCORE_MMAP;
#else
            fprintf (stderr, "can't mmap files\n");
#endif
	else if (strcmp(argv[i], "-s") == 0)
	    size = atoi(argv[++i]);
	else if (*argv[i] != '-' && history == NULL)
	    history = argv[i];
	else
	    usage();

    if (history == NULL)
	usage();
    if ((fpi = fopen(history, "r")) == NULL) {
	fprintf(stderr, "can't open %s\n", history);
	usage();
    }

    dbzgetoptions(&opt);
    opt.pag_incore = incore;
    dbzsetoptions(opt);

    if (initialize) {
	RemoveDBZ(history);
	gettimeofday(&start, NULL);
	if (dbzfresh(history, dbzsize(size)) < 0) {
	    fprintf(stderr, "cant dbzfresh %s\n", history);
	    exit(1);
	}
	gettimeofday(&end, NULL);
	printf("dbzfresh: %d msec\n", timediffms(start, end));
    } else {
#ifdef	DBZDEBUG
	dbzdebug(1);
#endif
	gettimeofday(&start, NULL);
	if (dbzinit(history) < 0) {
	    fprintf(stderr, "cant dbzinit %s\n", history);
	    exit(1);
	}
	gettimeofday(&end, NULL);
	printf("dbzinit: %d msec\n", timediffms(start, end));
    }

    gettimeofday(&start, NULL);
    where = ftello(fpi);
    for (line=1; fgets(ibuf, sizeof(ibuf), fpi); line++, where=ftello(fpi)) {
	if (*ibuf == '<') {
	    if ((p = strchr(ibuf, '\t')) == NULL) {
		fprintf(stderr, "ignoreing bad line: %s\n", ibuf);
		continue;
	    }
	    *p = '\0';
	    key = HashMessageID(ibuf);
	} else if (*ibuf == '[')
	    key = TextToHash(ibuf+1);
	else
	    continue;
	if (initialize) {
	    if (dbzstore(key, where) < 0) {
		fprintf(stderr, "cant store %s\n", ibuf);
		exit(1);
	    }
	} else {
	    if (!dbzfetch(key, &ivalue)) {
		    fprintf(stderr, "line %d can't fetch %s\n", line, ibuf);
		    exit(1);
		}
	}
    }
    gettimeofday(&end, NULL);
    i = timediffms(start, end);
    printf("%s: %d lines %.3f msec/id\n",
	(initialize) ? "dbzstore" : "dbzfetch",
	line, (double)i / (double)line);

    gettimeofday(&end, NULL);
    dbzclose();
    gettimeofday(&end, NULL);
    printf("dbzclose: %d msec\n", timediffms(start, end));
    return(0);
}
#endif	/* DBZTEST */
