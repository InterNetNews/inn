#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <sys/mman.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include "configdata.h"
#include "macros.h"
#include "clibrary.h"
#include "libinn.h"
#include "paths.h"
#include "storage.h"
#include "qio.h"
#include "ov3.h"

/* Data structure for specifying a location in the group index */
typedef struct {
    int                 recno;             /* Record number in group index */
} GROUPLOC;

#define GROUPHEADERHASHSIZE (16 * 1024)
#define GROUPHEADERMAGIC    (~(0xf1f0f33d))

typedef struct {
    int                 magic;
    GROUPLOC            hash[GROUPHEADERHASHSIZE];
    GROUPLOC            freelist;
} GROUPHEADER;

/* The group is matched based on the MD5 of the grouname. This may prove to
   be inadequate in the future, if so, the right thing to do is to is
   probably just to add a SHA1 hash into here also.  We get a really nice
   benefit from this being fixed length, we should try to keep it that way.
*/
typedef struct {
    HASH                hash;             /* MD5 hash of the group name, if */
    HASH                alias;            /* If not empty then this is the hash
					     of the group that this group is an
					     alias for */
    int                 high;             /* High water mark in group */
    int                 low;              /* Low water mark in group */
    int                 base;             /* Article number of the first entry
					     in the index */
    int                 count;            /* Number of articles in group */
    int                 flag;             /* Posting/Moderation Status */
    time_t              deleted;          /* When this was deleted, 0 otherwise */    
    ino_t               indexinode;       /* inode of the index file for the group,
					     used to detect when the file has been
					     recreated and swapped out. */
    GROUPLOC            next;             /* Next block in this chain */
} GROUPENTRY;

typedef struct {
    OFFSET_T           offset;
    int                length;
    TOKEN              token;
} INDEXENTRY;

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

/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char                *Header;
    int                 Length;
    BOOL                HasHeader;
} ARTOVERFIELD;

STATIC int              ARTfieldsize;
STATIC ARTOVERFIELD	*ARTfields;

#define CACHETABLESIZE 128
#define MAXCACHETIME (60*5)

STATIC int              GROUPfd;
STATIC GROUPHEADER      *GROUPheader = NULL;
STATIC GROUPENTRY       *GROUPentries = NULL;
STATIC int              GROUPcount = 0;
STATIC GROUPLOC         GROUPemptyloc = { -1 };

STATIC CACHEENTRY *CACHEdata[CACHETABLESIZE];
STATIC int OV3mode;
STATIC int OV3padamount = 128;
STATIC int CACHEentries = 0;
STATIC int CACHEhit = 0;
STATIC int CACHEmiss = 0;
STATIC int CACHEmaxentries = 128;

STATIC GROUPLOC GROUPnewnode(void);
STATIC BOOL GROUPremapifneeded(GROUPLOC loc);
STATIC void GROUPLOCclear(GROUPLOC *loc);
STATIC BOOL GROUPLOCempty(GROUPLOC loc);
STATIC BOOL GROUPlockhash(LOCKTYPE type);
STATIC BOOL GROUPlock(GROUPLOC gloc, LOCKTYPE type);
STATIC BOOL GROUPfilesize(int count);
STATIC BOOL GROUPexpand(int mode);
/* STATIC */ BOOL OV3packgroup(char *group, int delta);
STATIC BOOL OV3readschema(void);
STATIC char *OV3gen(char *name);

BOOL OV3open(int cachesize, int mode) {
    char                dirname[1024];
    char                *groupfn;
    struct stat         sb;
    int                 flag = 0;

    CACHEmaxentries = cachesize;
    OV3mode = mode;
    memset(&CACHEdata, '\0', sizeof(CACHEdata));
    
    strcpy(dirname, innconf->pathoverview);
    groupfn = NEW(char, strlen(dirname) + strlen("/group.index") + 1);
    strcpy(groupfn, dirname);
    strcat(groupfn, "/group.index");
    GROUPfd = open(groupfn, O_RDWR | O_CREAT, ARTFILE_MODE);
    if (GROUPfd < 0) {
	syslog(L_FATAL, "Could not create %s: %m", groupfn);
	DISPOSE(groupfn);
	return FALSE;
    }
    
    if (fstat(GROUPfd, &sb) < 0) {
	syslog(L_FATAL, "Could not fstat %s: %m", groupfn);
	DISPOSE(groupfn);
	return FALSE;
    }
    if (sb.st_size > sizeof(GROUPHEADER)) {
	if (mode & OV3_READ)
	    flag |= PROT_READ;
	if (mode & OV3_WRITE) {
	    /* 
	     * Note: below mapping of groupheader won't work unless we have 
	     * both PROT_READ and PROT_WRITE perms.
	     */
	    flag |= PROT_WRITE|PROT_READ;
	}
	GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
	if ((GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount), flag,
					       MAP_SHARED, GROUPfd, 0)) == (GROUPHEADER *) -1) {
	    syslog(L_FATAL, "Could not mmap %s in OV3open: %m", groupfn);
	    DISPOSE(groupfn);
	    return FALSE;
	}
	GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    } else {
	GROUPcount = 0;
	if (!GROUPexpand(mode))
	    return FALSE;
    }
    OV3readschema();
    
    DISPOSE(groupfn);

    return TRUE;
}

STATIC GROUPLOC GROUPfind(char *group) {
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

BOOL OV3groupstats(char *group, int *lo, int *hi, int *count, int *flag) {
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

BOOL OV3groupadd(char *group, char *flag) {
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
    GROUPlockhash(LOCK_WRITE);
    loc = GROUPnewnode();
    ge = &GROUPentries[loc.recno];
    ge->hash = grouphash;
    ge->deleted = ge->high = ge->low = ge->base = ge->count = 0;
    ge->flag = *flag;
    ge->next = GROUPheader->hash[i];
    GROUPheader->hash[i] = loc;
    GROUPlockhash(LOCK_UNLOCK);
    return TRUE;
}

STATIC BOOL GROUPfilesize(int count) {
    return (count * sizeof(GROUPENTRY)) + sizeof(GROUPHEADER);
}

/* Check if the given GROUPLOC refers to GROUPENTRY that we don't have mmap'ed,
** if so then see if the file has been grown by another writer and remmap
*/
STATIC BOOL GROUPremapifneeded(GROUPLOC loc) {
    struct stat         sb;
    
    if (loc.recno < GROUPcount)
	return TRUE;

    if (fstat(GROUPfd, &sb) < 0)
	return FALSE;

    if (GROUPfilesize(GROUPcount) >= sb.st_size)
	return TRUE;

    if (GROUPheader) {
	if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
	    syslog(L_FATAL, "Could not munmap group.index in GROUPremapifneeded: %m");
	    return FALSE;
	}
    }

    GROUPcount = (sb.st_size - sizeof(GROUPHEADER)) / sizeof(GROUPENTRY);
    GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
				     PROT_READ | PROT_WRITE, MAP_SHARED, GROUPfd, 0);
    if (GROUPheader == (GROUPHEADER *) -1) {
	syslog(L_FATAL, "Could not mmap group.index in GROUPremapifneeded: %m");
	return FALSE;
    }
    GROUPentries = (GROUPENTRY *)((char *)GROUPheader + sizeof(GROUPHEADER));
    return TRUE;
}

/* This function does not need to lock because it's callers are expected to do so */
STATIC BOOL GROUPexpand(int mode) {
    int                 i;
    int                 flag = 0;
    
    if (GROUPheader) {
	if (munmap((void *)GROUPheader, GROUPfilesize(GROUPcount)) < 0) {
	    syslog(L_FATAL, "Could not munmap group.index in GROUPexpand: %m");
	    return FALSE;
	}
    }
    GROUPcount += 1024;
    if (ftruncate(GROUPfd, GROUPfilesize(GROUPcount)) < 0) {
	syslog(L_FATAL, "Could not extend group.index: %m");
	return FALSE;
    }
    if (mode & OV3_READ)
	flag |= PROT_READ;
    if (mode & OV3_WRITE) {
	/* 
	 * Note: below check of magic won't work unless we have both PROT_READ
	 * and PROT_WRITE perms.
	 */
	flag |= PROT_WRITE|PROT_READ;
    }
    GROUPheader = (GROUPHEADER *)mmap(0, GROUPfilesize(GROUPcount),
				     flag, MAP_SHARED, GROUPfd, 0);
    if (GROUPheader == (GROUPHEADER *) -1) {
	syslog(L_FATAL, "Could not mmap group.index in GROUPexpand: %m");
	return FALSE;
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

STATIC GROUPLOC GROUPnewnode(void) {
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

BOOL OV3groupdel(char *group) {
    GROUPLOC            gloc;
    
    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return TRUE;

    GROUPentries[gloc.recno].deleted = time(NULL);
    HashClear(&GROUPentries[gloc.recno].hash);
    return TRUE;
}

STATIC void GROUPLOCclear(GROUPLOC *loc) {
    loc->recno = -1;
}

STATIC BOOL GROUPLOCempty(GROUPLOC loc) {
    return (loc.recno < 0);
}

STATIC BOOL GROUPlockhash(LOCKTYPE type) {
    return LockRange(GROUPfd, type, TRUE, 0, sizeof(GROUPHEADER));
}

STATIC BOOL GROUPlock(GROUPLOC gloc, LOCKTYPE type) {
    return LockRange(GROUPfd,
		     type,
		     TRUE,
		     sizeof(GROUPHEADER) + (sizeof(GROUPENTRY) * gloc.recno),
		     sizeof(GROUPENTRY));
}

STATIC BOOL OV3mmapgroup(GROUPHANDLE *gh) {
    struct stat         sb;
    
    if (fstat(gh->datafd, &sb) < 0) {
	syslog(L_ERROR, "OV3mmapgroup could not fstat data file for %s: %m", gh->group);
	return FALSE;
    }
    gh->datalen = sb.st_size;
    if (fstat(gh->indexfd, &sb) < 0) {
	syslog(L_ERROR, "OV3mmapgroup could not fstat index file for %s: %m", gh->group);
	return FALSE;
    }
    gh->indexlen = sb.st_size;
    if (!gh->datamem) {
	if ((gh->datamem = (char *)mmap(0, gh->datalen, PROT_READ, MAP_SHARED,
					gh->datafd, 0)) == (char *)-1) {
	    syslog(L_ERROR, "OV3 could not mmap data file for %s: %m", gh->group);
	    return FALSE;
	}
    }
    if (!gh->indexmem) {
	if ((gh->indexmem = (INDEXENTRY *)mmap(0, gh->indexlen, PROT_READ, MAP_SHARED, 
					       gh->indexfd, 0)) == (INDEXENTRY *)-1) {
	    syslog(L_ERROR, "OV3 could not mmap index file for  %s: %m", gh->group);
	    munmap(gh->datamem, gh->datalen);
	    return FALSE;
	}
    }
    return TRUE;
}

STATIC GROUPHANDLE *OV3opengroupfiles(char *group) {
    char                *sepgroup;
    char                *p;
    char                IDXpath[BIG_BUFFER];
    char                DATpath[BIG_BUFFER];
    char                **groupparts = NULL;
    GROUPHANDLE         *gh;
    int                 i, j;
    struct stat         sb;
    
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

    if (!MakeDirectory(IDXpath, TRUE)) {
	syslog(L_ERROR, "Could not create directory %s", IDXpath);
	return NULL;
    }
    sprintf(p, "%s.DAT", group);
    strcpy(DATpath, IDXpath);
    sprintf(p, "%s.IDX", group);

    gh = NEW(GROUPHANDLE, 1);
    memset(gh, '\0', sizeof(GROUPHANDLE));
    if ((gh->datafd = open(DATpath, O_RDWR| O_APPEND | O_CREAT, 0660)) < 0) {
	DISPOSE(gh);
	if (errno == ENOENT)
	    return NULL;
	syslog(L_ERROR, "OV3 could not open %s: %m", DATpath);
	return NULL;
    }
    if ((gh->indexfd = open(IDXpath, O_RDWR | O_CREAT, 0660)) < 0) {
	close(gh->datafd);
	DISPOSE(gh);
	syslog(L_ERROR, "OV3 could not open %s: %m", IDXpath);
	return NULL;
    }
    if (fstat(gh->indexfd, &sb) < 0) {
	DISPOSE(gh);
	syslog(L_ERROR, "OV3 could not fstat %s: %m", IDXpath);
	return FALSE;
    }
    gh->indexinode = sb.st_ino;
    gh->indexlen = gh->datalen = -1;
    gh->indexmem = NULL;
    gh->datamem = NULL;
    gh->group = COPY(group);
    return gh;
}

STATIC void OV3closegroupfiles(GROUPHANDLE *gh) {
    close(gh->indexfd);
    close(gh->datafd);
    if (gh->indexmem)
	munmap((void *)gh->indexmem, gh->indexlen);
    if (gh->datamem)
	munmap(gh->datamem, gh->datalen);
    if (gh->group)
	DISPOSE(gh->group);
    DISPOSE(gh);
}


void OV3cleancache(void) {
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
	syslog(L_NOTICE, "OV3 group cache is full, waiting 10 seconds");
	sleep(10);
    }
}


STATIC GROUPHANDLE *OV3opengroup(char *group) {
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
    if ((gh = OV3opengroupfiles(group)) == NULL)
	return NULL;
    gh->hash = hash;
    gh->gloc = gloc;
    if (OV3mode & OV3_WRITE) {
	/*
	 * Don't try to update ge->... unless we are in write mode, since
	 * otherwise we can't write to that mapped region. 
	 */
	ge->indexinode = gh->indexinode;
    }
    ce = NEW(CACHEENTRY, 1);
    memset(ce, '\0', sizeof(*ce));
    ce->gh = gh;
    ce->refcount++;
    ce->lastused = Now;
    
    /* Insert into the list */
    ce->next = CACHEdata[hashbucket];
    CACHEdata[hashbucket] = ce;
    CACHEentries++;
    return gh;
}

BOOL OV3closegroup(GROUPHANDLE *gh) {
    unsigned int        i;
    CACHEENTRY          *ce;

    memcpy(&i, &gh->hash, sizeof(i));
    i %= CACHETABLESIZE;
    for (ce = CACHEdata[i]; ce != NULL; ce = ce->next) {
	if (memcmp(&ce->gh->hash, &gh->hash, sizeof(HASH)) == 0) {
	    ce->refcount--;
	    break;
	}
    }
    return TRUE;
}

BOOL OV3addrec(GROUPENTRY *ge, GROUPHANDLE *gh, int artnum, TOKEN token, char *data, int len) {
    INDEXENTRY          ie;
    int                 base;
    
    if (ge->base == 0) {
	base = artnum > OV3padamount ? artnum - OV3padamount : 1;
    } else {
	base = ge->base;
	if (ge->base > artnum) {
	    syslog(L_ERROR, "Could not add %s:%d, base == %d", gh->group, artnum, ge->base);
	    return TRUE;
	}
    }
    memset(&ie, '\0', sizeof(ie));
    if (write(gh->datafd, data, len) != len) {
	syslog(L_ERROR, "Could not append overview record to %s: %m", gh->group);
	return TRUE;
    }
    if ((ie.offset = lseek(gh->datafd, 0, SEEK_CUR)) < 0) {
	syslog(L_ERROR, "Could not get offset of overview record in %s: %m", gh->group);
	return TRUE;
    }
    ie.length = len;
    ie.offset -= ie.length;
    ie.token = token;
    
    if (pwrite(gh->indexfd, &ie, sizeof(ie), (artnum - base) * sizeof(ie)) != sizeof(ie)) {
	syslog(L_ERROR, "Could not write index record for %s:%d", gh->group, artnum);
	return TRUE;
    }
    if ((ge->low <= 0) || (ge->low > artnum))
	ge->low = artnum;
    if ((ge->high <= 0) || (ge->high < artnum))
	ge->high = artnum;
    ge->count++;
    ge->base = base;
    if (ge->high - ge->low + 1 > ge->count)
	ge->count = ge->high - ge->low + 1;	
    return TRUE;
}

BOOL OV3add(TOKEN token, char *data, int len) {
    char                *next;
    static char         *xrefdata;
    char		*xrefstart;
    static int          datalen = 0;
    BOOL                found = FALSE;
    int                 xreflen;
    int                 i;
    char                *group;
    int                 artnum;
    GROUPHANDLE         *gh;
    GROUPENTRY		*ge;
    char                overdata[BIG_BUFFER];

    /*
     * find last Xref: in the overview line.  Note we need to find the *last*
     * Xref:, since there have been corrupted articles on Usenet with Xref:
     * fragments stuck in other header lines.  The last Xref: is guaranteed
     * to be from our server. 
     */
     
    for (next = data; ((len - (next - data)) > 6 ) && ((next = memchr(next, 'X', len - (next - data))) != NULL); ) {
	if (memcmp(next, "Xref: ", 6) == 0) {
	    found =  TRUE;
	    xrefstart = next;
	}
	next++;
    }
    
    if (!found)
	return FALSE;

    next = xrefstart;
    for (i = 0; (i < 2) && (next < (data + len)); i++) {
	if ((next = memchr(next, ' ', len - (next - data))) == NULL)
	    return FALSE;
	next++;
    }
    xreflen = len - (next - data);
    if (datalen == 0) {
	datalen = BIG_BUFFER;
	xrefdata = NEW(char, datalen);
    }
    if (xreflen > datalen) {
	datalen = xreflen;
	RENEW(xrefdata, char, datalen + 1);
    }
    memcpy(xrefdata, next, xreflen);
    xrefdata[xreflen] = '\0';
    for (group = xrefdata; group && *group; group = memchr(next, ' ', xreflen - (next - xrefdata))) {
	/* Parse the xref part into group name and article number */
	while (isspace((int)*group))
	    group++;
	if ((next = memchr(group, ':', xreflen - (group - xrefdata))) == NULL)
	    return FALSE;
	*next++ = '\0';
	artnum = atoi(next);
	if (artnum <= 0)
	    continue;

	if ((gh = OV3opengroup(group)) == NULL)
	    return FALSE;	
	sprintf(overdata, "%d\t", artnum);
	i = strlen(overdata);
	memcpy(overdata + i, data, len);
	i += len;
	memcpy(overdata + i, "\r\n", 2);
	i += 2;

	/* pack group if needed. */
	ge = &GROUPentries[gh->gloc.recno];
	if (ge->base > artnum) {
	    if (!OV3packgroup(group, OV3padamount + ge->base - artnum)) {
		OV3closegroup(gh);
		return FALSE;
	    }
	    /* sigh. need to close and reopen group after packing. */
	    OV3closegroup(gh);
	    if ((gh = OV3opengroup(group)) == NULL)
		return FALSE;
	}
	GROUPlock(gh->gloc, LOCK_WRITE);
	OV3addrec(ge, gh, artnum, token, overdata, i);
	GROUPlock(gh->gloc, LOCK_UNLOCK);
	OV3closegroup(gh);
    }        
    return TRUE;
}

void *OV3opensearch(char *group, int low, int high) {
    GROUPHANDLE         *gh;
    GROUPENTRY          *ge;
    ino_t               oldinode;
    int                 base;
    OV3SEARCH           *search;
    
    if ((gh = OV3opengroup(group)) == NULL)
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

    if (!OV3mmapgroup(gh)) {
	OV3closegroup(gh);
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

BOOL OV3search(void *handle, int *artnum, char **data, int *len, TOKEN *token) {
    OV3SEARCH           *search = (OV3SEARCH *)handle;
    INDEXENTRY           *ie;

    for (ie = search->gh->indexmem;
	 ((char *)&ie[search->cur] < (char *)search->gh->indexmem + search->gh->indexlen) &&
	     (search->cur <= search->limit) &&
	     (ie[search->cur].length == 0);
	 search->cur++);

    if (search->cur > search->limit)
	return FALSE;

    if ((char *)&ie[search->cur] >= (char *)search->gh->indexmem + search->gh->indexlen) {
	syslog(L_ERROR, "Truncated overview results for %s", search->group);
	return FALSE;
    }

    ie = &ie[search->cur];
    if (artnum)
	*artnum = search->base + search->cur;
    if (len)
	*len = ie->length;
    if (data)
	*data = search->gh->datamem + ie->offset;
    if (token)
	*token = ie->token;

    search->cur++;

    return TRUE;
}

void OV3closesearch(void *handle) {
    OV3SEARCH           *search = (OV3SEARCH *)handle;

    OV3closegroup(search->gh);
    DISPOSE(search->group);
    DISPOSE(search);
}

BOOL OV3getartinfo(char *group, int artnum, char **data, int *len, TOKEN *token) {
    void                *handle;
    BOOL                retval;
    if (!(handle = OV3opensearch(group, artnum, artnum)))
	return FALSE;
    retval = OV3search(handle, NULL, data, len, token);
    OV3closesearch(handle);
    return retval;
}
void OV3getdirpath(char *group, char *path) {
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

void OV3getIDXfilename(char *group, char *path) {
    char                *p;

    OV3getdirpath(group, path);
    p = path + strlen(path);
    sprintf(p, "%s.IDX", group);
}

void OV3getDATfilename(char *group, char *path) {
    char                *p;

    OV3getdirpath(group, path);
    p = path + strlen(path);
    sprintf(p, "%s.DAT", group);
}

/*
 * Shift group index file so it has lower value of base.
 */

/* STATIC */ BOOL
OV3packgroup(char *group, int delta) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    GROUPENTRY		*ge;
    GROUPLOC            gloc;
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    struct stat         sb;
    int			fd;
    int			numentries;
    GROUPHANDLE		*gh;
    OFFSET_T		nbytes;

    if (delta <= 0) return FALSE;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return FALSE;
    ge = &GROUPentries[gloc.recno];
    if (ge->count == 0)
	return TRUE;

    syslog(L_NOTICE, "OV3 repacking group %s, offset %d", group, delta);
    GROUPlock(gloc, LOCK_WRITE);

    if (delta > ge->base) delta = ge->base;

    strcpy(bakgroup, group);
    strcat(bakgroup, "-BAK");
    strcpy(newgroup, group);
    strcat(newgroup, "-NEW"); 
    OV3getIDXfilename(group, oldidx);
    OV3getIDXfilename(newgroup, newidx);
    OV3getIDXfilename(bakgroup, bakidx);

    unlink(newidx);

    /* open and mmap old group index */
    if ((gh = OV3opengroup(group)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    if (!OV3mmapgroup(gh)) {
	OV3closegroup(gh);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }

    if ((fd = open(newidx, O_RDWR | O_CREAT, 0660)) < 0) {
	syslog(L_ERROR, "OV3 could not open %s: %m", newidx);
	OV3closegroup(gh);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }

    /* write old index records to new file */
    numentries = ge->high - ge->low + 1;
    nbytes = numentries * sizeof(INDEXENTRY);
    if (pwrite(fd, &gh->indexmem[ge->low - ge->base] , nbytes,
	       sizeof(INDEXENTRY)*(ge->low - ge->base + delta)) != nbytes) {
	syslog(L_ERROR, "OV3 packgroup cant write to %s: %m", newidx);
	OV3closegroup(gh);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }	
    if (close(fd) < 0) {
	syslog(L_ERROR, "OV3 packgroup cant close %s: %m", newidx);
	OV3closegroup(gh);
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "Could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", newidx, oldidx);
	    break;
	}
	unlink(bakidx);
    } while (0);
    ge->indexinode = sb.st_ino;
    ge->base -= delta;
    GROUPlock(gloc, LOCK_UNLOCK);
    OV3closegroup(gh);
    return TRUE;
}

BOOL OV3expiregroup(char *group, int *lo) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    void                *handle;
    GROUPENTRY          *ge;
    GROUPLOC            gloc;
    char                *data;
    int                 len;
    TOKEN               token;
    int                 artnum;
    GROUPENTRY          newge;
    GROUPHANDLE         *newgh;
    ARTHANDLE           *ah;
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    char                bakdat[BIG_BUFFER], olddat[BIG_BUFFER], newdat[BIG_BUFFER];
    struct stat         sb;

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

    GROUPlock(gloc, LOCK_WRITE);
    if ((handle = OV3opensearch(group, ge->low, ge->high)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    if ((newgh = OV3opengroupfiles(newgroup)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	OV3closesearch(handle);
	return FALSE;
    }
    newge = *ge;
    newge.base = newge.count = 0;
    while (OV3search(handle, &artnum, &data, &len, &token)) {
	if ((ah = SMretrieve(token, RETR_STAT)) == NULL)
	    continue;
	SMfreearticle(ah);
#if 0
	if (p = strchr(data, '\t')) {
	    for (p--; (p >= data) && isdigit((int) *p); p--);
	    if (p >= data) {
		printf("bad article %s:%d\n", group, artnum);
		sprintf(overdata, "%d\t", artnum);
		i = strlen(overdata);
		p = overdata + i;
		memcpy(p, data, len - newlen - 2);
		p = overdata + len - 2;
		memcpy(p, "\r\n", 2);
		OV3addrec(&newge, newgh, artnum, token, overdata, len);
		continue;
	    }
	}
	if (atoi(data) != artnum) {
	    printf("misnumbered article %s:%d\n", group, artnum);
	    if ((p = strstr(data, "Xref: ")) == NULL) {
		syslog(L_ERROR, "Could not find Xref header in %s:%d", group, artnum);
		continue;
	    }
	    if ((p = strchr(p, ' ')) == NULL) {
		syslog(L_ERROR, "Could not find space after Xref header in %s:%d", group, artnum);
		continue;
	    }
	    if ((p = strstr(p, group)) == NULL) {
		syslog(L_ERROR, "Could not find group name in Xref header in %s:%d", group, artnum);
		continue;
	    }
	    p += strlen(group) + 1;
	    artnum = atoi(p);
	}
#endif
	OV3addrec(&newge, newgh, artnum, token, data, len);
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "Could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(olddat, bakdat) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", olddat, bakdat);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", newidx, oldidx);
	    break;
	}
	if (rename(newdat, olddat) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", newdat, olddat);
	    break;
	}
	unlink(bakidx);
	unlink(bakdat);
    } while (0);
    *ge = newge;
    ge->indexinode = sb.st_ino;
    GROUPlock(gloc, LOCK_UNLOCK);
    OV3closesearch(handle);
    OV3closegroupfiles(newgh);
    if (lo)
	*lo = ge->low;
    return TRUE;
}

BOOL OV3rebuilddatafromindex(char *group) {
    char                newgroup[BIG_BUFFER];
    char                bakgroup[BIG_BUFFER];
    char                *p;
    void                *handle;
    GROUPENTRY          *ge;
    GROUPLOC            gloc;
    char                *data;
    int                 len;
    TOKEN               token;
    int                 artnum;
    GROUPENTRY          newge;
    GROUPHANDLE         *newgh;
    char                *gendata;
    char                overdata[BIG_BUFFER];
    char                bakidx[BIG_BUFFER], oldidx[BIG_BUFFER], newidx[BIG_BUFFER];
    char                bakdat[BIG_BUFFER], olddat[BIG_BUFFER], newdat[BIG_BUFFER];
    struct stat         sb;
    int                 i;

    gloc = GROUPfind(group);
    if (GROUPLOCempty(gloc))
	return FALSE;
    ge = &GROUPentries[gloc.recno];
    
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

    GROUPlock(gloc, LOCK_WRITE);
    if ((handle = OV3opensearch(group, ge->low, ge->high)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	return FALSE;
    }
    if ((newgh = OV3opengroupfiles(newgroup)) == NULL) {
	GROUPlock(gloc, LOCK_UNLOCK);
	OV3closesearch(handle);
	return FALSE;
    }
    newge = *ge;
    newge.base = newge.low = newge.high = newge.count = 0;
    while (OV3search(handle, &artnum, &data, &len, &token)) {
	if ((gendata = OV3gen(TokenToText(token))) == NULL) {
	    syslog(L_ERROR, "Could not rebuild overview for %s:%d", group, artnum);
	    continue;
	}
	if ((p = strstr(gendata, "Xref: ")) == NULL) {
	    syslog(L_ERROR, "Could not find Xref header in %s:%d", group, artnum);
	    continue;
	}
	if ((p = strchr(p, ' ')) == NULL) {
	    syslog(L_ERROR, "Could not find space after Xref header in %s:%d", group, artnum);
	    continue;
	}
	if ((p = strstr(p, group)) == NULL) {
	    syslog(L_ERROR, "Could not find group name in Xref header in %s:%d", group, artnum);
	    continue;
	}
	p += strlen(group) + 1;
	artnum = atoi(p);
	sprintf(overdata, "%d\t", artnum);
	i = strlen(overdata);
	p = overdata + i;
	memcpy(p, gendata, strlen(gendata));
	p += strlen(gendata);
	memcpy(p, "\r\n", 2);
	len = p - overdata + 2;
	OV3addrec(&newge, newgh, artnum, token, overdata, len);
    }
    do {
	if (stat(newidx, &sb) < 0) {
	    unlink(newidx);
	    unlink(oldidx);
	    syslog(L_ERROR, "Could not stat %s", newidx);
	    break;
	}
	if (rename(oldidx, bakidx) < 0) {
	    unlink(newidx);
	    unlink(oldidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(olddat, bakdat) < 0) {
	    rename(bakidx, oldidx);
	    unlink(newidx);
	    unlink(oldidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(newidx, oldidx) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    unlink(oldidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	if (rename(newdat, olddat) < 0) {
	    rename(bakidx, oldidx);
	    rename(bakdat, olddat);
	    unlink(newidx);
	    unlink(oldidx);
	    syslog(L_ERROR, "Could not rename %s -> %s", oldidx, bakidx);
	    break;
	}
	unlink(bakidx);
	unlink(bakdat);
    } while (0);
    *ge = newge;
    ge->indexinode = sb.st_ino;
    GROUPlock(gloc, LOCK_UNLOCK);
    OV3closesearch(handle);
    OV3closegroupfiles(newgh);
    return TRUE;
}

void OV3close(void) {
    int                 i;
    CACHEENTRY          *ce, *next;

    for (i = 0; i < CACHETABLESIZE; i++) {
	for (ce = CACHEdata[i]; ce != NULL; ce = ce->next) {
	    OV3closegroupfiles(ce->gh);
	}
	for (ce = CACHEdata[i]; ce != NULL; ce = next) {
	    next = ce->next;
	    DISPOSE(ce);
	}
    }
}

/*
**  Read the overview schema.
*/
STATIC BOOL OV3readschema(void)
{
    FILE		        *F;
    char		        *p;
    ARTOVERFIELD	        *fp;
    int		                i;
    char			buff[SMBUF];
    
    /* Open file, count lines. */
    if ((F = fopen(cpcatpath(innconf->pathetc, _PATH_SCHEMA), "r")) == NULL) {
	syslog(L_FATAL, "Can't open %s, %m",
	       cpcatpath(innconf->pathetc, _PATH_SCHEMA));
	return FALSE;
    }
    for (i = 0; fgets(buff, sizeof buff, F) != NULL; i++)
	continue;
    (void)fseek(F, (OFFSET_T)0, SEEK_SET);
    ARTfields = NEW(ARTOVERFIELD, i + 1);
    
    /* Parse each field. */
    for (fp = ARTfields; fgets(buff, sizeof buff, F) != NULL; ) {
	/* Ignore blank and comment lines. */
	if ((p = strchr(buff, '\n')) != NULL)
	    *p = '\0';
	if ((p = strchr(buff, COMMENT_CHAR)) != NULL)
	    *p = '\0';
	if (buff[0] == '\0')
	    continue;
	if ((p = strchr(buff, ':')) != NULL) {
	    *p++ = '\0';
	    fp->HasHeader = EQ(p, "full");
	}
	else
	    fp->HasHeader = FALSE;
	fp->Header = COPY(buff);
	fp->Length = strlen(buff);
	fp++;
    }
    ARTfieldsize = fp - ARTfields;
    (void)fclose(F);
    return TRUE;
}


/*
**  Read an article and create an overview line without the trailing
**  newline.  Returns pointer to static space or NULL on error.
*/
STATIC char *OV3gen(char *name)
{
    STATIC ARTOVERFIELD		*Headers;
    STATIC BUFFER		B;
    ARTOVERFIELD	        *fp;
    ARTOVERFIELD	 	*hp;
    ARTOVERFIELD		*lasthp = 0;
    QIOSTATE			*qp;
    char			*colon;
    char			*line;
    char			*p;
    int		        	i;
    int		        	size;
    int		        	ov_size;
    long			lines;
    struct stat			Sb;
    long			t;
    char			value[10];
    
    /* Open article. */
    if ((qp = QIOopen(name)) == NULL)
	return NULL;
    if ((p = strrchr(name, '/')) != NULL)
	name = p + 1;
    
    /* Set up place to store headers. */
    if (Headers == NULL) {
	Headers = NEW(ARTOVERFIELD, ARTfieldsize);
	for (hp = Headers, i = ARTfieldsize; --i >= 0; hp++)
	    hp->Length = 0;
    } else {
	/* This disposes from the previous call.  This simplifies
	   handling later on.  We trade off this readable code
	   for the problem that nothing DISPOSEs() the last
	   caller's use.  mibsoft 8/22/97
	*/
	for (hp = Headers, i = ARTfieldsize; --i >= 0; hp++) {
	    if (hp->Length) {
		DISPOSE(hp->Header);
		hp->Header = 0;
	    }
	    hp->Length = 0;
	}
    }
    for (hp = Headers, i = ARTfieldsize; --i >= 0; hp++)
	hp->HasHeader = FALSE;
    
    for ( ; ; ) {
	/* Read next line. */
	if ((line = QIOread(qp)) == NULL) {
	    if (QIOtoolong(qp))
		continue;
	    /* Error or EOF (in headers!?); shouldn't happen. */
	    QIOclose(qp);
	    return NULL;
	}
	
	/* End of headers? */
	if (*line == '\0')
	    break;
	
	/* See if we want this header. */
	fp = ARTfields;
	for (hp = Headers, i = ARTfieldsize; --i >= 0; hp++, fp++) {
	    colon = &line[fp->Length];
	    if (*colon != ':')
		continue;
	    *colon = '\0';
	    if (!caseEQ(line, fp->Header)) {
		*colon = ':';
		continue;
	    }
	    *colon = ':';
	    if (fp->HasHeader)
		p = line;
	    else
		/* Skip colon and whitespace, store value. */
		for (p = colon; *++p && ISWHITE(*p); )
		    continue;
	    size = strlen(p);
	    if (!size) { /* Ignore empty headers 11/5/97 due to pmb1@york.ac.uk */
	        i = -1; /* Abort */
	        lasthp = 0;
	        break;
	    }
	    
            hp->Length = size;
            hp->Header = NEW(char, hp->Length + 1);
	    (void)strcpy(hp->Header, p);
	    for (p = hp->Header; *p; p++)
		if (*p == '\t' || *p == '\n' || *p == '\r')
		    *p = ' ';
	    hp->HasHeader = TRUE;
            lasthp = hp;
            break ;             /* the first one is used */
	}
        /* handle multi-line headers -- kondou@uxd.fc.nec.co.jp */
        if (i < 0) {
            if (lasthp && ISWHITE(*line)) {
                lasthp->Length += strlen(line);
                RENEW(lasthp->Header, char, lasthp->Length + 1);
                for (p = line; *p; p++)
                    if (*p == '\t' || *p == '\n' || *p == '\r')
                        *p = ' ';
                strcat(lasthp->Header, line);
            } else {
                lasthp = 0 ;
            }
        }
    }
    
    /* Read body of article, just to get lines. */
    for (lines = 0; ; lines++)
	if ((p = QIOread(qp)) == NULL) {
	    if (QIOtoolong(qp))
		continue;
	    if (QIOerror(qp)) {
		QIOclose(qp);
		return NULL;
	    }
	    break;
	}
    
    /* Calculate total size, fix hardwired headers. */
    ov_size = ARTfieldsize + 2;
    for (hp = Headers, fp = ARTfields, i = ARTfieldsize; --i >= 0; hp++, fp++) {
	if (caseEQ(fp->Header, "Bytes") || caseEQ(fp->Header, "Lines")) {
	    if (fp->Header[0] == 'B' || fp->Header[0] == 'b')
		t = fstat(QIOfileno(qp), &Sb) >= 0 ? (long)Sb.st_size : 0L;
	    else
		t = lines;
	    
	    (void)sprintf(value, "%ld", t);
	    size = strlen(value);
	    if (hp->Length == 0) {
		hp->Length = size;
		hp->Header = NEW(char, hp->Length + 1);
	    }
	    else if (hp->Length < size) {
		hp->Length = size;
		RENEW(hp->Header, char, hp->Length + 1);
	    }
	    (void)strcpy(hp->Header, value);
	    hp->HasHeader = TRUE;
	}
	if (hp->HasHeader)
	    ov_size += strlen(hp->Header);
    }
    
    /* Get space. */
    if (B.Size == 0) {
	B.Size = ov_size;
	B.Data = NEW(char, B.Size + 1);
    }
    else if (B.Size < ov_size) {
	B.Size = ov_size;
	RENEW(B.Data, char, B.Size + 1);
    }
    
    /* Glue all the fields together. */
    p = B.Data;
    for (hp = Headers, i = ARTfieldsize; --i >= 0; hp++) {
	if (hp->HasHeader)
	    p += strlen(strcpy(p, hp->Header));
	*p++ = '\t';
    }
    *(p - 1) = '\0';
    
    QIOclose(qp);
    return B.Data;
}

#ifdef _TEST_

int main(int argc, char **argv) {
    QIOSTATE            *qp;
    char                *line;
    char                *overview;
    char                **activeline = NULL;
    char                **histline = NULL;
    int                 linenum = 0;
    char                c;
    int                 val = 1;
    char                history[BIG_BUFFER] = "";
    BOOL                InitGroups = TRUE;
    int                  addtime, gentime;
    struct timeval      t1, t2;
    
    while ((c = getopt(argc, argv, "h:g")) != EOF) {
	switch (c) {
	case 'h':
	    strcpy(history, optarg);
	    break;
	case 'g':
	    InitGroups = FALSE;
	    break;
	default:
	}
    }
    
    if (ReadInnConf() < 0) exit(1);

    if (SMsetup(SM_PREOPEN, (void *)&val) && !SMinit()) {
	fprintf(stderr, "cant initialize storage method, %s",SMerrorstr);
	exit(1);
    }
    
    if (!OV3open(4096, OV3_WRITE)) {
	fprintf(stderr, "Could not open OV3 database\n");
	exit(1);
    }

    if (InitGroups) {
	/* Add all the groups in active to the groupdb */
	if ((qp = QIOopen(cpcatpath(innconf->pathdb, _PATH_ACTIVE))) == NULL) {
	    fprintf(stderr, "Could not open active file\n");
	    exit(2);
	}
	
	while ((line = QIOread(qp))) {
	    linenum++;
	    if (argify(line, &activeline) != 4) {
		freeargify(&activeline);
		fprintf(stderr, "Could not parse line %d of active file\n", linenum);
		exit(3);
	    }
	    if (!OV3groupadd(activeline[0], activeline[3])) {
		fprintf(stderr, "Could not add '%s %s' to groups.db\n", activeline[0],
			activeline[3]);
	    }
	    freeargify(&activeline);
	}
	QIOclose(qp);
    }

    if (history[0] == '\0')
	strcpy(history, cpcatpath(innconf->pathdb, _PATH_HISTORY));
    if ((qp = QIOopen(history)) == NULL) {
	fprintf(stderr, "Could not open history file\n");
	exit(4);
    }

    for (linenum = 1; (line = QIOread(qp)) != NULL; linenum++) {
	if (argify(line, &histline) != 3) {
	    freeargify(&histline);
	} else {
	    gettimeofday(&t1, NULL);
	    if ((overview = OV3gen(histline[2])) == NULL) {
		fprintf(stderr, "Could not generate overview for %s\n", histline[2]);
		freeargify(&histline);
		continue;
	    }
	    gettimeofday (&t2, NULL);	    
	    gentime += ((t2.tv_sec - t1.tv_sec) * 1000) + ((t2.tv_usec - t1.tv_usec) / 1000);
	    t1 = t2;
	    OV3add(TextToToken(histline[2]), overview, strlen(overview));
	    gettimeofday (&t2, NULL);	    
	    addtime += ((t2.tv_sec - t1.tv_sec) * 1000) + ((t2.tv_usec - t1.tv_usec) / 1000);
	    freeargify(&histline);
	}
    }
    
    QIOclose(qp);
    
    OV3close();
    printf("hit: %d miss: %d gen: %d  add %d\n", CACHEhit, CACHEmiss,
	   gentime, addtime);
    return 0;
}

#endif /* _TEST_ */
