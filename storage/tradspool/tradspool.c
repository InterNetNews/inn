/*  $Id$
**
**  Storage manager module for traditional spool format.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <time.h>

/* Needed for htonl() and friends on AIX 4.1. */
#include <netinet/in.h>
    
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/qio.h"
#include "inn/wire.h"
#include "libinn.h"
#include "paths.h"

#include "methods.h"
#include "tradspool.h"

typedef struct {
    char		*artbase; /* start of the article data -- may be mmaped */
    unsigned int	artlen; /* art length. */
    int 	nextindex;
    char		*curdirname;
    DIR			*curdir;
    struct _ngtent	*ngtp;
    bool 		mmapped;
} PRIV_TRADSPOOL;

/*
** The 64-bit hashed representation of a ng name that gets stashed in each token. 
*/

#define HASHEDNGLEN 8
typedef struct {
    char hash[HASHEDNGLEN];
} HASHEDNG;

/*
** We have two structures here for facilitating newsgroup name->number mapping
** and number->name mapping.  NGTable is a hash table based on hashing the
** newsgroup name, and is used to give the name->number mapping.  NGTree is
** a binary tree, indexed by newsgroup number, used for the number->name
** mapping.
*/

#define NGT_SIZE  2048

typedef struct _ngtent {
    char *ngname;
/*    HASHEDNG hash; XXX */
    unsigned long ngnumber;
    struct _ngtent *next;
    struct _ngtreenode *node;
} NGTENT;

typedef struct _ngtreenode {
    unsigned long ngnumber;
    struct _ngtreenode *left, *right;
    NGTENT *ngtp;
} NGTREENODE;

NGTENT *NGTable[NGT_SIZE];
unsigned long MaxNgNumber = 0;
NGTREENODE *NGTree;

bool NGTableUpdated; /* set to true if we've added any entries since reading 
			in the database file */

/* 
** Convert all .s to /s in a newsgroup name.  Modifies the passed string 
** inplace.
*/
static void
DeDotify(char *ngname) {
    char *p = ngname;

    for ( ; *p ; ++p) {
	if (*p == '.') *p = '/';
    }
    return;
}

/*
** Hash a newsgroup name to an 8-byte.  Basically, we convert all .s to 
** /s (so it doesn't matter if we're passed the spooldir name or newsgroup
** name) and then call Hash to MD5 the mess, then take 4 bytes worth of 
** data from the front of the hash.  This should be good enough for our
** purposes. 
*/

static HASHEDNG 
HashNGName(char *ng) {
    HASH hash;
    HASHEDNG return_hash;
    char *p;

    p = xstrdup(ng);
    DeDotify(p);
    hash = Hash(p, strlen(p));
    free(p);

    memcpy(return_hash.hash, hash.hash, HASHEDNGLEN);

    return return_hash;
}

#if 0 /* XXX */
/* compare two hashes */
static int
CompareHash(HASHEDNG *h1, HASHEDNG *h2) {
    int i;
    for (i = 0 ; i < HASHEDNGLEN ; ++i) {
	if (h1->hash[i] != h2->hash[i]) {
	    return h1->hash[i] - h2->hash[i];
	}
    }
    return 0;
}
#endif

/* Add a new newsgroup name to the NG table. */
static void
AddNG(char *ng, unsigned long number) {
    char *p;
    unsigned int h;
    HASHEDNG hash;
    NGTENT *ngtp, **ngtpp;
    NGTREENODE *newnode, *curnode, **nextnode;

    p = xstrdup(ng);
    DeDotify(p); /* canonicalize p to standard (/) form. */
    hash = HashNGName(p);

    h = (unsigned char)hash.hash[0];
    h = h + (((unsigned char)hash.hash[1])<<8);

    h = h % NGT_SIZE;

    ngtp = NGTable[h];
    ngtpp = &NGTable[h];
    while (true) {
	if (ngtp == NULL) {
	    /* ng wasn't in table, add new entry. */
	    NGTableUpdated = true;

	    ngtp = xmalloc(sizeof(NGTENT));
	    ngtp->ngname = p; /* note: we store canonicalized name */
	    /* ngtp->hash = hash XXX */
	    ngtp->next = NULL;

	    /* assign a new NG number if needed (not given) */
	    if (number == 0) {
		number = ++MaxNgNumber;
	    }
	    ngtp->ngnumber = number;

	    /* link new table entry into the hash table chain. */
	    *ngtpp = ngtp;

	    /* Now insert an appropriate record into the binary tree */
	    newnode = xmalloc(sizeof(NGTREENODE));
	    newnode->left = newnode->right = (NGTREENODE *) NULL;
	    newnode->ngnumber = number;
	    newnode->ngtp = ngtp;
	    ngtp->node = newnode;

	    if (NGTree == NULL) {
		/* tree was empty, so put our one element in and return */
		NGTree = newnode;
		return;
	    } else {
		nextnode = &NGTree;
		while (*nextnode) {
		    curnode = *nextnode;
		    if (curnode->ngnumber < number) {
			nextnode = &curnode->right;
		    } else if (curnode->ngnumber > number) {
			nextnode = &curnode->left;
		    } else {
			/* Error, same number is already in NGtree (shouldn't
                           happen!) */
                        warn("tradspool: AddNG: duplicate newsgroup number in"
                             " NGtree: %ld (%s)", number, p);
			return;
		    }
		}
		*nextnode = newnode;
		return;
	    }
	} else if (strcmp(ngtp->ngname, p) == 0) {
	    /* entry in table already, so return */
	    free(p);
	    return;
#if 0 /* XXX */
	} else if (CompareHash(&ngtp->hash, &hash) == 0) {
	    /* eep! we hit a hash collision. */
            warn("tradspool: AddNG: hash collision %s/%s", ngtp->ngname, p);
	    free(p);
	    return;
#endif 
	} else {
	    /* not found yet, so advance to next entry in chain */
	    ngtpp = &(ngtp->next);
	    ngtp = ngtp->next;
	}
    }
}

/* find a newsgroup table entry, given only the name. */
static NGTENT *
FindNGByName(char *ngname) {
    NGTENT *ngtp;
    unsigned int h;
    HASHEDNG hash;
    char *p;

    p = xstrdup(ngname);
    DeDotify(p); /* canonicalize p to standard (/) form. */
    hash = HashNGName(p);

    h = (unsigned char)hash.hash[0];
    h = h + (((unsigned char)hash.hash[1])<<8);

    h = h % NGT_SIZE;

    ngtp = NGTable[h];

    while (ngtp) {
	if (strcmp(p, ngtp->ngname) == 0) {
	    free(p);
	    return ngtp;
	}
	ngtp = ngtp->next;
    }
    free(p);
    return NULL; 
}

/* find a newsgroup/spooldir name, given only the newsgroup number */
static char *
FindNGByNum(unsigned long ngnumber) {
    NGTENT *ngtp;
    NGTREENODE *curnode;

    curnode = NGTree;

    while (curnode) {
	if (curnode->ngnumber == ngnumber) {
	    ngtp = curnode->ngtp;
	    return ngtp->ngname;
	}
	if (curnode->ngnumber < ngnumber) {
	    curnode = curnode->right;
	} else {
	    curnode = curnode->left;
	}
    }
    /* not in tree, return NULL */
    return NULL; 
}

#define _PATH_TRADSPOOLNGDB "tradspool.map"
#define _PATH_NEWTSNGDB "tradspool.map.new"


/* dump DB to file. */
static void
DumpDB(void)
{
    char *fname, *fnamenew;
    NGTENT *ngtp;
    unsigned int i;
    FILE *out;

    if (!SMopenmode) return; /* don't write if we're not in read/write mode. */
    if (!NGTableUpdated) return; /* no need to dump new DB */

    fname = concatpath(innconf->pathspool, _PATH_TRADSPOOLNGDB);
    fnamenew = concatpath(innconf->pathspool, _PATH_NEWTSNGDB);

    if ((out = fopen(fnamenew, "w")) == NULL) {
        syswarn("tradspool: DumpDB: can't write %s", fnamenew);
	free(fname);
	free(fnamenew);
	return;
    }
    for (i = 0 ; i < NGT_SIZE ; ++i) {
	ngtp = NGTable[i];
	for ( ; ngtp ; ngtp = ngtp->next) {
	    fprintf(out, "%s %lu\n", ngtp->ngname, ngtp->ngnumber);
	}
    }
    if (fclose(out) < 0) {
        syswarn("tradspool: DumpDB: can't close %s", fnamenew);
	free(fname);
	free(fnamenew);
	return;
    }
    if (rename(fnamenew, fname) < 0) {
        syswarn("tradspool: DumpDB: can't rename %s", fnamenew);
	free(fname);
	free(fnamenew);
	return;
    }
    free(fname);
    free(fnamenew);
    NGTableUpdated = false; /* reset modification flag. */
    return;
}

/* 
** init NGTable from saved database file and from active.  Note that
** entries in the database file get added first,  and get their specifications
** of newsgroup number from there. 
*/

static bool
ReadDBFile(void)
{
    char *fname;
    QIOSTATE *qp;
    char *line;
    char *p;
    unsigned long number;

    fname = concatpath(innconf->pathspool, _PATH_TRADSPOOLNGDB);
    if ((qp = QIOopen(fname)) == NULL) {
	/* only warn if db not found. */
        notice("tradspool: mapping file %s not found", fname);
    } else {
	while ((line = QIOread(qp)) != NULL) {
	    p = strchr(line, ' ');
	    if (p == NULL) {
                warn("tradspool: corrupt line in active: %s", line);
		QIOclose(qp);
		free(fname);
		return false;
	    }
	    *p++ = 0;
	    number = atol(p);
	    AddNG(line, number);
	    if (MaxNgNumber < number) MaxNgNumber = number;
	}
	QIOclose(qp);
    }
    free(fname);
    return true;
}

static bool
ReadActiveFile(void)
{
    char *fname;
    QIOSTATE *qp;
    char *line;
    char *p;

    fname = concatpath(innconf->pathdb, _PATH_ACTIVE);
    if ((qp = QIOopen(fname)) == NULL) {
        syswarn("tradspool: can't open %s", fname);
	free(fname);
	return false;
    }

    while ((line = QIOread(qp)) != NULL) {
	p = strchr(line, ' ');
	if (p == NULL) {
            syswarn("tradspool: corrupt line in active: %s", line);
	    QIOclose(qp);
	    free(fname);
	    return false;
	}
	*p = 0;
	AddNG(line, 0);
    }
    QIOclose(qp);
    free(fname);
    /* dump any newly added changes to database */
    DumpDB();
    return true;
}

static bool
InitNGTable(void)
{
    if (!ReadDBFile()) return false;

    /*
    ** set NGTableUpdated to false; that way we know if the load of active or
    ** any AddNGs later on did in fact add new entries to the db.
    */
    NGTableUpdated = false; 
    if (!SMopenmode)
	/* don't read active unless write mode. */
	return true;
    return ReadActiveFile(); 
}

/* 
** Routine called to check every so often to see if we need to reload the
** database and add in any new groups that have been added.   This is primarily
** for the benefit of innfeed in funnel mode, which otherwise would never
** get word that any new newsgroups had been added. 
*/

#define RELOAD_TIME_CHECK 600

static void
CheckNeedReloadDB(bool force)
{
    static time_t lastcheck, oldlastcheck, now;
    struct stat sb;
    char *fname;

    now = time(NULL);
    if (!force && lastcheck + RELOAD_TIME_CHECK > now)
        return;

    oldlastcheck = lastcheck;
    lastcheck = now;

    fname = concatpath(innconf->pathspool, _PATH_TRADSPOOLNGDB);
    if (stat(fname, &sb) < 0) {
	free(fname);
	return;
    }
    free(fname);
    if (sb.st_mtime > oldlastcheck) {
	/* add any newly added ngs to our in-memory copy of the db. */
	ReadDBFile();
    }
}

/* Init routine, called by SMinit */

bool
tradspool_init(SMATTRIBUTE *attr) {
    if (attr == NULL) {
        warn("tradspool: attr is NULL");
	SMseterror(SMERR_INTERNAL, "attr is NULL");
	return false;
    }
    if (!innconf->storeonxref) {
        warn("tradspool: storeonxref needs to be true");
	SMseterror(SMERR_INTERNAL, "storeonxref needs to be true");
	return false;
    }
    attr->selfexpire = false;
    attr->expensivestat = true;
    return InitNGTable();
}

/* Make a token for an article given the primary newsgroup name and article # */
static TOKEN
MakeToken(char *ng, unsigned long artnum, STORAGECLASS class) {
    TOKEN token;
    NGTENT *ngtp;
    unsigned long num;

    memset(&token, '\0', sizeof(token));

    token.type = TOKEN_TRADSPOOL;
    token.class = class;

    /* 
    ** if not already in the NG Table, be sure to add this ng! This way we
    ** catch things like newsgroups added since startup. 
    */
    if ((ngtp = FindNGByName(ng)) == NULL) {
	AddNG(ng, 0);
	DumpDB(); /* flush to disk so other programs can see the change */
	ngtp = FindNGByName(ng);
    } 

    num = ngtp->ngnumber;
    num = htonl(num);

    memcpy(token.token, &num, sizeof(num));
    artnum = htonl(artnum);
    memcpy(&token.token[sizeof(num)], &artnum, sizeof(artnum));
    return token;
}

/* 
** Convert a token back to a pathname. 
*/
static char *
TokenToPath(TOKEN token) {
    unsigned long ngnum;
    unsigned long artnum;
    char *ng, *path;
    size_t length;

    CheckNeedReloadDB(false);

    memcpy(&ngnum, &token.token[0], sizeof(ngnum));
    memcpy(&artnum, &token.token[sizeof(ngnum)], sizeof(artnum));
    artnum = ntohl(artnum);
    ngnum = ntohl(ngnum);

    ng = FindNGByNum(ngnum);
    if (ng == NULL) {
	CheckNeedReloadDB(true);
	ng = FindNGByNum(ngnum);
	if (ng == NULL)
	    return NULL;
    }

    length = strlen(ng) + 20 + strlen(innconf->patharticles);
    path = xmalloc(length);
    snprintf(path, length, "%s/%s/%lu", innconf->patharticles, ng, artnum);
    return path;
}

/*
** Crack an Xref line apart into separate strings, each of the form "ng:artnum".
** Return in "num" the number of newsgroups found. 
*/
static char **
CrackXref(char *xref, unsigned int *lenp) {
    char *p;
    char **xrefs;
    char *q;
    unsigned int len, xrefsize;

    len = 0;
    xrefsize = 5;
    xrefs = xmalloc(xrefsize * sizeof(char *));

    /* no path element should exist, nor heading white spaces exist */
    p = xref;
    while (true) {
	/* check for EOL */
	/* shouldn't ever hit null w/o hitting a \r\n first, but best to be paranoid */
	if (*p == '\n' || *p == '\r' || *p == 0) {
	    /* hit EOL, return. */
	    *lenp = len;
	    return xrefs;
	}
	/* skip to next space or EOL */
	for (q=p; *q && *q != ' ' && *q != '\n' && *q != '\r' ; ++q) ;

        xrefs[len] = xstrndup(p, q - p);

	if (++len == xrefsize) {
	    /* grow xrefs if needed. */
	    xrefsize *= 2;
            xrefs = xrealloc(xrefs, xrefsize * sizeof(char *));
	}

 	p = q;
	/* skip spaces */
	for ( ; *p == ' ' ; p++) ;
    }
}

TOKEN
tradspool_store(const ARTHANDLE article, const STORAGECLASS class) {
    char **xrefs;
    char *xrefhdr;
    TOKEN token;
    unsigned int numxrefs;
    char *ng, *p, *onebuffer;
    unsigned long artnum;
    char *path, *linkpath, *dirname;
    int fd;
    size_t used;
    char *nonwfarticle; /* copy of article converted to non-wire format */
    unsigned int i;
    size_t length, nonwflen;
    
    xrefhdr = article.groups;
    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
	token.type = TOKEN_EMPTY;
	SMseterror(SMERR_UNDEFINED, "bogus Xref: header");
	if (xrefs != NULL)
	    free(xrefs);
	return token;
    }

    if ((p = strchr(xrefs[0], ':')) == NULL) {
	token.type = TOKEN_EMPTY;
	SMseterror(SMERR_UNDEFINED, "bogus Xref: header");
	for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
	free(xrefs);
	return token;
    }
    *p++ = '\0';
    ng = xrefs[0];
    DeDotify(ng);
    artnum = atol(p);
    
    token = MakeToken(ng, artnum, class);

    length = strlen(innconf->patharticles) + strlen(ng) + 32;
    path = xmalloc(length);
    snprintf(path, length, "%s/%s/%lu", innconf->patharticles, ng, artnum);

    /* following chunk of code boldly stolen from timehash.c  :-) */
    if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
	p = strrchr(path, '/');
	*p = '\0';
	if (!MakeDirectory(path, true)) {
            syswarn("tradspool: could not create directory %s", path);
	    token.type = TOKEN_EMPTY;
	    free(path);
	    SMseterror(SMERR_UNDEFINED, NULL);
	    for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
	    free(xrefs);
	    return token;
	} else {
	    *p = '/';
	    if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
		SMseterror(SMERR_UNDEFINED, NULL);
                syswarn("tradspool: could not open %s", path);
		token.type = TOKEN_EMPTY;
		free(path);
		for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
		free(xrefs);
		return token;
	    }
	}
    }
    if (innconf->wireformat) {
	if (xwritev(fd, article.iov, article.iovcnt) != (ssize_t) article.len) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("tradspool: error writing to %s", path);
	    close(fd);
	    token.type = TOKEN_EMPTY;
	    unlink(path);
	    free(path);
	    for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
	    free(xrefs);
	    return token;
	}
    } else {
	onebuffer = xmalloc(article.len);
	for (used = i = 0 ; i < (unsigned int) article.iovcnt ; i++) {
	    memcpy(&onebuffer[used], article.iov[i].iov_base, article.iov[i].iov_len);
	    used += article.iov[i].iov_len;
	}
	nonwfarticle = wire_to_native(onebuffer, used, &nonwflen);
	free(onebuffer);
	if (write(fd, nonwfarticle, nonwflen) != (ssize_t) nonwflen) {
	    free(nonwfarticle);
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("tradspool: error writing to %s", path);
	    close(fd);
	    token.type = TOKEN_EMPTY;
	    unlink(path);
	    free(path);
	    for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
	    free(xrefs);
	    return token;
	}
	free(nonwfarticle);
    }
    close(fd);

    /* 
    ** blah, this is ugly.  Have to make symlinks under other pathnames for
    ** backwards compatiblility purposes. 
    */

    if (numxrefs > 1) {
	for (i = 1; i < numxrefs ; ++i) {
	    if ((p = strchr(xrefs[i], ':')) == NULL) continue;
	    *p++ = '\0';
	    ng = xrefs[i];
	    DeDotify(ng);
	    artnum = atol(p);

            length = strlen(innconf->patharticles) + strlen(ng) + 32;
	    linkpath = xmalloc(length);
	    snprintf(linkpath, length, "%s/%s/%lu", innconf->patharticles,
                     ng, artnum);
	    if (link(path, linkpath) < 0) {
		p = strrchr(linkpath, '/');
		*p = '\0';
		dirname = xstrdup(linkpath);
		*p = '/';
		if (!MakeDirectory(dirname, true) || link(path, linkpath) < 0) {
		    if (symlink(path, linkpath) < 0) {
			SMseterror(SMERR_UNDEFINED, NULL);
                        syswarn("tradspool: could not symlink %s to %s",
                                path, linkpath);
			token.type = TOKEN_EMPTY;
			free(dirname);
			free(linkpath);
			free(path);
			for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
			free(xrefs);
			return token;
		    }
		}
		free(dirname);
	    }
	    free(linkpath);
	}
    }
    free(path);
    for (i = 0 ; i < numxrefs; ++i) free(xrefs[i]);
    free(xrefs);
    return token;
}

static ARTHANDLE *
OpenArticle(const char *path, RETRTYPE amount) {
    int fd;
    PRIV_TRADSPOOL *private;
    char *p;
    struct stat sb;
    ARTHANDLE *art;
    char *wfarticle;
    size_t wflen;

    if (amount == RETR_STAT) {
	if (access(path, R_OK) < 0) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    return NULL;
	}
	art = xmalloc(sizeof(ARTHANDLE));
	art->type = TOKEN_TRADSPOOL;
	art->data = NULL;
	art->len = 0;
	art->private = NULL;
	return art;
    }

    if ((fd = open(path, O_RDONLY)) < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
	return NULL;
    }

    art = xmalloc(sizeof(ARTHANDLE));
    art->type = TOKEN_TRADSPOOL;

    if (fstat(fd, &sb) < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
        syswarn("tradspool: could not fstat article %s", path);
	free(art);
	close(fd);
	return NULL;
    }

    art->arrived = sb.st_mtime;

    private = xmalloc(sizeof(PRIV_TRADSPOOL));
    art->private = (void *)private;
    private->artlen = sb.st_size;
    if (innconf->articlemmap) {
	if ((private->artbase = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("tradspool: could not mmap article %s", path);
	    free(art->private);
	    free(art);
	    close(fd);
	    return NULL;
	}
        if (amount == RETR_ALL)
            madvise(private->artbase, sb.st_size, MADV_WILLNEED);
        else
            madvise(private->artbase, sb.st_size, MADV_SEQUENTIAL);

	/* consider coexisting both wireformatted and nonwireformatted */
	p = memchr(private->artbase, '\n', private->artlen);
	if (p == NULL || p == private->artbase) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("tradspool: apparently corrupt article %s", path);
	    munmap(private->artbase, private->artlen);
	    free(art->private);
	    free(art);
	    close(fd);
	    return NULL;
	}
	if (p[-1] == '\r') {
	    private->mmapped = true;
	} else {
	    wfarticle = wire_from_native(private->artbase, private->artlen,
                                         &wflen);
	    munmap(private->artbase, private->artlen);
	    private->artbase = wfarticle;
	    private->artlen = wflen;
	    private->mmapped = false;
	}
    } else {
	private->mmapped = false;
	private->artbase = xmalloc(private->artlen);
	if (read(fd, private->artbase, private->artlen) < 0) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("tradspool: could not read article %s", path);
	    free(private->artbase);
	    free(art->private);
	    free(art);
	    close(fd);
	    return NULL;
	}
	p = memchr(private->artbase, '\n', private->artlen);
	if (p == NULL || p == private->artbase) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("tradspool: apparently corrupt article %s", path);
	    free(art->private);
	    free(art);
	    close(fd);
	    return NULL;
	}
	if (p[-1] != '\r') {
	    /* need to make a wireformat copy of the article */
	    wfarticle = wire_from_native(private->artbase, private->artlen,
                                         &wflen);
	    free(private->artbase);
	    private->artbase = wfarticle;
	    private->artlen = wflen;
	}
    }
    close(fd);
    
    private->ngtp = NULL;
    private->curdir = NULL;
    private->curdirname = NULL;
    private->nextindex = -1;

    if (amount == RETR_ALL) {
	art->data = private->artbase;
	art->len = private->artlen;
	return art;
    }
    
    if (((p = wire_findbody(private->artbase, private->artlen)) == NULL)) {
	if (private->mmapped)
	    munmap(private->artbase, private->artlen);
	else
	    free(private->artbase);
	SMseterror(SMERR_NOBODY, NULL);
	free(art->private);
	free(art);
	return NULL;
    }

    if (amount == RETR_HEAD) {
	art->data = private->artbase;
	art->len = p - private->artbase;
	return art;
    }

    if (amount == RETR_BODY) {
	art->data = p;
	art->len = private->artlen - (p - private->artbase);
	return art;
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    if (private->mmapped)
	munmap(private->artbase, private->artlen);
    else
	free(private->artbase);
    free(art->private);
    free(art);
    return NULL;
}

    
ARTHANDLE *
tradspool_retrieve(const TOKEN token, const RETRTYPE amount) {
    char *path;
    ARTHANDLE *art;
    static TOKEN ret_token;

    if (token.type != TOKEN_TRADSPOOL) {
	SMseterror(SMERR_INTERNAL, NULL);
	return NULL;
    }

    if ((path = TokenToPath(token)) == NULL) {
	SMseterror(SMERR_NOENT, NULL);
	return NULL;
    }
    if ((art = OpenArticle(path, amount)) != (ARTHANDLE *)NULL) {
        ret_token = token;
        art->token = &ret_token;
    }
    free(path);
    return art;
}

void
tradspool_freearticle(ARTHANDLE *article)
{
    PRIV_TRADSPOOL *private;

    if (article == NULL)
        return;

    if (article->private) {
	private = (PRIV_TRADSPOOL *) article->private;
	if (private->mmapped)
	    munmap(private->artbase, private->artlen);
	else
	    free(private->artbase);
	if (private->curdir)
	    closedir(private->curdir);
	free(private->curdirname);
        free(private);
    }
    free(article);
}

bool 
tradspool_cancel(TOKEN token) {
    char **xrefs;
    char *xrefhdr;
    ARTHANDLE *article;
    unsigned int numxrefs;
    char *ng, *p;
    char *path, *linkpath;
    unsigned int i;
    bool result = true;
    unsigned long artnum;
    size_t length;

    if ((path = TokenToPath(token)) == NULL) {
	SMseterror(SMERR_UNDEFINED, NULL);
	free(path);
	return false;
    }
    /*
    **  Ooooh, this is gross.  To find the symlinks pointing to this article,
    ** we open the article and grab its Xref line (since the token isn't long
    ** enough to store this info on its own).   This is *not* going to do 
    ** good things for performance of fastrm...  -- rmtodd
    */
    if ((article = OpenArticle(path, RETR_HEAD)) == NULL) {
	free(path);
	SMseterror(SMERR_UNDEFINED, NULL);
        return false;
    }

    xrefhdr = wire_findheader(article->data, article->len, "Xref");
    if (xrefhdr == NULL) {
	/* for backwards compatibility; there is no Xref unless crossposted
	   for 1.4 and 1.5 */
	if (unlink(path) < 0) result = false;
	free(path);
	tradspool_freearticle(article);
        return result;
    }

    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
        if (xrefs != NULL)
            free(xrefs);
	free(path);
	tradspool_freearticle(article);
        SMseterror(SMERR_UNDEFINED, NULL);
        return false;
    }

    tradspool_freearticle(article);
    for (i = 1 ; i < numxrefs ; ++i) {
	if ((p = strchr(xrefs[i], ':')) == NULL) continue;
	*p++ = '\0';
	ng = xrefs[i];
	DeDotify(ng);
	artnum = atol(p);

        length = strlen(innconf->patharticles) + strlen(ng) + 32;
	linkpath = xmalloc(length);
	snprintf(linkpath, length, "%s/%s/%lu", innconf->patharticles, ng,
                 artnum);
	/* repeated unlinkings of a crossposted article may fail on account
	   of the file no longer existing without it truly being an error */
	if (unlink(linkpath) < 0)
	    if (errno != ENOENT || i == 1)
		result = false;
	free(linkpath);
    }
    if (unlink(path) < 0)
    	if (errno != ENOENT || numxrefs == 1)
	    result = false;
    free(path);
    for (i = 0 ; i < numxrefs ; ++i) free(xrefs[i]);
    free(xrefs);
    return result;
}

   
/*
** Find entries for possible articles in dir. "dir" (directory name "dirname").
** The dirname is needed so we can do stats in the directory to disambiguate
** files from symlinks and directories.
*/

static struct dirent *
FindDir(DIR *dir, char *dirname) {
    struct dirent *de;
    int i;
    bool flag;
    char *path;
    struct stat sb;
    size_t length;
    unsigned char namelen;

    while ((de = readdir(dir)) != NULL) {
	namelen = strlen(de->d_name);
	for (i = 0, flag = true ; i < namelen ; ++i) {
	    if (!CTYPE(isdigit, de->d_name[i])) {
		flag = false;
		break;
	    }
	}
	if (!flag) continue; /* if not all digits, skip this entry. */

        length = strlen(dirname) + namelen + 2;
	path = xmalloc(length);
        strlcpy(path, dirname, length);
        strlcat(path, "/", length);
        strlcat(path, de->d_name, length);

	if (lstat(path, &sb) < 0) {
	    free(path);
	    continue;
	}
	free(path);
	if (!S_ISREG(sb.st_mode)) continue;
	return de;
    }
    return NULL;
}

ARTHANDLE *
tradspool_next(ARTHANDLE *article, const RETRTYPE amount)
{
    PRIV_TRADSPOOL priv;
    PRIV_TRADSPOOL *newpriv;
    char *path, *linkpath;
    struct dirent *de;
    ARTHANDLE *art;
    unsigned long artnum;
    unsigned int i;
    static TOKEN token;
    char **xrefs;
    char *xrefhdr, *ng, *p, *expires, *x;
    unsigned int numxrefs;
    STORAGE_SUB	*sub;
    size_t length;

    if (article == NULL) {
	priv.ngtp = NULL;
	priv.curdir = NULL;
	priv.curdirname = NULL;
	priv.nextindex = -1;
    } else {
	priv = *(PRIV_TRADSPOOL *) article->private;
	free(article->private);
	free(article);
	if (priv.artbase != NULL) {
	    if (priv.mmapped)
		munmap(priv.artbase, priv.artlen);
	    else
		free(priv.artbase);
	}
    }

    while (!priv.curdir || ((de = FindDir(priv.curdir, priv.curdirname)) == NULL)) {
	if (priv.curdir) {
	    closedir(priv.curdir);
	    priv.curdir = NULL;
	    free(priv.curdirname);
	    priv.curdirname = NULL;
	}

	/*
	** advance ngtp to the next entry, if it exists, otherwise start 
	** searching down another ngtable hashchain. 
	*/
	while (priv.ngtp == NULL || (priv.ngtp = priv.ngtp->next) == NULL) {
	    /*
	    ** note that at the start of a search nextindex is -1, so the inc.
	    ** makes nextindex 0, as it should be.
	    */
	    priv.nextindex++;
	    if (priv.nextindex >= NGT_SIZE) {
		/* ran off the end of the table, so return. */
		return NULL;
	    }
	    priv.ngtp = NGTable[priv.nextindex];
	    if (priv.ngtp != NULL)
		break;
	}

        priv.curdirname = concatpath(innconf->patharticles, priv.ngtp->ngname);
	priv.curdir = opendir(priv.curdirname);
    }

    path = concatpath(priv.curdirname, de->d_name);
    i = strlen(priv.curdirname);
    /* get the article number while we're here, we'll need it later. */
    artnum = atol(&path[i+1]);

    art = OpenArticle(path, amount);
    if (art == (ARTHANDLE *)NULL) {
	art = xmalloc(sizeof(ARTHANDLE));
	art->type = TOKEN_TRADSPOOL;
	art->data = NULL;
	art->len = 0;
	art->private = xmalloc(sizeof(PRIV_TRADSPOOL));
	art->expires = 0;
	art->groups = NULL;
	art->groupslen = 0;
	newpriv = (PRIV_TRADSPOOL *) art->private;
	newpriv->artbase = NULL;
    } else {
	/* Skip linked (not symlinked) crossposted articles.

           This algorithm is rather questionable; it only works if the first
           group/number combination listed in the Xref header is the
           canonical path.  This will always be true for spools created by
           this implementation, but for traditional INN 1.x servers,
           articles are expired indepedently from each group and may expire
           out of the first listed newsgroup before other groups.  This
           algorithm will orphan such articles, not adding them to history.

           The bit of skipping articles by setting the length of the article
           to zero is also rather suspect, and I'm not sure what
           implications that might have for the callers of SMnext.

           Basically, this whole area really needs to be rethought. */
	xrefhdr = wire_findheader(art->data, art->len, "Xref");
	if (xrefhdr != NULL) {
	    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
		art->len = 0;
	    } else {
		/* assumes first one is the original */
		if ((p = strchr(xrefs[1], ':')) != NULL) {
		    *p++ = '\0';
		    ng = xrefs[1];
		    DeDotify(ng);
		    artnum = atol(p);

                    length = strlen(innconf->patharticles) + strlen(ng) + 32;
		    linkpath = xmalloc(length);
		    snprintf(linkpath, length, "%s/%s/%lu",
                             innconf->patharticles, ng, artnum);
		    if (strcmp(path, linkpath) != 0) {
			/* this is linked article, skip it */
			art->len = 0;
		    }
		    free(linkpath);
		}
	    }
	    for (i = 0 ; i < numxrefs ; ++i) free(xrefs[i]);
	    free(xrefs);
	    if (innconf->storeonxref) {
		/* skip path element */
		if ((xrefhdr = strchr(xrefhdr, ' ')) == NULL) {
		    art->groups = NULL;
		    art->groupslen = 0;
		} else {
		    for (xrefhdr++; *xrefhdr == ' '; xrefhdr++);
		    art->groups = xrefhdr;
		    for (p = xrefhdr ; (*p != '\n') && (*p != '\r') ; p++);
		    art->groupslen = p - xrefhdr;
		}
	    }
	}
	if (xrefhdr == NULL || !innconf->storeonxref) {
            ng = wire_findheader(art->data, art->len, "Newsgroups");
	    if (ng == NULL) {
		art->groups = NULL;
		art->groupslen = 0;
	    } else {
		art->groups = ng;
		for (p = ng ; (*p != '\n') && (*p != '\r') ; p++);
		art->groupslen = p - ng;
	    }
	}
        expires = wire_findheader(art->data, art->len, "Expires");
        if (expires == NULL) {
	    art->expires = 0;
	} else {
            /* optionally parse expire header */
            for (p = expires + 1; (*p != '\n') && (*(p - 1) != '\r'); p++)
                ;
            x = xmalloc(p - expires);
            memcpy(x, expires, p - expires - 1);
            x[p - expires - 1] = '\0';

            art->expires = parsedate_rfc2822_lax(x);
            if (art->expires == (time_t) -1)
                art->expires = 0;
            else
                art->expires -= time(NULL);
            free(x);
        }
	/* for backwards compatibility; assumes no Xref unless crossposted
	   for 1.4 and 1.5: just fall through */
    }
    newpriv = (PRIV_TRADSPOOL *) art->private;
    newpriv->nextindex = priv.nextindex;
    newpriv->curdir = priv.curdir;
    newpriv->curdirname = priv.curdirname;
    newpriv->ngtp = priv.ngtp;
    
    if ((sub = SMgetsub(*art)) == NULL || sub->type != TOKEN_TRADSPOOL) {
	/* maybe storage.conf is modified, after receiving article */
	token = MakeToken(priv.ngtp->ngname, artnum, 0);

        /* Only log an error if art->len is non-zero, since otherwise we get
           all the ones skipped via the hard-link skipping algorithm
           commented above. */
        if (art->len > 0)
            warn("tradspool: can't determine class of %s: %s",
                 TokenToText(token), SMerrorstr);
    } else {
	token = MakeToken(priv.ngtp->ngname, artnum, sub->class);
    }
    art->token = &token;
    free(path);
    return art;
}

static void
FreeNGTree(void)
{
    unsigned int i;
    NGTENT *ngtp, *nextngtp;

    for (i = 0 ; i < NGT_SIZE ; i++) {
        ngtp = NGTable[i];
        for ( ; ngtp != NULL ; ngtp = nextngtp) {
	    nextngtp = ngtp->next;
	    free(ngtp->ngname);
	    free(ngtp->node);
	    free(ngtp);
	}
	NGTable[i] = NULL;
    }
    MaxNgNumber = 0;
    NGTree = NULL;
}

bool tradspool_ctl(PROBETYPE type, TOKEN *token, void *value) {
    struct artngnum *ann;
    unsigned long ngnum;
    unsigned long artnum;
    char *ng, *p;

    switch (type) { 
    case SMARTNGNUM:
	if ((ann = (struct artngnum *)value) == NULL)
	    return false;
	CheckNeedReloadDB(false);
	memcpy(&ngnum, &token->token[0], sizeof(ngnum));
	memcpy(&artnum, &token->token[sizeof(ngnum)], sizeof(artnum));
	artnum = ntohl(artnum);
	ngnum = ntohl(ngnum);
	ng = FindNGByNum(ngnum);
	if (ng == NULL) {
	    CheckNeedReloadDB(true);
	    ng = FindNGByNum(ngnum);
	    if (ng == NULL)
		return false;
	}
	ann->groupname = xstrdup(ng);
        for (p = ann->groupname; *p != 0; p++)
            if (*p == '/')
                *p = '.';
	ann->artnum = (ARTNUM)artnum;
	return true;
    default:
	return false;
    }       
}

bool
tradspool_flushcacheddata(FLUSHTYPE type UNUSED)
{
    return true;
}

void
tradspool_printfiles(FILE *file, TOKEN token UNUSED, char **xref, int ngroups)
{
    int i;
    char *path, *p;

    for (i = 0; i < ngroups; i++) {
        path = xstrdup(xref[i]);
        for (p = path; *p != '\0'; p++)
            if (*p == '.' || *p == ':')
                *p = '/';
        fprintf(file, "%s\n", path);
        free(path);
    }
}

void
tradspool_shutdown(void) {
    DumpDB();
    FreeNGTree();
}
