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
#include <syslog.h>
#include <sys/stat.h>
#include <sys/uio.h>

/* Needed for htonl() and friends on AIX 4.1. */
#include <netinet/in.h>
    
#include "inn/qio.h"
#include "libinn.h"
#include "macros.h"
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

bool NGTableUpdated; /* set to TRUE if we've added any entries since reading 
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

    p = COPY(ng);
    DeDotify(p);
    hash = Hash(p, strlen(p));
    DISPOSE(p);

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

    p = COPY(ng);
    DeDotify(p); /* canonicalize p to standard (/) form. */
    hash = HashNGName(p);

    h = (unsigned char)hash.hash[0];
    h = h + (((unsigned char)hash.hash[1])<<8);

    h = h % NGT_SIZE;

    ngtp = NGTable[h];
    ngtpp = &NGTable[h];
    while (TRUE) {
	if (ngtp == NULL) {
	    /* ng wasn't in table, add new entry. */
	    NGTableUpdated = TRUE;

	    ngtp = NEW(NGTENT, 1);
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
	    newnode = NEW(NGTREENODE, 1);
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
			/* Error, same number is already in NGtree (shouldn't happen!) */
			syslog(L_ERROR, "tradspool: AddNG: duplicate newsgroup number in NGtree: %d(%s)", number, p);
			return;
		    }
		}
		*nextnode = newnode;
		return;
	    }
	} else if (strcmp(ngtp->ngname, p) == 0) {
	    /* entry in table already, so return */
	    DISPOSE(p);
	    return;
#if 0 /* XXX */
	} else if (CompareHash(&ngtp->hash, &hash) == 0) {
	    /* eep! we hit a hash collision. */
	    syslog(L_ERROR, "tradspool: AddNG: Hash collison %s/%s", ngtp->ngname, p);
	    DISPOSE(p);
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
NGTENT *
FindNGByName(char *ngname) {
    NGTENT *ngtp;
    unsigned int h;
    HASHEDNG hash;
    char *p;

    p = COPY(ngname);
    DeDotify(p); /* canonicalize p to standard (/) form. */
    hash = HashNGName(p);

    h = (unsigned char)hash.hash[0];
    h = h + (((unsigned char)hash.hash[1])<<8);

    h = h % NGT_SIZE;

    ngtp = NGTable[h];

    while (ngtp) {
	if (strcmp(p, ngtp->ngname) == 0) {
	    DISPOSE(p);
	    return ngtp;
	}
	ngtp = ngtp->next;
    }
    DISPOSE(p);
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
void
DumpDB(void) {
    char *fname, *fnamenew;
    NGTENT *ngtp;
    unsigned int i;
    FILE *out;

    if (!SMopenmode) return; /* don't write if we're not in read/write mode. */
    if (!NGTableUpdated) return; /* no need to dump new DB */

    fname = concatpath(innconf->pathspool, _PATH_TRADSPOOLNGDB);
    fnamenew = concatpath(innconf->pathspool, _PATH_NEWTSNGDB);

    if ((out = fopen(fnamenew, "w")) == NULL) {
	syslog(L_ERROR, "tradspool: DumpDB: can't write %s: %m", fnamenew);
	DISPOSE(fname);
	DISPOSE(fnamenew);
	return;
    }
    for (i = 0 ; i < NGT_SIZE ; ++i) {
	ngtp = NGTable[i];
	for ( ; ngtp ; ngtp = ngtp->next) {
	    fprintf(out, "%s %lu\n", ngtp->ngname, ngtp->ngnumber);
	}
    }
    if (fclose(out) < 0) {
	syslog(L_ERROR, "tradspool: DumpDB: can't close %s: %m", fnamenew);
	DISPOSE(fname);
	DISPOSE(fnamenew);
	return;
    }
    if (rename(fnamenew, fname) < 0) {
	syslog(L_ERROR, "tradspool: can't rename %s", fnamenew);
	DISPOSE(fname);
	DISPOSE(fnamenew);
	return;
    }
    DISPOSE(fname);
    DISPOSE(fnamenew);
    NGTableUpdated = FALSE; /* reset modification flag. */
    return;
}

/* 
** init NGTable from saved database file and from active.  Note that
** entries in the database file get added first,  and get their specifications
** of newsgroup number from there. 
*/

bool
ReadDBFile(void) {
    char *fname;
    QIOSTATE *qp;
    char *line;
    char *p;
    unsigned long number;

    fname = concatpath(innconf->pathspool, _PATH_TRADSPOOLNGDB);
    if ((qp = QIOopen(fname)) == NULL) {
	/* only warn if db not found. */
	syslog(L_NOTICE, "tradspool: %s not found", fname);
    } else {
	while ((line = QIOread(qp)) != NULL) {
	    p = strchr(line, ' ');
	    if (p == NULL) {
		syslog(L_FATAL, "tradspool: corrupt line in active %s", line);
		QIOclose(qp);
		DISPOSE(fname);
		return FALSE;
	    }
	    *p++ = 0;
	    number = atol(p);
	    AddNG(line, number);
	    if (MaxNgNumber < number) MaxNgNumber = number;
	}
	QIOclose(qp);
    }
    DISPOSE(fname);
    return TRUE;
}

bool
ReadActiveFile(void) {
    char *fname;
    QIOSTATE *qp;
    char *line;
    char *p;

    fname = concatpath(innconf->pathdb, _PATH_ACTIVE);
    if ((qp = QIOopen(fname)) == NULL) {
	syslog(L_FATAL, "tradspool: can't open %s", fname);
	DISPOSE(fname);
	return FALSE;
    }

    while ((line = QIOread(qp)) != NULL) {
	p = strchr(line, ' ');
	if (p == NULL) {
	    syslog(L_FATAL, "tradspool: corrupt line in active %s", line);
	    QIOclose(qp);
	    DISPOSE(fname);
	    return FALSE;
	}
	*p = 0;
	AddNG(line, 0);
    }
    QIOclose(qp);
    DISPOSE(fname);
    /* dump any newly added changes to database */
    DumpDB();
    return TRUE;
}

bool
InitNGTable(void) {
    if (!ReadDBFile()) return FALSE;

    /*
    ** set NGTableUpdated to false; that way we know if the load of active or
    ** any AddNGs later on did in fact add new entries to the db.
    */
    NGTableUpdated = FALSE; 
    if (!SMopenmode)
	/* don't read active unless write mode. */
	return TRUE;
    return ReadActiveFile(); 
}

/* 
** Routine called to check every so often to see if we need to reload the
** database and add in any new groups that have been added.   This is primarily
** for the benefit of innfeed in funnel mode, which otherwise would never
** get word that any new newsgroups had been added. 
*/

#define RELOAD_TIME_CHECK 600

void
CheckNeedReloadDB(bool force) {
    static TIMEINFO lastcheck, oldlastcheck, now;
    struct stat sb;
    char *fname;

    if (GetTimeInfo(&now) < 0) return; /* anyone ever seen gettimeofday fail? :-) */
    if (!force && lastcheck.time + RELOAD_TIME_CHECK > now.time) return;

    oldlastcheck = lastcheck;
    lastcheck = now;

    fname = concatpath(innconf->pathspool, _PATH_TRADSPOOLNGDB);
    if (stat(fname, &sb) < 0) {
	DISPOSE(fname);
	return;
    }
    DISPOSE(fname);
    if (sb.st_mtime > oldlastcheck.time) {
	/* add any newly added ngs to our in-memory copy of the db. */
	ReadDBFile();
    }
}

/* Init routine, called by SMinit */

bool
tradspool_init(SMATTRIBUTE *attr) {
    if (attr == NULL) {
	syslog(L_ERROR, "tradspool: attr is NULL");
	SMseterror(SMERR_INTERNAL, "attr is NULL");
	return FALSE;
    }
    if (!innconf->storeonxref) {
	syslog(L_ERROR, "tradspool: storeonxref needs to be true");
	SMseterror(SMERR_INTERNAL, "storeonxref needs to be true");
	return FALSE;
    }
    attr->selfexpire = FALSE;
    attr->expensivestat = TRUE;
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

    CheckNeedReloadDB(FALSE);

    memcpy(&ngnum, &token.token[0], sizeof(ngnum));
    memcpy(&artnum, &token.token[sizeof(ngnum)], sizeof(artnum));
    artnum = ntohl(artnum);
    ngnum = ntohl(ngnum);

    ng = FindNGByNum(ngnum);
    if (ng == NULL) {
	CheckNeedReloadDB(TRUE);
	ng = FindNGByNum(ngnum);
	if (ng == NULL)
	    return NULL;
    }

    path = NEW(char, strlen(ng)+20+strlen(innconf->patharticles));
    sprintf(path, "%s/%s/%lu", innconf->patharticles, ng, artnum);
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
    unsigned int slen;

    len = 0;
    xrefsize = 5;
    xrefs = NEW(char *, xrefsize);

    /* no path element should exist, nor heading white spaces exist */
    p = xref;
    while (TRUE) {
	/* check for EOL */
	/* shouldn't ever hit null w/o hitting a \r\n first, but best to be paranoid */
	if (*p == '\n' || *p == '\r' || *p == 0) {
	    /* hit EOL, return. */
	    *lenp = len;
	    return xrefs;
	}
	/* skip to next space or EOL */
	for (q=p; *q && *q != ' ' && *q != '\n' && *q != '\r' ; ++q) ;

	slen = q-p;
	xrefs[len] = NEW(char, slen+1);
	strncpy(xrefs[len], p, slen);
	xrefs[len][slen] = '\0';

	if (++len == xrefsize) {
	    /* grow xrefs if needed. */
	    xrefsize *= 2;
	    RENEW(xrefs, char *, xrefsize);
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
    int i;
    char *nonwfarticle; /* copy of article converted to non-wire format */
    int nonwflen, used;
    
    xrefhdr = article.groups;
    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
	token.type = TOKEN_EMPTY;
	SMseterror(SMERR_UNDEFINED, "bogus Xref: header");
	if (xrefs != NULL)
	    DISPOSE(xrefs);
	return token;
    }

    if ((p = strchr(xrefs[0], ':')) == NULL) {
	token.type = TOKEN_EMPTY;
	SMseterror(SMERR_UNDEFINED, "bogus Xref: header");
	for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
	DISPOSE(xrefs);
	return token;
    }
    *p++ = '\0';
    ng = xrefs[0];
    DeDotify(ng);
    artnum = atol(p);
    
    token = MakeToken(ng, artnum, class);

    path = NEW(char, strlen(innconf->patharticles) + strlen(ng) + 32);
    sprintf(path, "%s/%s/%lu", innconf->patharticles, ng, artnum);

    /* following chunk of code boldly stolen from timehash.c  :-) */
    if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
	p = strrchr(path, '/');
	*p = '\0';
	if (!MakeDirectory(path, TRUE)) {
	    syslog(L_ERROR, "tradspool: could not make directory %s %m", path);
	    token.type = TOKEN_EMPTY;
	    DISPOSE(path);
	    SMseterror(SMERR_UNDEFINED, NULL);
	    for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
	    DISPOSE(xrefs);
	    return token;
	} else {
	    *p = '/';
	    if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
		SMseterror(SMERR_UNDEFINED, NULL);
		syslog(L_ERROR, "tradspool: could not open %s %m", path);
		token.type = TOKEN_EMPTY;
		DISPOSE(path);
		for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
		DISPOSE(xrefs);
		return token;
	    }
	}
    }
    if (innconf->wireformat) {
	if ((xwritev(fd, article.iov, article.iovcnt)) != article.len) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    syslog(L_ERROR, "tradspool error writing %s %m", path);
	    close(fd);
	    token.type = TOKEN_EMPTY;
	    unlink(path);
	    DISPOSE(path);
	    for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
	    DISPOSE(xrefs);
	    return token;
	}
    } else {
	onebuffer = (char *)xmalloc(article.len);
	for (used = i = 0 ; i < article.iovcnt ; i++) {
	    memcpy(&onebuffer[used], article.iov[i].iov_base, article.iov[i].iov_len);
	    used += article.iov[i].iov_len;
	}
	nonwfarticle = FromWireFmt(onebuffer, used, &nonwflen);
	free(onebuffer);
	if (write(fd, nonwfarticle, nonwflen) != nonwflen) {
	    DISPOSE(nonwfarticle);
	    SMseterror(SMERR_UNDEFINED, NULL);
	    syslog(L_ERROR, "tradspool error writing %s %m", path);
	    close(fd);
	    token.type = TOKEN_EMPTY;
	    unlink(path);
	    DISPOSE(path);
	    for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
	    DISPOSE(xrefs);
	    return token;
	}
	DISPOSE(nonwfarticle);
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

	    linkpath = NEW(char, strlen(innconf->patharticles) + strlen(ng) + 32);
	    sprintf(linkpath, "%s/%s/%lu", innconf->patharticles, ng, artnum);
	    if (link(path, linkpath) < 0) {
		p = strrchr(linkpath, '/');
		*p = '\0';
		dirname = COPY(linkpath);
		*p = '/';
		if (!MakeDirectory(dirname, TRUE) || link(path, linkpath) < 0) {
#if !defined(HAVE_SYMLINK)
		    SMseterror(SMERR_UNDEFINED, NULL);
		    syslog(L_ERROR, "tradspool: could not link %s to %s %m", path, linkpath);
		    token.type = TOKEN_EMPTY;
		    DISPOSE(dirname);
		    DISPOSE(linkpath);
		    DISPOSE(path);
		    for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
		    DISPOSE(xrefs);
		    return token;
#else
		    if (symlink(path, linkpath) < 0) {
			SMseterror(SMERR_UNDEFINED, NULL);
			syslog(L_ERROR, "tradspool: could not symlink %s to %s %m", path, linkpath);
			token.type = TOKEN_EMPTY;
			DISPOSE(dirname);
			DISPOSE(linkpath);
			DISPOSE(path);
			for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
			DISPOSE(xrefs);
			return token;
		    }
#endif  /* !defined(HAVE_SYMLINK) */
		}
		DISPOSE(dirname);
	    }
	    DISPOSE(linkpath);
	}
    }
    DISPOSE(path);
    for (i = 0 ; i < numxrefs; ++i) DISPOSE(xrefs[i]);
    DISPOSE(xrefs);
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
    int wflen;

    if (amount == RETR_STAT) {
	if (access(path, R_OK) < 0) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    return NULL;
	}
	art = NEW(ARTHANDLE, 1);
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

    art = NEW(ARTHANDLE, 1);
    art->type = TOKEN_TRADSPOOL;

    if (fstat(fd, &sb) < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
	syslog(L_ERROR, "tradspool: could not fstat article: %m");
	DISPOSE(art);
	close(fd);
	return NULL;
    }

    art->arrived = sb.st_mtime;

    private = NEW(PRIV_TRADSPOOL, 1);
    art->private = (void *)private;
    private->artlen = sb.st_size;
    if (innconf->articlemmap) {
	if ((private->artbase = mmap(NULL, sb.st_size, PROT_READ, MAP__ARG, fd, 0)) == MAP_FAILED) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    syslog(L_ERROR, "tradspool: could not mmap article: %m");
	    DISPOSE(art->private);
	    DISPOSE(art);
	    close(fd);
	    return NULL;
	}
	/* consider coexisting both wireformatted and nonwireformatted */
	p = memchr(private->artbase, '\n', private->artlen);
	if (p == NULL || p == private->artbase) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    syslog(L_ERROR, "tradspool: could not mmap article: %m");
	    munmap(private->artbase, private->artlen);
	    DISPOSE(art->private);
	    DISPOSE(art);
	    close(fd);
	    return NULL;
	}
	if (p[-1] == '\r') {
	    private->mmapped = TRUE;
	} else {
	    wfarticle = ToWireFmt(private->artbase, private->artlen, &wflen);
	    munmap(private->artbase, private->artlen);
	    private->artbase = wfarticle;
	    private->artlen = wflen;
	    private->mmapped = FALSE;
	}
    } else {
	private->mmapped = FALSE;
	private->artbase = NEW(char, private->artlen);
	if (read(fd, private->artbase, private->artlen) < 0) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    syslog(L_ERROR, "tradspool: could not read article: %m");
	    DISPOSE(private->artbase);
	    DISPOSE(art->private);
	    DISPOSE(art);
	    close(fd);
	    return NULL;
	}
	p = memchr(private->artbase, '\n', private->artlen);
	if (p == NULL || p == private->artbase) {
	    SMseterror(SMERR_UNDEFINED, NULL);
	    syslog(L_ERROR, "tradspool: could not mmap article: %m");
	    DISPOSE(art->private);
	    DISPOSE(art);
	    close(fd);
	    return NULL;
	}
	if (p[-1] != '\r') {
	    /* need to make a wireformat copy of the article */
	    wfarticle = ToWireFmt(private->artbase, private->artlen, &wflen);
	    DISPOSE(private->artbase);
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
    
    if (((p = SMFindBody(private->artbase, private->artlen)) == NULL)) {
	if (private->mmapped)
	    munmap(private->artbase, private->artlen);
	else
	    DISPOSE(private->artbase);
	SMseterror(SMERR_NOBODY, NULL);
	DISPOSE(art->private);
	DISPOSE(art);
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
	DISPOSE(private->artbase);
    DISPOSE(art->private);
    DISPOSE(art);
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
    DISPOSE(path);
    return art;
}

void
tradspool_freearticle(ARTHANDLE *article) {
    PRIV_TRADSPOOL *private;

    if (!article) return;

    if (article->private) {
	private = (PRIV_TRADSPOOL *) article->private;
	if (private->mmapped)
	    munmap(private->artbase, private->artlen);
	else
	    DISPOSE(private->artbase);
	if (private->curdir) {
	    closedir(private->curdir);
	}
	DISPOSE(private->curdirname);
    }
}

bool 
tradspool_cancel(TOKEN token) {
    char **xrefs;
    char *xrefhdr;
    ARTHANDLE *article;
    unsigned int numxrefs;
    char *ng, *p;
    char *path, *linkpath;
    int i;
    bool result = TRUE;
    unsigned long artnum;

    if ((path = TokenToPath(token)) == NULL) {
	SMseterror(SMERR_UNDEFINED, NULL);
	DISPOSE(path);
	return FALSE;
    }
    /*
    **  Ooooh, this is gross.  To find the symlinks pointing to this article,
    ** we open the article and grab its Xref line (since the token isn't long
    ** enough to store this info on its own).   This is *not* going to do 
    ** good things for performance of fastrm...  -- rmtodd
    */
    if ((article = OpenArticle(path, RETR_HEAD)) == NULL) {
	DISPOSE(path);
	SMseterror(SMERR_UNDEFINED, NULL);
        return FALSE;
    }

    xrefhdr = (char *)HeaderFindMem(article->data, article->len, "Xref", 4);
    if (xrefhdr == NULL) {
	/* for backwards compatibility; there is no Xref unless crossposted
	   for 1.4 and 1.5 */
	if (unlink(path) < 0) result = FALSE;
	DISPOSE(path);
	tradspool_freearticle(article);
        return result;
    }

    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
	DISPOSE(path);
	tradspool_freearticle(article);
        SMseterror(SMERR_UNDEFINED, NULL);
        return FALSE;
    }

    tradspool_freearticle(article);
    for (i = 1 ; i < numxrefs ; ++i) {
	if ((p = strchr(xrefs[i], ':')) == NULL) continue;
	*p++ = '\0';
	ng = xrefs[i];
	DeDotify(ng);
	artnum = atol(p);

	linkpath = NEW(char, strlen(innconf->patharticles) + strlen(ng) + 32);
	sprintf(linkpath, "%s/%s/%lu", innconf->patharticles, ng, artnum);
	/* repeated unlinkings of a crossposted article may fail on account
	   of the file no longer existing without it truly being an error */
	if (unlink(linkpath) < 0)
	    if (errno != ENOENT || i == 1)
		result = FALSE;
	DISPOSE(linkpath);
    }
    if (unlink(path) < 0)
    	if (errno != ENOENT || numxrefs == 1)
	    result = FALSE;
    DISPOSE(path);
    for (i = 0 ; i < numxrefs ; ++i) DISPOSE(xrefs[i]);
    DISPOSE(xrefs);
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
    unsigned char namelen;

    while ((de = readdir(dir)) != NULL) {
	namelen = strlen(de->d_name);
	for (i = 0, flag = TRUE ; i < namelen ; ++i) {
	    if (!isdigit(de->d_name[i])) {
		flag = FALSE;
		break;
	    }
	}
	if (!flag) continue; /* if not all digits, skip this entry. */

	path = NEW(char, strlen(dirname)+namelen+2);
	strcpy(path, dirname);
	strcat(path, "/");
	strncpy(&path[strlen(dirname)+1], de->d_name, namelen);
	path[strlen(dirname)+namelen+1] = '\0';

	if (lstat(path, &sb) < 0) {
	    DISPOSE(path);
	    continue;
	}
	DISPOSE(path);
	if (!S_ISREG(sb.st_mode)) continue;
	return de;
    }
    return NULL;
}

ARTHANDLE *tradspool_next(const ARTHANDLE *article, const RETRTYPE amount) {
    PRIV_TRADSPOOL priv;
    PRIV_TRADSPOOL *newpriv;
    char *path, *linkpath;
    struct dirent *de;
    ARTHANDLE *art;
    unsigned long artnum;
    int i;
    static TOKEN token;
    unsigned char namelen;
    char **xrefs;
    char *xrefhdr, *ng, *p, *expires, *x;
    unsigned int numxrefs;
    STORAGE_SUB	*sub;

    if (article == NULL) {
	priv.ngtp = NULL;
	priv.curdir = NULL;
	priv.curdirname = NULL;
	priv.nextindex = -1;
    } else {
	priv = *(PRIV_TRADSPOOL *) article->private;
	DISPOSE(article->private);
	DISPOSE((void*)article);
	if (priv.artbase != NULL) {
	    if (priv.mmapped)
		munmap(priv.artbase, priv.artlen);
	    else
		DISPOSE(priv.artbase);
	}
    }

    while (!priv.curdir || ((de = FindDir(priv.curdir, priv.curdirname)) == NULL)) {
	if (priv.curdir) {
	    closedir(priv.curdir);
	    priv.curdir = NULL;
	    DISPOSE(priv.curdirname);
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

	priv.curdirname = NEW(char, strlen(innconf->patharticles)+strlen(priv.ngtp->ngname)+2);
	sprintf(priv.curdirname, "%s/%s",innconf->patharticles,priv.ngtp->ngname);
	priv.curdir = opendir(priv.curdirname);
    }

    namelen = strlen(de->d_name);
    path = NEW(char, strlen(priv.curdirname) + 2 + namelen);
    strcpy(path, priv.curdirname);
    strcat(path, "/");
    i = strlen(priv.curdirname);
    strncpy(&path[i+1], de->d_name, namelen);
    path[i+namelen+1] = '\0';
    /* get the article number while we're here, we'll need it later. */
    artnum = atol(&path[i+1]);

    art = OpenArticle(path, amount);
    if (art == (ARTHANDLE *)NULL) {
	art = NEW(ARTHANDLE, 1);
	art->type = TOKEN_TRADSPOOL;
	art->data = NULL;
	art->len = 0;
	art->private = (void *)NEW(PRIV_TRADSPOOL, 1);
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
	xrefhdr = (char *)HeaderFindMem(art->data, art->len, "Xref", 4);
	if (xrefhdr != NULL) {
	    if ((xrefs = CrackXref(xrefhdr, &numxrefs)) == NULL || numxrefs == 0) {
		art->len = 0;
	    } else {
		/* assumes first one is the original */
		if ((p = strchr(xrefs[0], ':')) != NULL) {
		    *p++ = '\0';
		    ng = xrefs[0];
		    DeDotify(ng);
		    artnum = atol(p);

		    linkpath = NEW(char, strlen(innconf->patharticles) + strlen(ng) + 32);
		    sprintf(linkpath, "%s/%s/%lu", innconf->patharticles, ng, artnum);
		    if (strcmp(path, linkpath) != 0) {
			/* this is linked article, skip it */
			art->len = 0;
		    }
		    DISPOSE(linkpath);
		}
	    }
	    for (i = 0 ; i < numxrefs ; ++i) DISPOSE(xrefs[i]);
	    DISPOSE(xrefs);
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
	} else if (innconf->storeonxref) {
	    art->groups = NULL;
	    art->groupslen = 0;
	}
	if (!innconf->storeonxref) {
	    if ((ng = (char *)HeaderFindMem(art->data, art->len, "Newsgroups", 10)) == NULL) {
		art->groups = NULL;
		art->groupslen = 0;
	    } else {
		art->groups = ng;
		for (p = ng ; (*p != '\n') && (*p != '\r') ; p++);
		art->groupslen = p - ng;
	    }
	}
	if ((expires = (char *)HeaderFindMem(art->data, art->len, "Expires", 7)) == NULL) {
	    art->expires = 0;
	} else {
            /* optionally parse expire header */
            for (p = expires + 1; (*p != '\n') && (*(p - 1) != '\r'); p++);
            x = NEW(char, p - expires);
            memcpy(x, expires, p - expires - 1);
            x[p - expires - 1] = '\0';

            art->expires = parsedate(x, NULL);
            if (art->expires == -1)
                art->expires = 0;
            else
                art->expires -= time(0);
            DISPOSE(x);
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
            syslog(L_ERROR, "tradspool: can't determine class: %s: %s",
                   TokenToText(token), SMerrorstr);
    } else {
	token = MakeToken(priv.ngtp->ngname, artnum, sub->class);
    }
    art->token = &token;
    DISPOSE(path);
    return art;
}

void
FreeNGTree(void) {
    unsigned int i;
    NGTENT *ngtp, *nextngtp;

    for (i = 0 ; i < NGT_SIZE ; i++) {
        ngtp = NGTable[i];
        for ( ; ngtp != NULL ; ngtp = nextngtp) {
	    nextngtp = ngtp->next;
	    DISPOSE(ngtp->ngname);
	    DISPOSE(ngtp->node);
	    DISPOSE(ngtp);
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
    char *ng;

    switch (type) { 
    case SMARTNGNUM:
	if ((ann = (struct artngnum *)value) == NULL)
	    return FALSE;
	CheckNeedReloadDB(FALSE);
	memcpy(&ngnum, &token->token[0], sizeof(ngnum));
	memcpy(&artnum, &token->token[sizeof(ngnum)], sizeof(artnum));
	artnum = ntohl(artnum);
	ngnum = ntohl(ngnum);
	ng = FindNGByNum(ngnum);
	if (ng == NULL) {
	    CheckNeedReloadDB(TRUE);
	    ng = FindNGByNum(ngnum);
	    if (ng == NULL)
		return FALSE;
	}
	ann->groupname = COPY(ng);
	ann->artnum = (ARTNUM)artnum;
	return TRUE;
    default:
	return FALSE;
    }       
}

bool tradspool_flushcacheddata(FLUSHTYPE type) {
    return TRUE;
}

void tradspool_printfiles(FILE *file, TOKEN token, char **xref, int ngroups) {
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
