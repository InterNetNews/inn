/*  $Id$
**
**  Indexed overview method.
*/
#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/stat.h>
#ifdef SYNCGINDEXMMAP
#include <limits.h>
#endif

#include "inn/innconf.h"
#include "libinn.h"
#include "macros.h"
#include "ov.h"
#include "paths.h"
#include "ov.h"
#include "ovinterface.h"
#include "tradindexed.h"
#include "storage.h"

#include "structure.h"

typedef struct {
    HASH               hash;
    char               *group;
    GROUPLOC           gloc;
    int                indexfd;
    int                datafd;
    INDEXENTRY         *indexmem;
    char               *datamem;
    int                indexlen;
    int                datalen;
    ino_t              indexinode;
} GROUPHANDLE;

typedef struct {
    char                *group;
    int                 limit;
    int                 base;
    int                 cur;
    GROUPHANDLE         *gh;
} OV3SEARCH;

typedef struct _CR {
    GROUPHANDLE        *gh;
    time_t             lastused;      /* time this was last used */
    int                refcount;      /* Number of things currently using this */
    struct _CR         *next;
} CACHEENTRY;

/*
**  A buffer; re-uses space.
*/
typedef struct _BUFFER {
    int		Used;
    int		Size;
    char	*Data;
} BUFFER;

#define CACHETABLESIZE 128
#define MAXCACHETIME (60*5)

static int              GROUPfd;
static GROUPHEADER      *GROUPheader = NULL;
static GROUPENTRY       *GROUPentries = NULL;
static int              GROUPcount = 0;
static GROUPLOC         GROUPemptyloc = { -1 };

static CACHEENTRY *CACHEdata[CACHETABLESIZE];
static int OV3mode;
static int OV3padamount = 128;
static int CACHEentries = 0;
static int CACHEhit = 0;
static int CACHEmiss = 0;
static int CACHEmaxentries = 128;
static bool Cutofflow;

#ifdef SYNCGINDEXMMAP
STATIC char		GROUPfn[PATH_MAX+1];
#endif

static GROUPLOC GROUPnewnode(void);
static bool GROUPremapifneeded(GROUPLOC loc);
static void GROUPLOCclear(GROUPLOC *loc);
static bool GROUPLOCempty(GROUPLOC loc);
static bool GROUPlockhash(enum inn_locktype type);
static bool GROUPlock(GROUPLOC gloc, enum inn_locktype type);
static off_t GROUPfilesize(int count);
static bool GROUPexpand(int mode);
static bool OV3packgroup(char *group, int delta);
static GROUPHANDLE *OV3opengroup(char *group, bool needcache);
static void OV3cleancache(void);
static bool OV3addrec(GROUPENTRY *ge, GROUPHANDLE *gh, int artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires);
static bool OV3closegroup(GROUPHANDLE *gh, bool needcache);
static void OV3getdirpath(char *group, char *path);
static void OV3getIDXfilename(char *group, char *path);
static void OV3getDATfilename(char *group, char *path);

void freeargify(char ***argvp) {
    if (**argvp != NULL) {
	DISPOSE(*argvp[0]);
	DISPOSE(*argvp);
	*argvp = NULL;
    }
}

/*
**  Parse a string into a NULL-terminated array of words; return number
**  of words.  If argvp isn't NULL, it and what it points to will be
**  DISPOSE'd.
*/
int argify(char *line, char ***argvp)
{
    char	        **argv;
    char	        *p;
    int	                i;

    if (*argvp != NULL)
	freeargify(argvp);

    /*  Copy the line, which we will split up. */
    while (ISWHITE(*line))
	line++;
    i = strlen(line);
    p = NEW(char, i + 1);
    (void)strcpy(p, line);

    /* Allocate worst-case amount of space. */
    for (*argvp = argv = NEW(char*, i + 2); *p; ) {
	/* Mark start of this word, find its end. */
	for (*argv++ = p; *p && !ISWHITE(*p); )
	    p++;
	if (*p == '\0')
	    break;

	/* Nip off word, skip whitespace. */
	for (*p++ = '\0'; ISWHITE(*p); )
	    p++;
    }
    *argv = NULL;
    return argv - *argvp;
}

bool tradindexed_open(int mode) {
    char                dirname[1024];
    char                *groupfn;
    struct stat         sb;
    int                 fdlimit;
    int                 flag = 0;
    int                 openmode;

    OV3mode = mode;
    if (!(OV3mode & OV_WRITE))
	CACHEmaxentries = 1;
    else
	CACHEmaxentries = innconf->overcachesize;
    fdlimit = getfdlimit();
    if (fdlimit > 0 && fdlimit < CACHEmaxentries * 2) {
        syslog(L_FATAL, "tradindexed: not enough file descriptors for"
               " overview cache, increase rlimitnofile or decrease"
               " overcachesize");
        return FALSE;
    }
    memset(&CACHEdata, '\0', sizeof(CACHEdata));
    
    strcpy(dirname, innconf->pathoverview);
    groupfn = NEW(char, strlen(dirname) + strlen("/group.index") + 1);
    strcpy(groupfn, dirname);
    strcat(groupfn, "/group.index");
#ifdef SYNCGINDEXMMAP
    strncpy(GROUPfn, groupfn, PATH_MAX);
#endif
    if (mode & OV_WRITE) {
	openmode = O_RDWR | O_CREAT;
    } else {
	openmode = O_RDONLY;
    }
    GROUPfd = open(groupfn, openmode, ARTFILE_MODE);
    if (GROUPfd < 0) {
	syslog(L_FATAL, "tradindexed: could not create %s: %m", groupfn);
	DISPOSE(groupfn);
	return FALSE;
    }

    if (fstat(GROUPfd, &sb) < 0) {
	syslog(L_FATAL, "tradindexed: could not fstat %s: %m", groupfn);
	DISPOSE(groupfn);
	close(GROUPfd);
	return FALSE;
    }
    if (sb.st_size > sizeof(GROUPHEADER)) {
	if (innconf->tradindexedmmap) {
	    if (mode & OV_READ)
		flag |= PROT_READ;
	    if (mode & OV_WRITE) {
		/* 
		 * Note: below mapping of groupheader won't work
		 * unless we have both PROT_READ and PROT_WRITE perms.
		 */
		flag |= PROT_WRITE|PROT_READ;
	    }
	}
	GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
	if (innconf->tradindexedmmap) {
	    if ((GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount), flag,
						   MAP_SHARED, GROUPfd, 0)) == (GROUPHEADER *) -1) {
		syslog(L_FATAL, "tradindexed: could not mmap %s in tradindexed_open: %m", groupfn);
		DISPOSE(groupfn);
		close(GROUPfd);
		return FALSE;
	    }
	} else {
	    GROUPheader = NEW(GROUPHEADER *, GROUPfilesize(GROUPcount));

	    if (GROUPfilesize(GROUPcount) != read(GROUPfd, (void *)GROUPheader, GROUPfilesize(GROUPcount))) {
		syslog(L_FATAL, "tradindexed: could not read %s in tradindexed_open: %m", groupfn);
		DISPOSE(GROUPheader);
		DISPOSE(groupfn);
		close(GROUPfd);
		return FALSE;
	    }
	}

	GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    } else {
	GROUPcount = 0;
	if (!GROUPexpand(mode)) {
	    DISPOSE(groupfn);
	    close(GROUPfd);
	    return FALSE;
	}
    }
    close_on_exec(GROUPfd, true);
    
    DISPOSE(groupfn);
    Cutofflow = FALSE;

    return TRUE;
}

static GROUPLOC GROUPfind(char *group) {
    HASH                grouphash;
    unsigned int        i;
    GROUPLOC            loc;
    
    grouphash = Hash(group, strlen(group));
    memcpy(&i, &grouphash, sizeof(i));

    loc = GROUPheader->hash[i % GROUPHEADERHASHSIZE];
    GROUPremapifneeded(loc);

    while (!GROUPLOCempty(loc)) {
	if (GROUPentries[loc.recno].deleted == 0) {
	    if (memcmp(&grouphash, &GROUPentries[loc.recno].hash, sizeof(HASH)) == 0) {
		return loc;
	    }
	}
	loc = GROUPentries[loc.recno].next;
    }
    return GROUPemptyloc;
} 

bool tradindexed_groupstats(char *group, int *lo, int *hi, int *count, int *flag) {
    GROUPLOC            gloc;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc)) {
	return FALSE;
    }
    if (lo != NULL)
	*lo = GROUPentries[gloc.recno].low;
    if (hi != NULL)
	*hi = GROUPentries[gloc.recno].high;
    if (count != NULL)
	*count = GROUPentries[gloc.recno].count;
    if (flag != NULL)
	*flag = GROUPentries[gloc.recno].flag;
    return TRUE;
}

bool tradindexed_groupadd(char *group, ARTNUM lo, ARTNUM hi, char *flag) {
    unsigned int        i;
    HASH                grouphash;
    GROUPLOC            loc;
    GROUPENTRY          *ge;

    loc = GROUPfind(group);
    if (!GROUPLOCempty(loc)) {
	GROUPentries[loc.recno].flag = *flag;
	return TRUE;
    }
    grouphash = Hash(group, strlen(group));
    memcpy(&i, &grouphash, sizeof(i));
    i = i % GROUPHEADERHASHSIZE;
    GROUPlockhash(INN_LOCK_WRITE);
    loc = GROUPnewnode();
    ge = &GROUPentries[loc.recno];
    ge->hash = grouphash;
    if (lo != 0)
	ge->low = lo;
    ge->high = hi;
    ge->deleted = ge->low = ge->base = ge->count = 0;
    ge->flag = *flag;
    ge->next = GROUPheader->hash[i];
    GROUPheader->hash[i] = loc;
    GROUPlockhash(INN_LOCK_UNLOCK);
    return TRUE;
}

static off_t GROUPfilesize(int count) {
    return ((off_t)count * sizeof(GROUPENTRY)) + sizeof(GROUPHEADER);
}

/* Check if the given GROUPLOC refers to GROUPENTRY that we don't have mmap'ed,
** if so then see if the file has been grown by another writer and remmap
*/
static bool GROUPremapifneeded(GROUPLOC loc) {
    struct stat         sb;
    
    if (loc.recno < GROUPcount)
	return TRUE;

    if (fstat(GROUPfd, &sb) < 0)
	return FALSE;

    if (GROUPfilesize(GROUPcount) >= sb.st_size)
	return TRUE;

    if (GROUPheader) {
	if (innconf->tradindexedmmap) {
	    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
		syslog(L_FATAL, "tradindexed: could not munmap group.index in GROUPremapifneeded: %m");
		return FALSE;
	    }
	} else {
	    DISPOSE(GROUPheader);
	}
    }

    GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
    if (innconf->tradindexedmmap) {
	int flag = PROT_READ;

	if (OV3mode & OV_WRITE) {
	    flag |= PROT_WRITE;
	}
	GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
					  flag, MAP_SHARED, GROUPfd, 0);
	if (GROUPheader == (GROUPHEADER *) -1) {
	    syslog(L_FATAL, "tradindexed: could not mmap group.index in GROUPremapifneeded: %m");
	    return FALSE;
	}
    } else {
	GROUPheader = NEW(GROUPHEADER *, GROUPfilesize(GROUPcount));

	if (GROUPfilesize(GROUPcount) != read(GROUPfd, (void *)GROUPheader, GROUPfilesize(GROUPcount))) {
	    syslog(L_FATAL, "tradindexed: could not read group.index in GROUPremapifneeded: %m");
	    DISPOSE(GROUPheader);
	    return FALSE;
	}
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    return TRUE;
}

/* This function does not need to lock because it's callers are expected to do so */
static bool GROUPexpand(int mode) {
    int                 i;
    int                 flag = 0;
    
    if (GROUPheader) {
	if (innconf->tradindexedmmap) {
	    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
		syslog(L_FATAL, "tradindexed: could not munmap group.index in GROUPexpand: %m");
		return FALSE;
	    }
	} else {
	    DISPOSE(GROUPheader);
	}
    }
    GROUPcount += 1024;
    if (ftruncate(GROUPfd, GROUPfilesize(GROUPcount)) < 0) {
	syslog(L_FATAL, "tradindexed: could not extend group.index: %m");
	return FALSE;
    }
    if (innconf->tradindexedmmap) {
	if (mode & OV_READ)
	    flag |= PROT_READ;
	if (mode & OV_WRITE) {
	    /* 
	     * Note: below check of magic won't work unless we have
	     * both PROT_READ and PROT_WRITE perms.
	     */
	    flag |= PROT_WRITE|PROT_READ;
	}
	GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
					  flag, MAP_SHARED, GROUPfd, 0);
	if (GROUPheader == (GROUPHEADER *) -1) {
	    syslog(L_FATAL, "tradindexed: could not mmap group.index in GROUPexpand: %m");
	    return FALSE;
	}
    } else {
	GROUPheader = NEW(GROUPHEADER *, GROUPfilesize(GROUPcount));

	if (GROUPfilesize(GROUPcount) != read(GROUPfd, (void *)GROUPheader, GROUPfilesize(GROUPcount))) {
	    syslog(L_FATAL, "tradindexed: could not read group.index in GROUPexpand: %m");
	    DISPOSE(GROUPheader);
	    return FALSE;
	}
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    if (GROUPheader->magic != GROUPHEADERMAGIC) {
	GROUPheader->magic = GROUPHEADERMAGIC;
	GROUPLOCclear(&GROUPheader->freelist);
	for (i = 0; i < GROUPHEADERHASHSIZE; i++)
	    GROUPLOCclear(&GROUPheader->hash[i]);
    }
    /* Walk the new entries from the back to the front, adding them to the freelist */
    for (i = GROUPcount - 1; (GROUPcount - 1024) <= i; i--) {
	GROUPentries[i].next = GROUPheader->freelist;
	GROUPheader->freelist.recno = i;
    }
    return TRUE;
}

static GROUPLOC GROUPnewnode(void) {
    GROUPLOC            loc;
    
    /* If we didn't find any free space, then make some */
    if (GROUPLOCempty(GROUPheader->freelist)) {
	if (!GROUPexpand(OV3mode)) {
	    return GROUPemptyloc;
	}
    }
    assert(!GROUPLOCempty(GROUPheader->freelist));
    loc = GROUPheader->freelist;
    GROUPheader->freelist = GROUPentries[GROUPheader->freelist.recno].next;
    return loc;
}

bool tradindexed_groupdel(char *group) {
    GROUPLOC            gloc;
    char                *sepgroup;
    char                *p;
    char                IDXpath[BIG_BUFFER];
    char                DATpath[BIG_BUFFER];
    char                **groupparts = NULL;
    int                 i, j;
    
    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return TRUE;

    GROUPentries[gloc.recno].deleted = time(NULL);
    HashClear(&GROUPentries[gloc.recno].hash);

    sepgroup = COPY(group);
    for (p = sepgroup; *p != '\0'; p++)
	if (*p == '.')
	    *p = ' ';
    
    i = argify(sepgroup, &groupparts);
    DISPOSE(sepgroup);
    strcpy(IDXpath, innconf->pathoverview);
    strcat(IDXpath, "/");
    for (p = IDXpath + strlen(IDXpath), j = 0; j < i; j++) {
	*p++ = groupparts[j][0];
	*p++ = '/';
    }
    *p = '\0';
    freeargify(&groupparts);

    sprintf(p, "%s.DAT", group);
    strcpy(DATpath, IDXpath);
    sprintf(p, "%s.IDX", group);
    unlink(IDXpath);
    unlink(DATpath);

    return TRUE;
}

static void GROUPLOCclear(GROUPLOC *loc) {
    loc->recno = -1;
}

static bool GROUPLOCempty(GROUPLOC loc) {
    return (loc.recno < 0);
}

static bool GROUPlockhash(enum inn_locktype type) {
    return inn_lock_range(GROUPfd, type, true, 0, sizeof(GROUPHEADER));
}

static bool GROUPlock(GROUPLOC gloc, enum inn_locktype type) {
    return inn_lock_range(GROUPfd,
			  type,
			  true,
			  sizeof(GROUPHEADER) + (sizeof(GROUPENTRY) * gloc.recno),
			  sizeof(GROUPENTRY));
}

static bool OV3mmapgroup(GROUPHANDLE *gh) {
    struct stat         sb;
    
    if (fstat(gh->datafd, &sb) < 0) {
	syslog(L_ERROR, "tradindexed: could not fstat data file for %s: %m", gh->group);
	return FALSE;
    }
    gh->datalen = sb.st_size;
    if (fstat(gh->indexfd, &sb) < 0) {
	syslog(L_ERROR, "tradindexed: could not fstat index file for %s: %m", gh->group);
	return FALSE;
    }
    gh->indexlen = sb.st_size;
    if (gh->datalen == 0 || gh->indexlen == 0) {
	gh->datamem = (char *)-1;
	gh->indexmem = (INDEXENTRY *)-1;
	return TRUE;
    }
    if (innconf->tradindexedmmap) {
	if (!gh->datamem) {
	    if ((gh->datamem = (char *)mmap(0, gh->datalen, PROT_READ, MAP_SHARED,
					    gh->datafd, 0)) == (char *)-1) {
		syslog(L_ERROR, "tradindexed: could not mmap data file for %s: %m", gh->group);
		return FALSE;
	    }
	}
    } else {
	if (gh->datamem)
	    free (gh->datamem);

	gh->datamem = NEW(char, gh->datalen);
	if (gh->datalen != read(gh->datafd, (void *)gh->datamem, gh->datalen)) {
	    syslog(L_ERROR, "tradindexed: could not read data file for %s: %m", gh->group);
	    DISPOSE(gh->datamem);
	    return FALSE;
	}
    }
    if (innconf->tradindexedmmap) {
	if (!gh->indexmem) {
	    if ((gh->indexmem = (INDEXENTRY *)mmap(0, gh->indexlen, PROT_READ, MAP_SHARED, 
						   gh->indexfd, 0)) == (INDEXENTRY *)-1) {
		syslog(L_ERROR, "tradindexed: could not mmap index file for  %s: %m", gh->group);
		munmap(gh->datamem, gh->datalen);
		return FALSE;
	    }
	}
    } else {
	if (gh->indexmem)
	    DISPOSE(gh->indexmem);

	gh->indexmem = NEW(char, gh->indexlen);
	if (gh->indexlen != read(gh->indexfd, (void *)gh->indexmem, gh->indexlen)) {
	    syslog(L_ERROR, "tradindexed: could not read index file for  %s: %m", gh->group);
	    DISPOSE(gh->indexmem);
	    DISPOSE(gh->datamem);
	    return FALSE;
	}
    }
    return TRUE;
}

static GROUPHANDLE *OV3opengroupfiles(char *group) {
    char                *sepgroup;
    char                *p;
    char                IDXpath[BIG_BUFFER];
    char                DATpath[BIG_BUFFER];
    char                **groupparts = NULL;
    GROUPHANDLE         *gh;
    int                 i, j;
    struct stat         sb;
    int                 openmode;

    sepgroup = COPY(group);
    for (p = sepgroup; *p != '\0'; p++)
	if (*p == '.')
	    *p = ' ';
    
    i = argify(sepgroup, &groupparts);
    DISPOSE(sepgroup);
    strcpy(IDXpath, innconf->pathoverview);
    strcat(IDXpath, "/");
    for (p = IDXpath + strlen(IDXpath), j = 0; j < i; j++) {
	*p++ = groupparts[j][0];
	*p++ = '/';
    }
    *p = '\0';
    freeargify(&groupparts);

    sprintf(p, "%s.DAT", group);
    strcpy(DATpath, IDXpath);
    sprintf(p, "%s.IDX", group);

    gh = NEW(GROUPHANDLE, 1);
    memset(gh, '\0', sizeof(GROUPHANDLE));
    if (OV3mode & OV_WRITE) {
	openmode = O_RDWR | O_CREAT;
    } else {
	openmode = O_RDONLY;
    }
    gh->datafd = open(DATpath, openmode | O_APPEND, 0660);
    if (gh->datafd < 0 && (OV3mode & OV_WRITE)) {
	p = strrchr(IDXpath, '/');
	*p = '\0';
	if (!MakeDirectory(IDXpath, TRUE)) {
	    syslog(L_ERROR, "tradindexed: could not create directory %s", IDXpath);
	    return NULL;
	}
	*p = '/';
	gh->datafd = open(DATpath, openmode | O_APPEND, 0660);
    }
    if (gh->datafd < 0) {
	DISPOSE(gh);
	if (errno == ENOENT)
	    return NULL;
	syslog(L_ERROR, "tradindexed: could not open %s: %m", DATpath);
	return NULL;
    }
    gh->indexfd = open(IDXpath, openmode, 0660);
    if (gh->indexfd < 0) {
	close(gh->datafd);
	DISPOSE(gh);
	syslog(L_ERROR, "tradindexed: could not open %s: %m", IDXpath);
	return NULL;
    }
    if (fstat(gh->indexfd, &sb) < 0) {
	close(gh->datafd);
	close(gh->indexfd);
	DISPOSE(gh);
	syslog(L_ERROR, "tradindexed: could not fstat %s: %m", IDXpath);
	return NULL;
    }
    close_on_exec(gh->datafd, true);
    close_on_exec(gh->indexfd, true);
    gh->indexinode = sb.st_ino;
    gh->indexlen = gh->datalen = -1;
    gh->indexmem = NULL;
    gh->datamem = NULL;
    gh->group = COPY(group);
    return gh;
}

static void OV3closegroupfiles(GROUPHANDLE *gh) {
    close(gh->indexfd);
    close(gh->datafd);
    if (gh->indexmem)
    if (innconf->tradindexedmmap) {
	munmap((void *)gh->indexmem, gh->indexlen);
    } else {
	DISPOSE(gh->indexmem);
    }
    if (gh->datamem)
    if (innconf->tradindexedmmap) {
	munmap(gh->datamem, gh->datalen);
    } else {
	DISPOSE(gh->datamem);
    }
    if (gh->group)
	DISPOSE(gh->group);
    DISPOSE(gh);
#ifdef SYNCGINDEXMMAP
/* msync async to not hit the feed too badly, may be altogether superfluous */
    mmap_flush(GROUPheader, GROUPfilesize(GROUPcount));
    utime(GROUPfn, NULL);
#endif

}


static void OV3cleancache(void) {
    int                 i;
    CACHEENTRY          *ce, *prev;
    CACHEENTRY          *saved = NULL;
    CACHEENTRY          *sprev = NULL;
    int                 savedbucket;
    
    while (CACHEentries >= CACHEmaxentries) {
	for (i = 0; i < CACHETABLESIZE; i++) {
	    for (prev = NULL, ce = CACHEdata[i]; ce != NULL; prev = ce, ce = ce->next) {
		if (ce->refcount > 0)
		    continue;
		if ((saved == NULL) || (saved->lastused > ce->lastused)) {
		    saved = ce;
		    sprev = prev;
		    savedbucket = i;
		}
	    }
	}

	if (saved != NULL) {
	    OV3closegroupfiles(saved->gh);
	    if (sprev) {
		sprev->next = saved->next;
	    } else {
		CACHEdata[savedbucket] = saved->next;
	    }
	    DISPOSE(saved);
	    CACHEentries--;
	    return;
	}
	syslog(L_NOTICE, "tradindexed: group cache is full, waiting 10 seconds");
	sleep(10);
    }
}


static GROUPHANDLE *OV3opengroup(char *group, bool needcache) {
    unsigned int        hashbucket;
    GROUPHANDLE         *gh;
    HASH                hash;
    CACHEENTRY          *ce, *prev;
    time_t              Now;
    GROUPLOC            gloc;
    GROUPENTRY          *ge;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return NULL;

    ge = &GROUPentries[gloc.recno];
    if (needcache) {
      Now = time(NULL);
      hash = Hash(group, strlen(group));
      memcpy(&hashbucket, &hash, sizeof(hashbucket));
      hashbucket %= CACHETABLESIZE;
      for (prev = NULL, ce = CACHEdata[hashbucket]; ce != NULL; prev = ce, ce = ce->next) {
	if (memcmp(&ce->gh->hash, &hash, sizeof(hash)) == 0) {
	    if (((Now - ce->lastused) > MAXCACHETIME) ||
		(GROUPentries[gloc.recno].indexinode != ce->gh->indexinode)) {
		OV3closegroupfiles(ce->gh);
		if (prev)
		    prev->next = ce->next;
		else
		    CACHEdata[hashbucket] = ce->next;
		DISPOSE(ce);
		CACHEentries--;
		break;
	    }
	    CACHEhit++;
	    ce->lastused = Now;
	    ce->refcount++;
	    return ce->gh;
	}
      }
      OV3cleancache();
      CACHEmiss++;
    }
    if ((gh = OV3opengroupfiles(group)) == NULL)
	return NULL;
    gh->hash = hash;
    gh->gloc = gloc;
    if (OV3mode & OV_WRITE) {
	/*
	 * Don't try to update ge->... unless we are in write mode, since
	 * otherwise we can't write to that mapped region. 
	 */
	ge->indexinode = gh->indexinode;
    }
    if (needcache) {
      ce = NEW(CACHEENTRY, 1);
      memset(ce, '\0', sizeof(*ce));
      ce->gh = gh;
      ce->refcount++;
      ce->lastused = Now;
    
      /* Insert into the list */
      ce->next = CACHEdata[hashbucket];
      CACHEdata[hashbucket] = ce;
      CACHEentries++;
    }
    return gh;
}

static bool OV3closegroup(GROUPHANDLE *gh, bool needcache) {
    unsigned int        i;
    CACHEENTRY          *ce;

    if (needcache) {
      memcpy(&i, &gh->hash, sizeof(i));
      i %= CACHETABLESIZE;
      for (ce = CACHEdata[i]; ce != NULL; ce = ce->next) {
	if (memcmp(&ce->gh->hash, &gh->hash, sizeof(HASH)) == 0) {
	    ce->refcount--;
	    break;
	}
      }
    } else
      OV3closegroupfiles(gh);
    return TRUE;
}

static bool OV3addrec(GROUPENTRY *ge, GROUPHANDLE *gh, int artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
    INDEXENTRY          ie;
    int                 base;
    
    if (ge->base == 0) {
	base = artnum > OV3padamount ? artnum - OV3padamount : 1;
    } else {
	base = ge->base;
	if (ge->base > artnum) {
	    syslog(L_ERROR, "tradindexed: could not add %s:%d, base == %d", gh->group, artnum, ge->base);
	    return FALSE;
	}
    }
    memset(&ie, '\0', sizeof(ie));
    if (write(gh->datafd, data, len) != len) {
	syslog(L_ERROR, "tradindexed: could not append overview record to %s: %m", gh->group);
	return FALSE;
    }
    if ((ie.offset = lseek(gh->datafd, 0, SEEK_CUR)) < 0) {
	syslog(L_ERROR, "tradindexed: could not get offset of overview record in %s: %m", gh->group);
	return FALSE;
    }
    ie.length = len;
    ie.offset -= ie.length;
    ie.arrived = arrived;
    ie.expires = expires;
    ie.token = token;
    
    if (xpwrite(gh->indexfd, &ie, sizeof(ie), (artnum - base) * sizeof(ie)) != sizeof(ie)) {
	syslog(L_ERROR, "tradindexed: could not write index record for %s:%d", gh->group, artnum);
	return FALSE;
    }
    if ((ge->low <= 0) || (ge->low > artnum))
	ge->low = artnum;
    if ((ge->high <= 0) || (ge->high < artnum))
	ge->high = artnum;
    ge->count++;
    ge->base = base;
    return TRUE;
}

bool tradindexed_add(char *group, ARTNUM artnum, TOKEN token, char *data, int len, time_t arrived, time_t expires) {
    GROUPHANDLE         *gh;
    GROUPENTRY		*ge;
    GROUPLOC            gloc;

    if ((gh = OV3opengroup(group, TRUE)) == NULL) {
	/*
	** check to see if group is in group.index, and if not, just 
	** continue (presumably the group was rmgrouped earlier, but we
	** still have articles referring to it in spool).
	*/
	gloc = GROUPfind(group);
	if (GROUPLOCempty(gloc))
	    return TRUE;
	/*
	** group was there, but open failed for some reason, return err.
	*/
	return FALSE;
    }	

    /* pack group if needed. */
    ge = &GROUPentries[gh->gloc.recno];
    if (Cutofflow && ge->low > artnum) {
	OV3closegroup(gh, TRUE);
	return TRUE;
    }
    if (ge->base > artnum) {
	if (!OV3packgroup(group, OV3padamount + ge->base - artnum)) {
	    OV3closegroup(gh, TRUE);
	    return FALSE;
	}
	/* sigh. need to close and reopen group after packing. */
	OV3closegroup(gh, TRUE);
	if ((gh = OV3opengroup(group, TRUE)) == NULL)
	    return FALSE;
    }
    GROUPlock(gh->gloc, INN_LOCK_WRITE);
    OV3addrec(ge, gh, artnum, token, data, len, arrived, expires);
    GROUPlock(gh->gloc, INN_LOCK_UNLOCK);
    OV3closegroup(gh, TRUE);

    return TRUE;
}

bool tradindexed_cancel(TOKEN token) {
    return TRUE;
}

void *tradindexed_opensearch(char *group, int low, int high) {
    GROUPHANDLE         *gh;
    GROUPENTRY          *ge;
    ino_t               oldinode;
    int                 base;
    OV3SEARCH           *search;
    
    if ((gh = OV3opengroup(group, FALSE)) == NULL)
	return NULL;

    ge = &GROUPentries[gh->gloc.recno];
    /* It's possible that you could get two different files with the
       same inode here, but it's incredibly unlikely considering the
       run time of this loop */
    do {
	oldinode = ge->indexinode;
	if (low < ge->low)
	    low = ge->low;
	if (high > ge->high)
	    high = ge->high;
	base = ge->base;
    } while (ge->indexinode != oldinode);

    /* Adjust the searching range if low < base since the active file can
       be out of sync with the overview database and this is where the
       initial lowmark comes from. */
    if (high < base) {
        OV3closegroup(gh, FALSE);
        return NULL;
    }
    if (low < base)
       low = base;

    if (!OV3mmapgroup(gh)) {
	OV3closegroup(gh, FALSE);
	return NULL;
    }
    
    search = NEW(OV3SEARCH, 1);
    search->limit = high - base;
    search->cur = low - base;
    search->base = base;
    search->gh = gh;
    search->group = COPY(group);
    return (void *)search;
}

bool ov3search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived, time_t *expires) {
    OV3SEARCH           *search = (OV3SEARCH *)handle;
    INDEXENTRY           *ie;

    if (search->gh->datamem == (char *)-1 || search->gh->indexmem == (INDEXENTRY *)-1)
	return FALSE;
    for (ie = search->gh->indexmem;
	 ((char *)&ie[search->cur] < (char *)search->gh->indexmem + search->gh->indexlen) &&
	     (search->cur <= search->limit) &&
	     (ie[search->cur].length == 0);
	 search->cur++);

    if (search->cur > search->limit)
	return FALSE;

    if ((char *)&ie[search->cur] >= (char *)search->gh->indexmem + search->gh->indexlen) {
	/* don't claim, since highest article may be canceled and there may be
	   no index room for it */
	return FALSE;
    }

    ie = &ie[search->cur];
    if (ie->offset > search->gh->datalen || ie->offset + ie->length > search->gh->datalen)
	/* index may be corrupted, do not go further */
	return FALSE;

    if (artnum)
	*artnum = search->base + search->cur;
    if (len)
	*len = ie->length;
    if (data)
	*data = search->gh->datamem + ie->offset;
    if (token)
	*token = ie->token;
    if (arrived)
	*arrived = ie->arrived;
    if (expires)
	*expires = ie->expires;

    search->cur++;

    return TRUE;
}

bool tradindexed_search(void *handle, ARTNUM *artnum, char **data, int *len, TOKEN *token, time_t *arrived) {
  return(ov3search(handle, artnum, data, len, token, arrived, NULL));
}

void tradindexed_closesearch(void *handle) {
    OV3SEARCH           *search = (OV3SEARCH *)handle;

    OV3closegroup(search->gh, FALSE);
    DISPOSE(search->group);
    DISPOSE(search);
}

bool tradindexed_getartinfo(char *group, ARTNUM artnum, TOKEN *token) {
    void                *handle;
    bool                retval;
    if (!(handle = tradindexed_opensearch(group, artnum, artnum)))
	return FALSE;
    retval = tradindexed_search(handle, NULL, NULL, NULL, token, NULL);
    tradindexed_closesearch(handle);
    return retval;
}

static void OV3getdirpath(char *group, char *path) {
    char                *sepgroup;
    char                *p;
    char                **groupparts = NULL;
    int                 i, j;
    
    sepgroup = COPY(group);
    for (p = sepgroup; *p != '\0'; p++)
	if (*p == '.')
	    *p = ' ';
    
    i = argify(sepgroup, &groupparts);
    DISPOSE(sepgroup);
    strcpy(path, innconf->pathoverview);
    strcat(path, "/");
    for (p = path + strlen(path), j = 0; j < i; j++) {
	*p++ = groupparts[j][0];
	*p++ = '/';
    }
    *p = '\0';
    freeargify(&groupparts);
}

static void OV3getIDXfilename(char *group, char *path) {
    char                *p;

    OV3getdirpath(group, path);
    p = path + strlen(path);
    sprintf(p, "%s.IDX", group);
}

static void OV3getDATfilename(char *group, char *path) {
    char                *p;

    OV3getdirpath(group, path);
    p = path + strlen(path);
    sprintf(p, "%s.DAT", group);
}

/*
 * Shift group index file so it has lower value of base.
 */

static bool OV3packgroup(char *group, int delta) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    GROUPENTRY		*ge;
    GROUPLOC            gloc;
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    struct stat         sb;
    int			fd;
    GROUPHANDLE		*gh;

    if (delta <= 0) return FALSE;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return FALSE;
    ge = &GROUPentries[gloc.recno];
    if (ge->count == 0)
	return TRUE;

    syslog(L_NOTICE, "tradindexed: repacking group %s, offset %d", group, delta);
    GROUPlock(gloc, INN_LOCK_WRITE);

    if (delta >= ge->base) delta = ge->base - 1;

    strcpy(bakgroup, group);
    strcat(bakgroup, "-BAK");
    strcpy(newgroup, group);
    strcat(newgroup, "-NEW"); 
    OV3getIDXfilename(group, oldidx);
    OV3getIDXfilename(newgroup, newidx);
    OV3getIDXfilename(bakgroup, bakidx);

    unlink(newidx);

    /* open and mmap old group index */
    if ((gh = OV3opengroup(group, FALSE)) == NULL) {
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	return FALSE;
    }
    if (!OV3mmapgroup(gh)) {
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	return FALSE;
    }

    if ((fd = open(newidx, O_RDWR | O_CREAT, 0660)) < 0) {
	syslog(L_ERROR, "tradindexed: could not open %s: %m", newidx);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	return FALSE;
    }

    if (xpwrite(fd, gh->indexmem, gh->indexlen, sizeof(INDEXENTRY) * delta) < 0) {
	syslog(L_ERROR, "tradindexed: packgroup cant write to %s: %m", newidx);
	close(fd);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	return FALSE;
    }	
    if (close(fd) < 0) {
	syslog(L_ERROR, "tradindexed: packgroup cant close %s: %m", newidx);
	OV3closegroup(gh, FALSE);
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	return FALSE;
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", newidx, oldidx);
	    break;
	}
	unlink(bakidx);
    } while (0);
    ge->indexinode = sb.st_ino;
    ge->base -= delta;
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    OV3closegroup(gh, FALSE);
    return TRUE;
}

bool tradindexed_expiregroup(char *group, int *lo, struct history *h) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    void                *handle;
    GROUPENTRY          *ge;
    GROUPLOC            gloc;
    char                *data;
    int                 len;
    TOKEN               token;
    ARTNUM              artnum;
    GROUPENTRY          newge;
    GROUPHANDLE         *newgh;
    ARTHANDLE           *ah;
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    char                bakdat[BIG_BUFFER], olddat[BIG_BUFFER], newdat[BIG_BUFFER];
    struct stat         sb;
    time_t		arrived, expires;

    if (group == NULL)
	return TRUE;
    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return FALSE;
    ge = &GROUPentries[gloc.recno];
    if (ge->count == 0) {
	if (lo)
	    *lo = ge->low;
	return TRUE;
    }
    
    strcpy(bakgroup, group);
    strcat(bakgroup, "-BAK");
    strcpy(newgroup, group);
    strcat(newgroup, "-NEW"); 
    OV3getIDXfilename(group, oldidx);
    OV3getIDXfilename(newgroup, newidx);
    OV3getIDXfilename(bakgroup, bakidx);
    OV3getDATfilename(group, olddat);
    OV3getDATfilename(newgroup, newdat);
    OV3getDATfilename(bakgroup, bakdat);

    unlink(newidx);
    unlink(newdat);

    GROUPlock(gloc, INN_LOCK_WRITE);
    if ((handle = tradindexed_opensearch(group, ge->low, ge->high)) == NULL) {
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	return FALSE;
    }
    if ((newgh = OV3opengroupfiles(newgroup)) == NULL) {
	GROUPlock(gloc, INN_LOCK_UNLOCK);
	tradindexed_closesearch(handle);
	return FALSE;
    }
    newge = *ge;
    newge.base = newge.low = newge.count = 0;
    while (ov3search(handle, &artnum, &data, &len, &token, &arrived, &expires)) {
	ah = NULL;
	if (!SMprobe(EXPENSIVESTAT, &token, NULL) || OVstatall) {
	    if ((ah = SMretrieve(token, RETR_STAT)) == NULL)
		continue;
	    SMfreearticle(ah);
	} else {
	    if (!OVhisthasmsgid(h, data))
		continue; 
	}
	if (innconf->groupbaseexpiry && OVgroupbasedexpire(token, group, data, len, arrived, expires))
	    continue;
	OV3addrec(&newge, newgh, artnum, token, data, len, arrived, expires);
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(olddat, bakdat) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", olddat, bakdat);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", newidx, oldidx);
	    break;
	}
	if (rename(newdat, olddat) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    syslog(L_ERROR, "tradindexed: could not rename %s -> %s", newdat, olddat);
	    break;
	}
	unlink(bakidx);
	unlink(bakdat);
    } while (0);
    if (newge.low == 0)
	/* no article for the group */
	newge.low = newge.high;
    *ge = newge;
    ge->indexinode = sb.st_ino;
    if (lo) {
	if (ge->count == 0)
	    /* lomark should be himark + 1, if no article for the group */
	    *lo = ge->low + 1;
	else
	    *lo = ge->low;
    }
    GROUPlock(gloc, INN_LOCK_UNLOCK);
    tradindexed_closesearch(handle);
    OV3closegroupfiles(newgh);
    return TRUE;
}

bool tradindexed_ctl(OVCTLTYPE type, void *val) {
    int *i;
    bool *boolval;
    OVSORTTYPE *sorttype;
    switch (type) {
    case OVSPACE:
	i = (int *)val;
	*i = -1;
	return TRUE;
    case OVSORT:
	sorttype = (OVSORTTYPE *)val;
	*sorttype = OVNEWSGROUP;
	return TRUE;
      case OVCUTOFFLOW:
	Cutofflow = *(bool *)val;
	return TRUE;
    case OVSTATICSEARCH:
	i = (int *)val;
	*i = FALSE;
	return TRUE;
    case OVCACHEKEEP:
    case OVCACHEFREE:
	boolval = (bool *)val;
	*boolval = FALSE;
	return TRUE;
    default:
	return FALSE;
    }
}

void tradindexed_close(void) {
    int                 i;
    CACHEENTRY          *ce, *next;
    struct stat         sb;

    for (i = 0; i < CACHETABLESIZE; i++) {
	for (ce = CACHEdata[i]; ce != NULL; ce = ce->next) {
	    OV3closegroupfiles(ce->gh);
	}
	for (ce = CACHEdata[i]; ce != NULL; ce = next) {
	    next = ce->next;
	    ce->next = NULL;
	    DISPOSE(ce);
	}
    }
    if (fstat(GROUPfd, &sb) < 0)
	return;
    close(GROUPfd);

    if (GROUPheader) {
	if (innconf->tradindexedmmap) {
	    if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
		syslog(L_FATAL, "tradindexed: could not munmap group.index in tradindexed_close: %m");
	    }
	} else {
	    DISPOSE(GROUPheader);
	}
    }
}
