/*  $Id$
**
**  Like the timehash storage method (and heavily inspired by it), but uses
**  the CAF library to store multiple articles in a single file.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#include "caf.h"
#include "inn/fdflag.h"
#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "inn/libinn.h"
#include "methods.h"
#include "timecaf.h"
#include "inn/paths.h"

/* Needed for htonl() and friends on AIX 4.1. */
#include <netinet/in.h>

typedef struct {
    char		*artdata; /* start of the article data -- may be mmaped */
    char		*mmapbase; /* actual start of mmaped region (on pagesize bndry, not necessarily == artdaya */
    unsigned int	artlen; /* art length. */
    size_t		mmaplen; /* length of mmap region. */
    DIR			*top; /* open handle on top level dir. */
    DIR	       		*sec; /* open handle on the 2nd level directory */
    DIR 		*ter; /* open handle on 3rd level dir. */
    struct dirent	*topde; /* last entry we got from top */
    struct dirent	*secde; /* last entry we got from sec */
    struct dirent	*terde; /* last entry we got from sec */
    CAFTOCENT		*curtoc;
    ARTNUM		curartnum;
    CAFHEADER		curheader;
} PRIV_TIMECAF;

/* current path/fd for an open CAF file */
typedef struct {
    char	*path; /* path to file. */
    int		fd; /* open fd -- -1 if no file currently open. */
} CAFOPENFILE;

static CAFOPENFILE ReadingFile, WritingFile;
static char *DeletePath;
static ARTNUM *DeleteArtnums;
static unsigned int NumDeleteArtnums, MaxDeleteArtnums;

typedef enum {FIND_DIR, FIND_CAF, FIND_TOPDIR} FINDTYPE;

/*
** Structures for the cache for stat information (to make expireover etc.)
** faster.
**
** The first structure contains the TOC info for a single CAF file.  The 2nd
** one has pointers to the info for up to 256 CAF files, indexed
** by the 2nd least significant byte of the arrival time.
*/

struct caftoccacheent {
    CAFTOCENT *toc;
    CAFHEADER header;
};
typedef struct caftoccacheent CAFTOCCACHEENT;

struct caftocl1cache {
    CAFTOCCACHEENT *entries[256];
};
typedef struct caftocl1cache CAFTOCL1CACHE;

/*
** and similar structures indexed by the 3rd and 4th bytes of the arrival time.
** pointing to the lower level structures.  Note that the top level structure
** (the one indexed by the MSByte of the timestamp) is likely to have only
** one active pointer, unless your spool keeps more than 194 days of articles,
** but it doesn't cost much to keep that one structure around and keep the
** code general.
*/

struct caftocl2cache {
    CAFTOCL1CACHE *l1ptr[256];
};
typedef struct caftocl2cache CAFTOCL2CACHE;

struct caftocl3cache {
    CAFTOCL2CACHE *l2ptr[256];
};
typedef struct caftocl3cache CAFTOCL3CACHE;

static CAFTOCL3CACHE *TOCCache[256]; /* indexed by storage class! */
static int TOCCacheHits, TOCCacheMisses;

/*
**  The token is @04nn00aabbccyyyyxxxx0000000000000000@
**  where "04" is the timecaf method number,
**  "nn" the hexadecimal value of the storage class,
**  "aabbccdd" the arrival time in hexadecimal (dd is unused),
**  "xxxxyyyy" the hexadecimal sequence number seqnum.
**
**  innconf->patharticles + '/timecaf-nn/bb/aacc.CF'
**  where "nn" is the hexadecimal value of the storage class,
**  "aabbccdd" the arrival time in hexadecimal (dd is unused).
*/
char *
timecaf_explaintoken(const TOKEN token)
{
    char                *text;
    uint32_t            arrival;
    uint16_t            seqnum1;
    uint16_t            seqnum2;

    memcpy(&arrival, &token.token[0], sizeof(arrival));
    memcpy(&seqnum1, &token.token[4], sizeof(seqnum1));
    memcpy(&seqnum2, &token.token[6], sizeof(seqnum2));
 
    xasprintf(&text, "method=timecaf class=%u time=%lu seqnum=%lu file=%s/timecaf-%02x/%02x/%02x%02x.CF",
              (unsigned int) token.class, ((unsigned long) (ntohl(arrival))) << 8,
              ((unsigned long) ntohs(seqnum1)) + (((unsigned long) ntohs(seqnum2)) << 16),
              innconf->patharticles, token.class,
              (ntohl(arrival) >> 8) & 0xff,
              (ntohl(arrival) >> 16) & 0xff,
              ntohl(arrival) & 0xff);

    return text;
}

static TOKEN MakeToken(time_t now, ARTNUM seqnum, STORAGECLASS class, TOKEN *oldtoken) {
    TOKEN               token;
    uint32_t            i;
    uint16_t            s;

    if (oldtoken == (TOKEN *)NULL)
	memset(&token, '\0', sizeof(token));
    else
	memcpy(&token, oldtoken, sizeof(token));
    token.type = TOKEN_TIMECAF;
    token.class = class;
    i = htonl(now);
    memcpy(token.token, &i, sizeof(i));
    s = htons(seqnum & 0xffff);
    memcpy(&token.token[sizeof(i)], &s, sizeof(s));
    s = htons((seqnum >> 16) & 0xffff);
    memcpy(&token.token[sizeof(i)+sizeof(s)], &s, sizeof(s));
    return token;
}


static void BreakToken(TOKEN token, time_t *now, ARTNUM *seqnum) {
    uint32_t            i;
    uint16_t            s1 = 0;
    uint16_t            s2 = 0;

    memcpy(&i, token.token, sizeof(i));
    memcpy(&s1, &token.token[sizeof(i)], sizeof(s1));
    memcpy(&s2, &token.token[sizeof(i)+sizeof(s1)], sizeof(s2));
    *now = ntohl(i);
    *seqnum = (ARTNUM)((ntohs(s2) << 16) + ntohs(s1));
}

/*
** Note: the time here is really "time>>8", i.e. a timestamp that's been
** shifted right by 8 bits.
*/
static char *MakePath(time_t now, const STORAGECLASS class) {
    char *path;
    size_t length;

    length = strlen(innconf->patharticles) + 32;
    path = xmalloc(length);
    snprintf(path, length, "%s/timecaf-%02x/%02x/%02x%02x.CF",
             innconf->patharticles, class,
             (unsigned int)((now >> 8) & 0xff),
             (unsigned int)((now >> 16) & 0xff),
             (unsigned int)(now & 0xff));

    return path;
}

static TOKEN *PathNumToToken(char *path, ARTNUM artnum) {
    int			n;
    unsigned int        t1, t2;
    STORAGECLASS        class;
    time_t              timestamp;
    static TOKEN	token;

    n = sscanf(path, "timecaf-%02x/%02x/%04x.CF", (unsigned int *)&class, &t1, &t2);
    if (n != 3)
	return (TOKEN *)NULL;
    timestamp = ((t1 << 8) & 0xff00) | ((t2 << 8) & 0xff0000) | ((t2 << 0) & 0xff);
    token = MakeToken(timestamp, artnum, class, (TOKEN *)NULL);
    return &token;
}


bool timecaf_init(SMATTRIBUTE *attr) {
    if (attr == NULL) {
        warn("timecaf: attr is NULL");
	SMseterror(SMERR_INTERNAL, "attr is NULL");
	return false;
    }
    attr->selfexpire = false;
    attr->expensivestat = false;
    if (STORAGE_TOKEN_LENGTH < 6) {
        warn("timecaf: token length is less than six bytes");
	SMseterror(SMERR_TOKENSHORT, NULL);
	return false;
    }
    ReadingFile.fd = WritingFile.fd = -1;
    ReadingFile.path = WritingFile.path = (char *)NULL;
    return true;
}

/*
** Routines for managing the 'TOC cache' (cache of TOCs of various CAF files)
**
** Attempt to look up a given TOC entry in the cache.  Takes the timestamp
** as arguments.
*/

static CAFTOCCACHEENT *
CheckTOCCache(time_t timestamp, STORAGECLASS tokenclass)
{
    CAFTOCL2CACHE *l2;
    CAFTOCL1CACHE *l1;
    CAFTOCCACHEENT *cent;
    unsigned char tmp;

    if (TOCCache[tokenclass] == NULL) return NULL; /* cache is empty */

    tmp = (timestamp>>16) & 0xff;
    l2 = TOCCache[tokenclass]->l2ptr[tmp];
    if (l2 == NULL) return NULL;

    tmp = (timestamp>>8) & 0xff;
    l1 = l2->l1ptr[tmp];
    if (l1 == NULL) return NULL;

    tmp = (timestamp) & 0xff;
    cent = l1->entries[tmp];

    ++TOCCacheHits;
    return cent;
}

/*
** Add given TOC and header to the cache.  Assume entry is not already in
** cache.
*/
static CAFTOCCACHEENT *
AddTOCCache(time_t timestamp, CAFTOCENT *toc, CAFHEADER head, STORAGECLASS tokenclass)
{
    CAFTOCL2CACHE *l2;
    CAFTOCL1CACHE *l1;
    CAFTOCCACHEENT *cent;
    unsigned char tmp;
    int i;

    if (TOCCache[tokenclass] == NULL) {
	TOCCache[tokenclass] = xmalloc(sizeof(CAFTOCL3CACHE));
	for (i = 0 ; i < 256 ; ++i) TOCCache[tokenclass]->l2ptr[i] = NULL;
    }

    tmp = (timestamp>>16) & 0xff;
    l2 = TOCCache[tokenclass]->l2ptr[tmp];
    if (l2 == NULL) {
	TOCCache[tokenclass]->l2ptr[tmp] = l2 = xmalloc(sizeof(CAFTOCL2CACHE));
	for (i = 0 ; i < 256 ; ++i) l2->l1ptr[i] = NULL;
    }

    tmp = (timestamp>>8) & 0xff;
    l1 = l2->l1ptr[tmp];
    if (l1 == NULL) {
	l2->l1ptr[tmp] = l1 = xmalloc(sizeof(CAFTOCL1CACHE));
	for (i = 0 ; i < 256 ; ++i) l1->entries[i] = NULL;
    }

    tmp = (timestamp) & 0xff;
    cent = xmalloc(sizeof(CAFTOCCACHEENT));
    l1->entries[tmp] = cent;

    cent->header = head;
    cent->toc = toc;
    ++TOCCacheMisses;
    return cent;
}

/*
** Do stating of an article, going thru the TOC cache if possible.
*/

static ARTHANDLE *
StatArticle(time_t timestamp, ARTNUM artnum, STORAGECLASS tokenclass)
{
    CAFTOCCACHEENT *cent;
    CAFTOCENT *toc;
    CAFHEADER head;
    char *path;
    CAFTOCENT *tocentry;
    ARTHANDLE *art;

    cent = CheckTOCCache(timestamp,tokenclass);
    if (cent == NULL) {
	path = MakePath(timestamp, tokenclass);
	toc = CAFReadTOC(path, &head);
	if (toc == NULL) {
	    if (caf_error == CAF_ERR_ARTNOTHERE) {
		SMseterror(SMERR_NOENT, NULL);
	    } else {
		SMseterror(SMERR_UNDEFINED, NULL);
	    }
	    free(path);
	    return NULL;
	}
	cent = AddTOCCache(timestamp, toc, head, tokenclass);
	free(path);
    }

    /* check current TOC for the given artnum. */
    if (artnum < cent->header.Low || artnum > cent->header.High) {
	SMseterror(SMERR_NOENT, NULL);
	return NULL;
    }

    tocentry = &(cent->toc[artnum - cent->header.Low]);
    if (tocentry->Size == 0) {
	/* no article with that article number present */
	SMseterror(SMERR_NOENT, NULL);
	return NULL;
    }

    /* stat is a success, so build a null art struct to represent that. */
    art = xmalloc(sizeof(ARTHANDLE));
    art->type = TOKEN_TIMECAF;
    art->data = NULL;
    art->len = 0;
    art->private = NULL;
    return art;
}
	

static void
CloseOpenFile(CAFOPENFILE *foo) {
    if (foo->fd >= 0) {
	close(foo->fd);
	foo->fd = -1;
	free(foo->path);
	foo->path = NULL;
    }
}

TOKEN timecaf_store(const ARTHANDLE article, const STORAGECLASS class) {
    char                *path;
    char                *p;
    time_t              now;
    time_t              timestamp;
    TOKEN               token;
    int                 fd;
    ssize_t             result;
    ARTNUM		art;

    if (article.arrived == (time_t)0)
	now = time(NULL);
    else
	now = article.arrived;

    timestamp = now>>8;
    art = 0;  /* magic: 0=="next available article number. */

    memset(&token, 0, sizeof(token));

    path = MakePath(timestamp, class);
    /* check to see if we have this CAF file already open. */
    if (WritingFile.fd < 0 || strcmp(WritingFile.path, path) != 0) {
	/* we're writing to a different file, close old one and start new one. */
	CloseOpenFile(&WritingFile);
	fd = CAFOpenArtWrite(path, &art, true, article.len);
	if (fd < 0) {
	    if (caf_error == CAF_ERR_IO && caf_errno == ENOENT) {
		/* directories in the path don't exist, try creating them. */
		p = strrchr(path, '/');
		*p = '\0';
		if (!MakeDirectory(path, true)) {
                    syswarn("timecaf: could not create directory %s", path);
		    token.type = TOKEN_EMPTY;
		    free(path);
		    SMseterror(SMERR_UNDEFINED, NULL);
		    token.type = TOKEN_EMPTY;
		    return token;
		} else {
		    *p = '/';
		    fd = CAFOpenArtWrite(path, &art, true, article.len);
		    if (fd < 0) {
                        warn("timecaf: could not OpenArtWrite %s/%ld: %s",
                             path, art, CAFErrorStr());
			SMseterror(SMERR_UNDEFINED, NULL);
			free(path);
			token.type = TOKEN_EMPTY;
			return token;
		    }
		}
	    } else {
                warn("timecaf: could not OpenArtWrite %s/%ld: %s", path, art,
                     CAFErrorStr());
		SMseterror(SMERR_UNDEFINED, NULL);
		free(path);
		token.type = TOKEN_EMPTY;
		return token;
	    }
	}
    } else {
	/* can reuse existing fd, assuming all goes well. */
	fd = WritingFile.fd;

	/* nuke extraneous copy of path to avoid mem leaks. */
	free(path);
	path = WritingFile.path;

	if (CAFStartWriteFd(fd, &art, article.len) < 0) {
            warn("timecaf: could not OpenArtWriteFd %s/%ld: %s", path, art,
                 CAFErrorStr());
	    SMseterror(SMERR_UNDEFINED, NULL);
	    free(path);
	    token.type = TOKEN_EMPTY;
	    return token;
	}
    }
    WritingFile.fd = fd;
    WritingFile.path = path;
    fdflag_close_exec(fd, true);
    result = xwritev(fd, article.iov, article.iovcnt);
    if (result != (ssize_t) article.len) {
	SMseterror(SMERR_UNDEFINED, NULL);
        syswarn("timecaf: error writing %s", path);
	token.type = TOKEN_EMPTY;
	CloseOpenFile(&WritingFile);
	return token;
    }
    if (CAFFinishArtWrite(fd) < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
        warn("timecaf: error writing %s: %s", path, CAFErrorStr());
	token.type = TOKEN_EMPTY;
	CloseOpenFile(&WritingFile);
	return token;
    }

    return MakeToken(timestamp, art, class, article.token);
}

/* Get a handle to article artnum in CAF-file path. */
static ARTHANDLE *OpenArticle(const char *path, ARTNUM artnum, const RETRTYPE amount) {
    int                 fd;
    PRIV_TIMECAF        *private;
    char                *p;
    size_t		len;
    ARTHANDLE           *art;
    static long		pagesize = 0;

    if (pagesize == 0) {
        pagesize = getpagesize();
        if (pagesize < 0) {
            syswarn("timecaf: getpagesize failed");
            pagesize = 0;
	    return NULL;
        }
    }

/* XXX need to figure some way to cache open fds or something? */
    if ((fd = CAFOpenArtRead(path, artnum, &len)) < 0) {
        if (caf_error == CAF_ERR_ARTNOTHERE) {
	    SMseterror(SMERR_NOENT, NULL);
	} else {
	    SMseterror(SMERR_UNDEFINED, NULL);
	}
	return NULL;
    }

    art = xmalloc(sizeof(ARTHANDLE));
    art->type = TOKEN_TIMECAF;

    if (amount == RETR_STAT) {
	art->data = NULL;
	art->len = 0;
	art->private = NULL;
	close(fd);
	return art;
    }

    private = xmalloc(sizeof(PRIV_TIMECAF));
    art->private = (void *)private;
    private->artlen = len;
    if (innconf->articlemmap) {
	off_t curoff, tmpoff;
	size_t delta;

	curoff = lseek(fd, (off_t) 0, SEEK_CUR);
	delta = curoff % pagesize;
	tmpoff = curoff - delta;
	private->mmaplen = len + delta;
	if ((private->mmapbase = mmap(NULL, private->mmaplen, PROT_READ, MAP_SHARED, fd, tmpoff)) == MAP_FAILED) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("timecaf: could not mmap article");
	    free(art->private);
	    free(art);
	    return NULL;
	}
	mmap_invalidate(private->mmapbase, private->mmaplen);
        if (amount == RETR_ALL)
            madvise(private->mmapbase, private->mmaplen, MADV_WILLNEED);
        else
            madvise(private->mmapbase, private->mmaplen, MADV_SEQUENTIAL);
	private->artdata = private->mmapbase + delta;
    } else {
        private->artdata = xmalloc(private->artlen);
	if (read(fd, private->artdata, private->artlen) < 0) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("timecaf: could not read article");
	    free(private->artdata);
	    free(art->private);
	    free(art);
	    return NULL;
	}
    }
    close(fd);

    private->top = NULL;
    private->sec = NULL;
    private->ter = NULL;
    private->curtoc = NULL;
    private->curartnum = 0;
    private->topde = NULL;
    private->secde = NULL;
    private->terde = NULL;

    if (amount == RETR_ALL) {
	art->data = private->artdata;
	art->len = private->artlen;
	return art;
    }

    if ((p = wire_findbody(private->artdata, private->artlen)) == NULL) {
	SMseterror(SMERR_NOBODY, NULL);
	if (innconf->articlemmap)
	    munmap(private->mmapbase, private->mmaplen);
	else
	    free(private->artdata);
	free(art->private);
	free(art);
	return NULL;
    }

    if (amount == RETR_HEAD) {
	art->data = private->artdata;
	art->len = p - private->artdata;
        /* Headers end just before the first empty line (\r\n). */
        art->len = art->len - 2;
	return art;
    }

    if (amount == RETR_BODY) {
	art->data = p + 4;
	art->len = art->len - (private->artdata - p - 4);
	return art;
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    if (innconf->articlemmap)
	munmap(private->mmapbase, private->mmaplen);
    else
	free(private->artdata);
    free(art->private);
    free(art);
    return NULL;
}

ARTHANDLE *timecaf_retrieve(const TOKEN token, const RETRTYPE amount) {
    time_t              timestamp;
    ARTNUM              artnum;
    char                *path;
    ARTHANDLE           *art;
    static TOKEN	ret_token;
    time_t		now;

    if (token.type != TOKEN_TIMECAF) {
	SMseterror(SMERR_INTERNAL, NULL);
	return NULL;
    }

    BreakToken(token, &timestamp, &artnum);

    /*
    ** Do a possible shortcut on RETR_STAT requests, going through the "TOC cache"
    ** we mentioned above.  We only try to go through the TOC Cache under these
    ** conditions:
    **   1) SMpreopen is true (so we're "preopening" the TOCs).
    **   2) the timestamp is older than the timestamp corresponding to current
    ** time.  Any timestamp that matches current time (to within 256 seconds)
    ** would be in a CAF file that innd is actively
    ** writing, in which case we would not want to cache the TOC for that
    ** CAF file.
    */

    if (SMpreopen && amount == RETR_STAT) {
	now = time(NULL);
	if (timestamp < ((now >> 8) & 0xffffff)) {
	    return StatArticle(timestamp, artnum, token.class);
	}
    }

    path = MakePath(timestamp, token.class);
    if ((art = OpenArticle(path, artnum, amount)) != (ARTHANDLE *)NULL) {
	art->arrived = timestamp<<8; /* XXX not quite accurate arrival time,
				     ** but getting a more accurate one would
				     ** require more fiddling with CAF innards.
				     */
	ret_token = token;
	art->token = &ret_token;
    }
    free(path);
    return art;
}

void timecaf_freearticle(ARTHANDLE *article) {
    PRIV_TIMECAF       *private;

    if (!article)
	return;

    if (article->private) {
	private = (PRIV_TIMECAF *)article->private;
	if (innconf->articlemmap)
	    munmap(private->mmapbase, private->mmaplen);
	else
	    free(private->artdata);
	if (private->top)
	    closedir(private->top);
	if (private->sec)
	    closedir(private->sec);
	if (private->ter)
	    closedir(private->ter);
	if (private->curtoc)
	    free(private->curtoc);
	free(private);
    }
    free(article);
}

/* Do cancels of all the article ids collected for a given pathname. */

static void
DoCancels(void) {
    if (DeletePath != NULL) {
	if (NumDeleteArtnums != 0) {
	    /*
	    ** Murgle. If we are trying to cancel something out of the
	    ** currently open-for-writing file, we need to close it before
	    ** doing CAFRemove...
	    */
	    if (WritingFile.path != NULL && strcmp(WritingFile.path, DeletePath) == 0) {
	        CloseOpenFile(&WritingFile);
	    }
	    /* XXX should really check err. code here, but not much we can really do. */
	    CAFRemoveMultArts(DeletePath, NumDeleteArtnums, DeleteArtnums);
	    free(DeleteArtnums);
	    DeleteArtnums = NULL;
	    NumDeleteArtnums = MaxDeleteArtnums = 0;
	}
	free(DeletePath);
	DeletePath = NULL;
    }
}

bool timecaf_cancel(TOKEN token) {
    time_t              now;
    ARTNUM              seqnum;
    char                *path;

    BreakToken(token, &now, &seqnum);
    path = MakePath(now, token.class);
    if (DeletePath == NULL) {
	DeletePath = path;
    } else if (strcmp(DeletePath, path) != 0) {
	/* different path, so flush all pending cancels. */
	DoCancels();
	DeletePath = path;
    } else {
	free(path); /* free redundant copy of path */
    }
    if (NumDeleteArtnums >= MaxDeleteArtnums) {
	/* allocate/expand storage for artnums. */
	if (MaxDeleteArtnums == 0) {
	    MaxDeleteArtnums = 100;
	} else {
	    MaxDeleteArtnums *= 2;
	}
        DeleteArtnums = xrealloc(DeleteArtnums, MaxDeleteArtnums * sizeof(ARTNUM));
    }
    DeleteArtnums[NumDeleteArtnums++] = seqnum;	

    return true;
}

static struct dirent *FindDir(DIR *dir, FINDTYPE type) {
    struct dirent       *de;

    while ((de = readdir(dir)) != NULL) {
        if (type == FIND_TOPDIR)
	    if ((strlen(de->d_name) == 10) &&
		(strncmp(de->d_name, "timecaf-", 8) == 0) &&
		isxdigit((unsigned char) de->d_name[8]) &&
		isxdigit((unsigned char) de->d_name[9]))
	        return de;

	if (type == FIND_DIR)
	    if ((strlen(de->d_name) == 2)
                && isxdigit((unsigned char) de->d_name[0])
                && isxdigit((unsigned char) de->d_name[1]))
		return de;

	if (type == FIND_CAF)
	    if ((strlen(de->d_name) == 7) &&
		isxdigit((unsigned char) de->d_name[0]) &&
		isxdigit((unsigned char) de->d_name[1]) &&
		isxdigit((unsigned char) de->d_name[2]) &&
		isxdigit((unsigned char) de->d_name[3]) &&
		(de->d_name[4] == '.') &&
		(de->d_name[5] == 'C') &&
		(de->d_name[6] == 'F'))
		return de;
	}

    return NULL;
}

/* Grovel thru a CAF table-of-contents finding the next still-existing article */
static int
FindNextArt(const CAFHEADER *head, CAFTOCENT *toc, ARTNUM *artp)
{
    ARTNUM art;
    CAFTOCENT *tocp;
    art = *artp;
    if (art == 0) {
	art = head->Low - 1; /* we never use art # 0, so 0 is a flag to start
			       searching at the beginning */
    }
    while (true) {
	art++;
	if (art > head->High) return false; /* ran off the end of the TOC */
	tocp = &toc[art - head->Low];
	if (tocp->Size != 0) {
	    /* got a valid article */
	    *artp = art;
	    return true;
	}
    }
}


ARTHANDLE *
timecaf_next(ARTHANDLE *article, const RETRTYPE amount)
{
    PRIV_TIMECAF	priv, *newpriv;
    char                *path;
    ARTHANDLE           *art;
    size_t              length;

    length = strlen(innconf->patharticles) + 32;
    path = xmalloc(length);
    if (article == NULL) {
	priv.top = NULL;
	priv.sec = NULL;
	priv.ter = NULL;
	priv.curtoc = NULL;
	priv.topde = NULL;
	priv.secde = NULL;
	priv.terde = NULL;
    } else {
	priv = *(PRIV_TIMECAF *)article->private;
	free(article->private);
	free(article);
	if (innconf->articlemmap)
	    munmap(priv.mmapbase, priv.mmaplen);
	else
	    free(priv.artdata);
    }

    while (priv.curtoc == NULL || !FindNextArt(&priv.curheader, priv.curtoc, &priv.curartnum)) {
	if (priv.curtoc) {
	    free(priv.curtoc);
	    priv.curtoc = NULL;
	}
	while (!priv.ter || ((priv.terde = FindDir(priv.ter, FIND_CAF)) == NULL)) {
	    if (priv.ter) {
		closedir(priv.ter);
		priv.ter = NULL;
	    }
	    while (!priv.sec || ((priv.secde = FindDir(priv.sec, FIND_DIR)) == NULL)) {
	        if (priv.sec) {
		    closedir(priv.sec);
		    priv.sec = NULL;
		}
		if (!priv.top || ((priv.topde = FindDir(priv.top, FIND_TOPDIR)) == NULL)) {
		    if (priv.top) {
			/* end of search */
			closedir(priv.top);
			priv.top = NULL;
			free(path);
			return NULL;
		    }
		    snprintf(path, length, "%s", innconf->patharticles);
		    if ((priv.top = opendir(path)) == NULL) {
			SMseterror(SMERR_UNDEFINED, NULL);
			free(path);
			return NULL;
		    }
		    if ((priv.topde = FindDir(priv.top, FIND_TOPDIR)) == NULL) {
			SMseterror(SMERR_UNDEFINED, NULL);
			closedir(priv.top);
			free(path);
			return NULL;
		    }
		}
		snprintf(path, length, "%s/%s", innconf->patharticles, priv.topde->d_name);
		if ((priv.sec = opendir(path)) == NULL)
		    continue;
	    }
	    snprintf(path, length, "%s/%s/%s", innconf->patharticles, priv.topde->d_name, priv.secde->d_name);
	    if ((priv.ter = opendir(path)) == NULL)
		continue;
	}
	snprintf(path, length, "%s/%s/%s/%s", innconf->patharticles, priv.topde->d_name, priv.secde->d_name, priv.terde->d_name);
	if ((priv.curtoc = CAFReadTOC(path, &priv.curheader)) == NULL)
	    continue;
	priv.curartnum = 0;
    }
    snprintf(path, length, "%s/%s/%s/%s", innconf->patharticles, priv.topde->d_name, priv.secde->d_name, priv.terde->d_name);
    art = OpenArticle(path, priv.curartnum, amount);
    if (art == (ARTHANDLE *)NULL) {
	art = xmalloc(sizeof(ARTHANDLE));
	art->type = TOKEN_TIMECAF;
	art->data = NULL;
	art->len = 0;
	art->private = xmalloc(sizeof(PRIV_TIMECAF));
    }
    newpriv = (PRIV_TIMECAF *)art->private;
    newpriv->top = priv.top;
    newpriv->sec = priv.sec;
    newpriv->ter = priv.ter;
    newpriv->topde = priv.topde;
    newpriv->secde = priv.secde;
    newpriv->terde = priv.terde;
    newpriv->curheader = priv.curheader;
    newpriv->curtoc = priv.curtoc;
    newpriv->curartnum = priv.curartnum;

    snprintf(path, length, "%s/%s/%s", priv.topde->d_name, priv.secde->d_name, priv.terde->d_name);
    art->token = PathNumToToken(path, priv.curartnum);
    art->arrived = priv.curtoc[priv.curartnum - priv.curheader.Low].ModTime;
    free(path);
    return art;
}

bool timecaf_ctl(PROBETYPE type, TOKEN *token UNUSED, void *value) {
    struct artngnum *ann;

    switch (type) {
    case SMARTNGNUM:
	if ((ann = (struct artngnum *)value) == NULL)
	    return false;
	/* make SMprobe() call timecaf_retrieve() */
	ann->artnum = 0;
	return true;
    default:
	return false;
    }
}

bool timecaf_flushcacheddata(FLUSHTYPE type) {
    if (type == SM_ALL || type == SM_CANCELLEDART)
	DoCancels();
    return true;
}

void
timecaf_printfiles(FILE *file, TOKEN token, char **xref UNUSED,
                   int ngroups UNUSED)
{
    fprintf(file, "%s\n", TokenToText(token));
}

void timecaf_shutdown(void) {
    CloseOpenFile(&WritingFile);
    DoCancels();
}
