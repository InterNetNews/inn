#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <dirent.h>
#include <ctype.h>
#include <clibrary.h>
#include <errno.h>
#include <limits.h>
#include <logging.h>
#include <macros.h>
#include <configdata.h>
#include <libinn.h>
#include <methods.h>

#include <configdata.h>
#include <interface.h>
#include "cnfs.h"
#include "cnfs-private.h"
#include "paths.h"

typedef struct {
    /**** Stuff to be cleaned up when we're done with the article */
    char		*base;		/* Base of mmap()ed art */
    int			len;		/* Length of article (and thus
					   mmap()ed art */
    /**** Used for "next" iterator */
    CYCBUFF		*cycbuff;	/* Cyclic buffer info structure */
    CYCBUFF_OFF_T	offset;		/* offset of start of article */
    int			index;		/* cycbuff's index into cycbufftab */
    int			rolledover;	/* true if we've rolled over to 
					   beginning of the cycbuff */
} PRIV_CNFS;

char LocalLogName[] = "CNFS-sm";
CYCBUFF		cycbufftab[MAX_CYCBUFFS];
int		cycbufftab_free = 0;
METACYCBUFF 	metacycbufftab[MAX_METACYCBUFFS];
int		metacycbufftab_free = 0;
CNFSEXPIRERULES	metaexprulestab[MAX_METACYCBUFF_RULES];
int		metaexprulestab_free = 0;

int		__CNFS_Write_Allowed = 0;	/* XXX an ugly hack */
int		__CNFS_Cancel_Allowed = 0;	/* XXX an ugly hack */

#define	MAX_DESCRIPTORS	34
struct {
    int fd;     /* File descriptor */
    int inuse;  /* Flag: true if this fd is "open", i.e. in use */
} fdpool[MAX_CYCBUFFS][MAX_DESCRIPTORS];
int fdpooluninitialized = 1;

static TOKEN CNFSMakeToken(char *cycbuffname, CYCBUFF_OFF_T offset,
		       INT32_T cycnum, STORAGECLASS class) {
    TOKEN               token;
    INT32_T		int32;

    memset(&token, '\0', sizeof(token));
    /*
    ** XXX We'll assume that TOKENSIZE is 16 bytes and that we divvy it
    ** up as: 8 bytes for cycbuffname, 4 bytes for offset, 4 bytes
    ** for cycnum.  See also: CNFSBreakToken() for hard-coded constants.
    */
    if (strlen(cycbuffname) > 8) {
	syslog(L_FATAL, "cnfs: cycbuffname '%s' is longer than 8 bytes",
	       cycbuffname);
	SMseterror(SMERR_INTERNAL, "cycbuffname longer than 8 bytes");
	return token;
    }
    token.type = TOKEN_CNFS;
    token.class = class;
    memcpy(token.token, cycbuffname, strlen(cycbuffname));
    int32 = htonl(offset / CNFS_BLOCKSIZE);
    memcpy(&token.token[8], &int32, sizeof(int32));
    int32 = htonl(cycnum);
    memcpy(&token.token[12], &int32, sizeof(int32));
    return token;
}

/*
** NOTE: We assume that cycbuffname is 9 bytes long.
*/

static BOOL CNFSBreakToken(TOKEN token, char *cycbuffname,
			   CYCBUFF_OFF_T *offset, INT32_T *cycnum) {
    INT32_T	int32;

    if (cycbuffname == NULL || offset == NULL || cycnum == NULL) {
	syslog(L_FATAL, "cnfs: BreakToken: invalid argument",
	       cycbuffname);
	SMseterror(SMERR_INTERNAL, "BreakToken: invalid argument");
	return FALSE;
    }
    memcpy(cycbuffname, token.token, 8);
    *(cycbuffname + 8) = '\0';		/* Just to be paranoid */
    memcpy(&int32, &token.token[8], sizeof(int32));
    *offset = ntohl(int32) * CNFS_BLOCKSIZE;
    memcpy(&int32, &token.token[12], sizeof(int32));
    *cycnum = ntohl(int32);
    return TRUE;
}

BOOL cnfs_init(void) {
    static	initialized = 0;

    if (initialized)
	return TRUE;
    initialized = 1;
    if (STORAGE_TOKEN_LENGTH < 16) {
	syslog(L_FATAL, "timehash: token length is less than 16 bytes");
	SMseterror(SMERR_TOKENSHORT, NULL);
	return FALSE;
    }

    CNFSread_config();
    CNFSpost_config_debug();
#ifdef	CNFS_DEBUG
    CNFSinit_disks(1);
#else	/* CNFS_DEBUG */
    CNFSinit_disks(0);
#endif	/* CNFS_DEBUG */
    CNFSpost_init_debug();

    if (__CNFS_Write_Allowed) {
	syslog(L_NOTICE, "CNFS storage manager initialized for read/write");
    }
    return TRUE;
}

TOKEN cnfs_store(const ARTHANDLE article, STORAGECLASS class) {
    TOKEN               token;
    char		*p;
    int			newsgroups = 1;
    CYCBUFF		*cycbuff = NULL;
    METACYCBUFF		*metacycbuff = NULL;
    int			i;
    static char		buf[1024], *bufp = buf;
    int			chars = 0;
    char		*artcycbuffname;
    CYCBUFF_OFF_T	artoffset, middle;
    INT32_T		artcyclenum;
    CNFSARTHEADER	cah;
    struct iovec	iov[2];
    int			tonextblock;
    int			fd;

    if (! __CNFS_Write_Allowed) {
	SMseterror(SMERR_INTERNAL, "cnfs_store: Process not authorized to write article");
	syslog(LOG_CRIT, "cnfs_store: Process not authorized to write article");
	token.type = TOKEN_EMPTY;
	return token;
    }
    /*
    ** Since one of our selection criteria is the number of newsgroups
    ** the article is posted to, we need to figure that number out.
    ** In 1.5.1, we could quickly look in the ARTDATA structure for the
    ** answer.  We have to parse the header to find it now.
    */
    if ((p = strstr(article.data, "Newsgroups:")) != NULL) {
	while (p != NULL && !isspace(*p))
	    p++;		/* Advance past "Newsgroups:" */
	while (isspace(*p))
	    p++;		/* Advance past whitespace */
	while (!isspace(*p)) {
	    if (*p == ',')
		newsgroups++;
	    if (chars < 1000) {
		*bufp++ = *p;	/* Copy newsgroup to 'buf' for comparison */
		chars++;
	    }
	    p++;
	}
	*bufp = '\0';		/* Terminate to make wildmat() happy */
    }
    for (i = 0; i < metaexprulestab_free; i++) {
	if (metaexprulestab[i].pattern[0] == '~' ){
	    if (metaexprulestab[i].pattern[1] == '>' ){
		if (article.len >= atoi(metaexprulestab[i].pattern + 2)){
		    metacycbuff = metaexprulestab[i].dest;
		    break;
		}
	    } else if (metaexprulestab[i].pattern[1] == '<' ){
		if (article.len < atoi(metaexprulestab[i].pattern + 2)){
		    metacycbuff = metaexprulestab[i].dest;
		    break;
		}
	    } else if (metaexprulestab[i].pattern[1] == 'G' ){
		if (newsgroups > atoi(metaexprulestab[i].pattern + 2)) {
		    metacycbuff = metaexprulestab[i].dest;
		    break;
		}
	    } else {
		syslog(LOG_ERR, "%s: bogus metaexprulestab[%d].pattern: %s",
		       i, metaexprulestab[i].pattern);
		break;
	    }
	} else {
	    if (wildmat(buf, metaexprulestab[i].pattern)) {
		metacycbuff = metaexprulestab[i].dest;
		break;
	    }
	}
    }
    if (metacycbuff == NULL) {
	SMseterror(SMERR_INTERNAL, "no rules match");
	syslog(LOG_ERR, "%s: no matches for group '%s'",
	       LocalLogName, buf);
	token.type = TOKEN_EMPTY;
	return token;
    }

    /*
    ** We'll only deal with interleaved metacycbuffs
    ** for now.  Sequential ones can come in a "future release" and/or 
    ** as an exercise to the reader.
    */
    if (metacycbuff->type == METACYCBUFF_SEQ) {
	SMseterror(SMERR_INTERNAL, "Sequential CNFS method not supported");
	syslog(LOG_ERR, "%s: cnfs write attempt for size %d group %s failed: sequential metacycbuffs (%s) not supported",
	       LocalLogName, article.len, buf, metacycbuff->name);
	token.type = TOKEN_EMPTY;
	return token;
    }

    cycbuff = metacycbuff->members[metacycbuff->memb_next];  
    /* Article too big? */
    if (cycbuff->free + article.len > cycbuff->len - CNFS_BLOCKSIZE - 1) {
	cycbuff->free = cycbuff->minartoffset;
	cycbuff->cyclenum++;
	CNFSflushhead(cycbuff);		/* Flush, just for giggles */
	syslog(LOG_ERR, "%s: cycbuff %s rollover to cycle 0x%x... remain calm",
	       LocalLogName, cycbuff->name, cycbuff->cyclenum);
    }
    /* Ah, at least we know all three important data */
    artcycbuffname = cycbuff->name;
    artoffset = cycbuff->free;
    artcyclenum = cycbuff->cyclenum;

    memset(&cah, 0, sizeof(cah));
    cah.zottf = CNFS_ZOTTF;
    cah.size = htonl(article.len);
    /* Drat!  We need to locate the "Message-ID" header now */
    if ((p = strstr(article.data, "Message-ID:")) == NULL) {
	SMseterror(SMERR_INTERNAL, "cnfs_store(): no Message-ID header found");
	syslog(L_ERROR, "cnfs_store(): no Message-ID header found");
	token.type = TOKEN_EMPTY;
	return token;
    }
    while (p != NULL && !isspace(*p))
	p++;		/* Advance past "Message-ID:" */
    while (isspace(*p))
	p++;		/* Advance past whitespace */
    bufp = cah.m_id;
    chars = 0;
    while (!isspace(*p)) {
	if (chars < 64) {
	    *bufp++ = *p;	/* Copy Message-ID to cah.m_id */
	    chars++;
	}
	p++;
    }
    *bufp = '\0';		/* Terminate! */

    if ((fd = CNFSGetWriteFd(cycbuff, NULL, artoffset)) < 0) {
	/* SMseterror() already done */
	token.type = TOKEN_EMPTY;
	return token;
    }
    iov[0].iov_base = (caddr_t) &cah;
    iov[0].iov_len = sizeof(cah);
    iov[1].iov_base = article.data;
    iov[1].iov_len = article.len;
    if (xwritev(fd, iov, 2) < 0) {
	SMseterror(SMERR_INTERNAL, "cnfs_store() xwritev() failed");
	syslog(L_ERROR,
	       "cnfs_store() xwritev() failed for '%s' offset 0x%s: %m",
	       artcycbuffname, CNFSofft2hex(artoffset, 0));
	token.type = TOKEN_EMPTY;
	return token;
    }
    CNFSPutWriteFd(cycbuff, NULL, fd);

    /* Now that the article is written, advance the free pointer & flush */
    cycbuff->free += (CYCBUFF_OFF_T) article.len + sizeof(cah);
    tonextblock = CNFS_BLOCKSIZE - (cycbuff->free & (CNFS_BLOCKSIZE - 1));
    cycbuff->free += (CYCBUFF_OFF_T) tonextblock;
    /*
    ** If cycbuff->free > cycbuff->len, don't worry.  The next cnfs_store()
    ** will detect the situation & wrap around correctly.
    */
    metacycbuff->memb_next = (metacycbuff->memb_next + 1) % metacycbuff->count;
    if (++metacycbuff->write_count % METACYCBUFF_UPDATE == 0) {
	for (i = 0; i < metacycbuff->count; i++) {
	    CNFSflushhead(metacycbuff->members[i]);
	}
    }
    if (metacycbuff->write_count % 1000000 == 0) {
	syslog(LOG_ERR,
	       "cnfs_store: metacycbuff %s just wrote its %ld'th article",
	       metacycbuff->name, metacycbuff->write_count);
    }
    CNFSUsedBlock(cycbuff, artoffset, TRUE, TRUE);
    for (middle = artoffset + CNFS_BLOCKSIZE; middle < cycbuff->free;
	 middle += CNFS_BLOCKSIZE) {
	CNFSUsedBlock(cycbuff, middle, TRUE, FALSE);
    }
    return CNFSMakeToken(artcycbuffname, artoffset, artcyclenum, class);
}

ARTHANDLE *cnfs_retrieve(const TOKEN token, RETRTYPE amount) {
    char		cycbuffname[9];
    CYCBUFF_OFF_T	offset;
    INT32_T		cycnum;
    CYCBUFF		*cycbuff;
    ARTHANDLE   	*art;
    int			fd;
    CNFSARTHEADER	*cah;
    PRIV_CNFS		*private;
    char		*p;
    static long		pagesize = 0;
    long		pagefudge;
    CYCBUFF_OFF_T	mmapoffset;
    int			ret;
    
    if (innconf == NULL) {
	if ((ret = ReadInnConf(_PATH_CONFIG)) < 0) {
	    SMseterror(SMERR_INTERNAL, "ReadInnConf() failed");
	    syslog(L_ERROR, "ReadInnConf() failed, returned %d", ret);
	    return NULL;
	}
    }
    if (pagesize == 0) {
#ifdef	XXX
	if ((pagesize = sysconf(_SC_PAGESIZE)) < 0) {
	    SMseterror(SMERR_INTERNAL, "sysconf(_SC_PAGESIZE) failed");
	    syslog(L_ERROR, "sysconf(_SC_PAGESIZE) failed: %m");
	    return NULL;
	}
#else	/* XXX */
	pagesize = 16384;	/* XXX Need comprehensive, portable solution */
#endif	/* XXX */
    }

    if (token.type != TOKEN_CNFS) {
	SMseterror(SMERR_INTERNAL, NULL);
	return NULL;
    }
    if (! CNFSBreakToken(token, cycbuffname, &offset, &cycnum)) {
	/* SMseterror() should have already been called */
	return NULL;
    }
    if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	syslog(L_ERROR, "cnfs_retrieve: token %s: bogus cycbuff name: %s:0x%s:%ld",
	       token, cycbuffname, CNFSofft2hex(offset, 0), cycnum);
	return NULL;
    }

    if ((fd = CNFSGetReadFd(cycbuff, NULL, offset, cycnum, FALSE)) < 0) {
	return NULL;
    }
    if ((art = NEW(ARTHANDLE, 1)) == NULL) {
	SMseterror(SMERR_INTERNAL, "malloc failed");
	return NULL;
    }
    art->type = TOKEN_CNFS;
    if (amount == RETR_STAT) {
	art->data = NULL;
	art->len = 0;
	art->private = NULL;
	CNFSPutReadFd(cycbuff, NULL, fd);
	return art;
    }
    /*
    ** Because we don't know the length of the article (yet), we'll
    ** just mmap() a chunk of memory which is guaranteed to be larger
    ** than the largest article can be.
    ** XXX Because the max article size can be changed, we could get into hot
    ** XXX water here.  So, to be safe, we double MAX_ART_SIZE and add enough
    ** XXX extra for the pagesize fudge factor and CNFSARTHEADER structure.
    */
    private = NEW(PRIV_CNFS, 1);
    art->private = (void *)private;
    private->len = 2 * innconf->maxartsize + pagesize + 200;
    private->cycbuff = cycbuff;
    private->offset = offset;
    private->index = cycbuff->index;
    private->rolledover = 0;
    pagefudge = offset % pagesize;
    mmapoffset = offset - pagefudge;
    if ((private->base = mmap((MMAP_PTR)0, private->len, PROT_READ, MAP__ARG,
			      fd, mmapoffset)) == (MMAP_PTR) -1) {
        SMseterror(SMERR_UNDEFINED, "mmap() failed");
        syslog(L_ERROR, "CNFS-sm: could not mmap() token %s %s:0x%s:%ld: %m",
	        token, cycbuffname, CNFSofft2hex(offset, 0), cycnum);
	CNFSPutReadFd(cycbuff, NULL, fd);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
	
    }
    cah = (CNFSARTHEADER *) (private->base + pagefudge);
    CNFSPutReadFd(cycbuff, NULL, fd);
    if (cah->zottf != the_true_zottf) {
	/* Must've just been cancelled/overwritten on us! */
	munmap(private->base, private->len);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
    }

    if (amount == RETR_ALL) {
	art->data = private->base + pagefudge + sizeof(CNFSARTHEADER);
	art->len = cah->size;
	return art;
    }
    if ((p = SMFindBody(private->base + pagefudge + sizeof(CNFSARTHEADER),
			art->len)) == NULL) {
        SMseterror(SMERR_NOBODY, NULL);
	munmap(private->base, private->len);
        DISPOSE(art->private);
        DISPOSE(art);
        return NULL;
    }
    if (amount == RETR_HEAD) {
	art->data = private->base + pagefudge + sizeof(CNFSARTHEADER);
        art->len = p - private->base + pagefudge + sizeof(CNFSARTHEADER);
        return art;
    }
    if (amount == RETR_BODY) {
        art->data = p + 4;
        art->len = art->len - (private->base + pagefudge +
			       sizeof(CNFSARTHEADER) - p - 4);
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    munmap(private->base, private->len);
    DISPOSE(art->private);
    DISPOSE(art);
    return NULL;
}

void cnfs_freearticle(ARTHANDLE *article) {
    PRIV_CNFS	*private;

    if (!article)
	return;
    
    if (article->private) {
	private = (PRIV_CNFS *)article->private;
#ifdef	MADV_DONTNEED
	madvise(private->base, private->len, MADV_DONTNEED);
#endif	/* MADV_DONTNEED */
	munmap(private->base, private->len);
	DISPOSE(private);
    }
    DISPOSE(article);
}

BOOL cnfs_cancel(TOKEN token) {
#ifdef	XXX
    int                 time;
    int                 seqnum;
    char                *path;
    int                 result;

    BreakToken(token, &time, &seqnum);
    path = MakePath(time, seqnum, token.class);
    result = unlink(path);
    DISPOSE(path);
    if (result < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
	return FALSE;
    }
#endif	/* XXX */
    return TRUE;
}

ARTHANDLE *cnfs_next(const ARTHANDLE *article, RETRTYPE amount) {
#ifdef	XXX
    PRIV_TIMEHASH       priv;
    PRIV_TIMEHASH       *newpriv;
#endif	/* XXX */
    char                *path;
    struct dirent       *de;
    ARTHANDLE           *art;

#ifdef	XXX
    path = NEW(char, strlen(_PATH_SPOOL) + 32);
    if (article == NULL) {
	priv.top = NULL;
	priv.sec = NULL;
	priv.artdir = NULL;
	priv.topde = NULL;
	priv.secde = NULL;
    } else {
	priv = *(PRIV_TIMEHASH *)article->private;
    }

    while (!priv.artdir || ((de = FindDir(priv.artdir, FIND_ART)) == NULL)) {
	if (priv.artdir) {
	    closedir(priv.artdir);
	    priv.artdir = NULL;
	}
	while (!priv.sec || ((priv.secde = FindDir(priv.sec, FIND_DIR)) == NULL)) {
	    if (priv.sec) {
		closedir(priv.sec);
		priv.sec = NULL;
	    }
	    if (priv.top && ((priv.topde = FindDir(priv.top, FIND_DIR)) != NULL)) {
		sprintf(path, "%s/time", _PATH_SPOOL);
		if ((priv.top = opendir(path)) == NULL) {
		    SMseterror(SMERR_UNDEFINED, NULL);
		    DISPOSE(path);
		    return NULL;
		}
		if ((priv.topde = FindDir(priv.top, FIND_DIR)) == NULL) {
		    SMseterror(SMERR_UNDEFINED, NULL);
		    closedir(priv.top);
		    DISPOSE(path);
		    return NULL;
		}
	    }
	    sprintf(path, "%s/time/%s", _PATH_SPOOL, priv.topde->d_name);
	    if ((priv.sec = opendir(path)) == NULL)
		continue;
	}
	sprintf(path, "%s/time/%s/%s", _PATH_SPOOL, priv.topde->d_name, priv.secde->d_name);
	if ((priv.artdir = opendir(path)) == NULL)
	    continue;
    }

    art = OpenArticle(path, amount);
    if (art) {
	newpriv = (PRIV_TIMEHASH *)art->private;
	newpriv->top = priv.top;
	newpriv->sec = priv.sec;
	newpriv->artdir = priv.artdir;
	newpriv->topde = priv.topde;
	newpriv->secde = priv.secde;
    }
    DISPOSE(path);
#endif	/* XXX */
    return art;
}

void cnfs_shutdown(void) {
    CNFSflushallheads();
    CNFSmunmapbitfields();
}

/*
** CNFSread_config() -- Read the cnfs partition/file configuration file.
**
** Oh, for the want of Perl!  My parser probably shows that I don't use
** C all that often anymore....
*/

long	the_true_zottf = 0;		/* Globally-accessed via CNFS_ZOTTF */
int  	wearebigendian = 0;

int
CNFSread_config(void)
{
#define	MAX_CTAB_SIZE \
    (MAX_CYCBUFFS + MAX_METACYCBUFFS + MAX_METACYCBUFF_RULES + 3)
	char		*config, *from, *to, *ctab[MAX_CTAB_SIZE];
    struct stat	statbuf;
    int		ctab_free = 0;	/* Index to next free slot in ctab */
    int		ctab_i;
							   
    if (the_true_zottf == 0) {
	union {
	    long l;
	    char c[sizeof(long)];
	} u;
	u.l = 1;
	if (u.c[sizeof(long) - 1] == 1) {
	    the_true_zottf = 0x00001234;	/* Big endian */
	    wearebigendian = 1;
	} else if (u.c[0] == 1) {
	    the_true_zottf = 0x34120000;
	    wearebigendian = 0;
	} else {
	    syslog(LOG_CRIT, "%s cannot determine endian'ness, exiting!",
		   "CNFSread_config");
	    exit(111);
	}
    }

    if ((config = ReadInFile(_PATH_CYCBUFFCONFIG, &statbuf)) == NULL) {
	syslog(LOG_CRIT, "%s cannot read %s", LocalLogName, _PATH_CYCBUFFCONFIG, NULL);
	exit(1);
    }
    for (from = to = config; *from; ) {
	if (ctab_free > MAX_CTAB_SIZE - 1) {
	    syslog(LOG_CRIT, "%s maximum size (%d) of cycbuff config file reached",
		   LocalLogName, MAX_CTAB_SIZE, NULL);
	    exit(1);
	}
	if (*from == '#') {	/* Comment line? */
	    while (*from && *from != '\n')
		from++;				/* Skip past it */
	    from++;
	    continue;				/* Back to top of loop */
	}
	if (*from == '\n') {	/* End or just a blank line? */
	    from++;
	    continue;				/* Back to top of loop */
	}
	/* If we're here, we've got the beginning of a real entry */
	ctab[ctab_free++] = to = from;
	while (1) {
	    if (*from && *from == '\\' && *(from + 1) == '\n') {
		from += 2;		/* Skip past backslash+newline */
		while (*from && isspace(*from))
		    from++;
		continue;
	    }
	    if (*from && *from != '\n')
		*to++ = *from++;
	    if (*from == '\n') {
		*to++ = '\0';
		from++;
		break;
	    }
	    if (! *from)
		break;
	}
    }

#ifdef	CNFS_DEBUG
    {
	int i = 0;

	puts("\nString version of ctab");
	while (i < ctab_free) {
	    puts(ctab[i++]);
	}
	puts("");
    }
#endif	/* CNFS_DEBUG */

    for (ctab_i = 0; ctab_i < ctab_free; ctab_i++) {
	if (strncmp(ctab[ctab_i], "cycbuff:", 8) == 0) {
	    CNFSparse_part_line(ctab[ctab_i] + 8);
	} else if (strncmp(ctab[ctab_i], "metacycbuff:", 12) == 0) {
	    CNFSparse_metapart_line(ctab[ctab_i] + 12);
	} else if (strncmp(ctab[ctab_i], "groups:", 7) == 0) {
	    CNFSparse_groups_line(ctab[ctab_i] + 7);
	} else {
	    syslog(LOG_ERR, "%s: Bogus metacycbuff config line '%s' ignored",
		   LocalLogName, ctab[ctab_i]);
	}
    }
    if (cycbufftab_free == 0) {
	syslog(LOG_CRIT, "%s: zero cycbuffs defined, exiting!", LocalLogName);
	exit(1);
    }
    if (metacycbufftab_free == 0) {
	syslog(LOG_CRIT, "%s: zero metacycbuffs defined, exiting!", LocalLogName);
	exit(1);
    }
    if (metaexprulestab_free == 0) {
	syslog(LOG_CRIT, "%s: zero meta expiration rules defined, exiting!",
	       LocalLogName);
	exit(1);
    }
    return 1;
}

/*
** CNFSinit_disks -- Finish initializing cycbufftab and metacycbufftab.
**	Called by "innd" only -- we open (and keep) a read/write
**	file descriptor for each CYCBUFF.
**
** Calling this function repeatedly shouldn't cause any harm
** speed-wise or bug-wise, as long as the caller is accessing the
** CYCBUFFs _read-only_.  If innd calls this function repeatedly,
** bad things will happen.
*/

int AllowV2Initialization = 0;	/* Do not modify unless you know what you're doing */

void
CNFSinit_disks(int verbose)
{
  char		*myLocalLogName = "CNFSinit_disks", buf[64];
  CYCBUFFEXTERN	rpx;
  int		i, fd, bytes;
  CYCBUFF_OFF_T	tmpo;
  char		bufoff[64], bufcycle[64];
  int		just_initialized;
  int		protections;

  /*
  ** Discover the state of our cycbuffs.  If any of them are in icky shape,
  ** duck shamelessly & exit.
  */

  for (i = 0; i < cycbufftab_free; i++) {
    if (strcmp(cycbufftab[i].path, "/dev/null") == 0)
      continue;
    if (cycbufftab[i].fdrd < 0) {
	if ((fd = open(cycbufftab[i].path, O_RDONLY)) < 0) {
	    syslog(LOG_CRIT,
		   "%s: ERROR opening '%s' O_RDONLY, aborting! : %m",
		   LocalLogName, cycbufftab[i].path);
	    exit(1);
	} else {
	    cycbufftab[i].fdrd = fd;
	}
    }
    if (cycbufftab[i].fdrdwr < 0 &&
	(__CNFS_Write_Allowed || __CNFS_Cancel_Allowed)) {
	if ((fd = open(cycbufftab[i].path, O_RDWR)) < 0) {
	    syslog(LOG_CRIT,
		   "%s: ERROR opening '%s' O_RDWR, aborting! : %m",
		   LocalLogName, cycbufftab[i].path);
	    exit(1);
	} else {
	    cycbufftab[i].fdrdwr = fd;
	}
    }
    if (verbose) 
	fprintf(stderr, "%s: cycbuff %s has read/write cycbuff[i].fdrd %d\n",
	       myLocalLogName, cycbufftab[i].name, cycbufftab[i].fdrd);
    if ((tmpo = CNFSseek(cycbufftab[i].fdrd, (CYCBUFF_OFF_T) 0, SEEK_SET)) < 0) {
	syslog(LOG_CRIT, "%s: pre-magic read lseek failed: %m", LocalLogName);
	exit(1);
    }
    if ((bytes = read(cycbufftab[i].fdrd, &rpx, sizeof(CYCBUFFEXTERN))) != sizeof(rpx)) {
	syslog(LOG_CRIT, "%s: read magic failed: %m", LocalLogName);
	exit(1);
    }
    if (CNFSseek(cycbufftab[i].fdrd, tmpo, SEEK_SET) != tmpo) {
	syslog(LOG_CRIT, "%s: post-magic read lseek to 0x%s failed: %m",
	       LocalLogName, CNFSofft2hex(tmpo, 0));
	exit(1);
    }
    just_initialized = 0;

    /*
    ** Much of this checking from previous revisions is (probably) bogus
    ** & buggy & particularly icky & unupdated.  Use at your own risk.  :-)
    */

    if (strncmp(rpx.magic, CNFS_MAGICV1, strlen(CNFS_MAGICV1)) == 0) {
	syslog(LOG_NOTICE, "%s: cycbuff %s is magic version 1",
		   LocalLogName, cycbufftab[i].name);
	syslog(LOG_CRIT, "%s: exiting! (backward compatibility removed)",
	       LocalLogName);
	exit(1);
    } else if (strncmp(rpx.magic, CNFS_MAGICV2, strlen(CNFS_MAGICV2)) == 0) {
	if (! AllowV2Initialization) {
	    syslog(LOG_NOTICE, "%s: cycbuff %s is magic version 2",
		   LocalLogName, cycbufftab[i].name);
	    syslog(LOG_NOTICE,
		   "%s: cycbuff %s: convert to version 3 using 'v2-to-v3'",
		   LocalLogName, cycbufftab[i].name);
	    syslog(LOG_CRIT, "%s: exiting! (backward compatibility removed)",
		   LocalLogName);
	    exit(1);
	}
	cycbufftab[i].magicver = 2; /* Only if AllowV2Initialization is TRUE */
	if (verbose)
	    fprintf(stderr,
		    "%s: cycbuff %s is magic version 2 (CONVERSION UNDERWAY?!?)\n",
		   LocalLogName, cycbufftab[i].name);
	if (strncmp(rpx.name, cycbufftab[i].name, CNFSNASIZ) != 0) {
	    syslog(LOG_CRIT, "%s: Mismatch 1: read %s for cycbuff %s", LocalLogName,
		   rpx.name, cycbufftab[i].name);
	    exit(1);
	}
	if (strncmp(rpx.path, cycbufftab[i].path, CNFSPASIZ) != 0) {
	    syslog(LOG_CRIT, "%s: Mismatch 2: read %s for cycbuff %s", LocalLogName,
		   rpx.path, cycbufftab[i].path);
	    exit(1);
	}
	strncpy(buf, rpx.lena, CNFSLASIZ);
	buf[CNFSLASIZ] = '\0';
	tmpo = CNFShex2offt(buf);
	if (tmpo != cycbufftab[i].len) {
	    syslog(LOG_CRIT, "%s: Mismatch: read 0x%s length for cycbuff %s",
		   LocalLogName, CNFSofft2hex(tmpo, 0), cycbufftab[i].path);
	    exit(1);
	}
	strncpy(buf, rpx.freea, CNFSLASIZ);
	cycbufftab[i].free = CNFShex2offt(buf);
	strncpy(buf, rpx.updateda, CNFSLASIZ);
	cycbufftab[i].updated = CNFShex2offt(buf);
	strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
	cycbufftab[i].cyclenum = CNFShex2offt(buf);
	cycbufftab[i].articlepending = 0;
    } else if (strncmp(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3)) == 0) {
	cycbufftab[i].magicver = 3;
	if (verbose)
	    fprintf(stderr, "%s: cycbuff %s is magic version 3\n",
		   LocalLogName, cycbufftab[i].name);
	if (strncmp(rpx.name, cycbufftab[i].name, CNFSNASIZ) != 0) {
	    syslog(LOG_CRIT, "%s: Mismatch 3: read %s for cycbuff %s", LocalLogName,
		   rpx.name, cycbufftab[i].name);
	    exit(1);
	}
	if (strncmp(rpx.path, cycbufftab[i].path, CNFSPASIZ) != 0) {
	    syslog(LOG_CRIT, "%s: Path mismatch: read %s for cycbuff %s",
		   LocalLogName, rpx.path, cycbufftab[i].path);
	    syslog(LOG_CRIT,
		   "%s: If this is not OK, kill this process within 60 seconds",
		   LocalLogName);
	    sleep(60);
	}
	strncpy(buf, rpx.lena, CNFSLASIZ);
	buf[CNFSLASIZ] = '\0';
	tmpo = CNFShex2offt(buf);
	if (tmpo != cycbufftab[i].len) {
	    syslog(LOG_CRIT, "%s: Mismatch: read 0x%s length for cycbuff %s",
		   LocalLogName, CNFSofft2hex(tmpo, 0), cycbufftab[i].path);
	    exit(1);
	}
	cycbufftab[i].articlepending = 0;
	if (! CNFSReadFreeAndCycle(&cycbufftab[i])) {
	    abort();	/* XXX Probably the unsociable & rude thing to do */
	}
    } else {
	if (__CNFS_Write_Allowed) {
	    syslog(LOG_NOTICE,
		   "%s: No magic cookie found for cycbuff %s, initializing",
		   LocalLogName, cycbufftab[i].name);
	} else {
	    syslog(LOG_ERR,
		   "%s: No magic cookie found for cycbuff %s, NOT permitted to initialize",
		   LocalLogName, cycbufftab[i].name);
	}
	cycbufftab[i].magicver = 3;
	cycbufftab[i].free = cycbufftab[i].minartoffset;
	cycbufftab[i].updated = 0;
	cycbufftab[i].cyclenum = 1;
	CNFSflushhead(&cycbufftab[i]);
	just_initialized = 1;
    }
    strcpy(buf, ctime(&cycbufftab[i].updated));
    buf[strlen(buf) - 1] = '\0';	/* Zap newline */
    if (verbose && ! just_initialized) {
	strcpy(bufoff, CNFSofft2hex(cycbufftab[i].free, 0));
	strcpy(bufcycle, CNFSofft2hex(cycbufftab[i].cyclenum, 0));
	fprintf(stderr, "%s: Cycbuff %s last updated %s\n", LocalLogName,
		cycbufftab[i].name, buf);
	fprintf(stderr,
		"%s: Cycbuff %s will resume at offset 0x%s cycle 0x%s\n",
		LocalLogName, /* Danger, Will Robinson! */
		cycbufftab[i].name, bufoff, bufcycle);
    }
    errno = 0;
    protections = PROT_READ;
    protections |= (__CNFS_Cancel_Allowed) ? PROT_WRITE : 0;
    fd= (protections & PROT_WRITE) ? cycbufftab[i].fdrdwr : cycbufftab[i].fdrd;
    if ((cycbufftab[i].bitfield =
	 mmap((caddr_t) 0, cycbufftab[i].minartoffset, protections,
	      MAP_SHARED, fd, (off_t) 0)) == MAP_FAILED || errno != 0) {
	syslog(LOG_ERR,
	       "%s: CNFSinitdisks: mmap for %s offset %d len %d failed: %m",
	       LocalLogName, cycbufftab[i].path, 0, cycbufftab[i].minartoffset);
	exit(98);
    }
  }

  /*
  ** OK.  Time to figure out the state of our metacycbuffs...
  **
  */
  for (i = 0; i < metacycbufftab_free; i++) {
    if (metacycbufftab[i].type == METACYCBUFF_INTER) {
      /* This is easy ... just choose one */
      metacycbufftab[i].memb_next = 0;
    } else if (metacycbufftab[i].type == METACYCBUFF_SEQ) {
	syslog(LOG_CRIT,
	       "%s: Unsupported metacycbuff type ... should never happen",
	       LocalLogName);
	exit(99);
    } else {
      syslog(LOG_CRIT, "%s: how did metacycbuff %s get type %d???  Exiting!",
	     LocalLogName, metacycbufftab[i].name, metacycbufftab[i].type);
      exit(1);
    }
    metacycbufftab[i].write_count = 0;		/* Let's not forget this */
  }
}

void
CNFSparse_part_line(char *l)
{
  char		*p;
  int		i = cycbufftab_free;	/* Because it's too long to type */
  struct stat	sb;
  CYCBUFF_OFF_T	len, minartoffset;
  int		tonextblock;

  /* Symbolic cnfs partition name */
  if ((p = strchr(l, ':')) == NULL) {
    syslog(LOG_ERR, "%s: bad cycbuff name in line '%s', ignoring",
	   LocalLogName, l);
    return;
  }
  *p = '\0';
  if (strlen(l) > 8) {
    syslog(LOG_ERR, "%s: bad cycbuff name too long in line '%s', ignoring",
	   LocalLogName, l);
    return;
  }
  strncpy(cycbufftab[i].name, l, 8);
  cycbufftab[i].name[8] = '\0';
  cycbufftab[i].index = i;
  l = ++p;

  /* Path to cnfs partition */
  if ((p = strchr(l, ':')) == NULL) {
    syslog(LOG_ERR, "%s: bad pathname in line '%s', ignoring",
	   LocalLogName, l);
    return;
  }
  *p = '\0';
  strncpy(cycbufftab[i].path, l, 63);
  cycbufftab[i].path[63] = '\0';
  if (stat(cycbufftab[i].path, &sb) < 0) {
    syslog(LOG_ERR, "%s: file '%s' does not exist, ignoring '%s' cycbuff",
	   LocalLogName, cycbufftab[i].path, cycbufftab[i].name);
    return;
  }
  l = ++p;

  /* Length/size of symbolic partition */
  len = atoi(l) * 1024;		/* This value in KB in decimal */
  if (len < 0) {
    syslog(LOG_ERR, "%s: bad length '%ld' for '%s' cycbuff, ignoring",
	   /* Danger... */ LocalLogName, len, cycbufftab[i].name);
    return;
  }
  cycbufftab[i].len = len;
  l = ++p;

  cycbufftab[i].fdrd = -1;
  cycbufftab[i].fdrdwr = -1;
  cycbufftab[i].fdrdwr_inuse = 0;
  /*
  ** The minimum article offset will be the size of the bitfield itself,
  ** len / (blocksize * 8), plus however many additional blocks the CYCBUFF
  ** external header occupies ... then round up to the next block.
  */
  minartoffset =
      cycbufftab[i].len / (CNFS_BLOCKSIZE * 8) + CNFS_BEFOREBITF;
  tonextblock = CNFS_BLOCKSIZE - (minartoffset & (CNFS_BLOCKSIZE - 1));
  cycbufftab[i].minartoffset = minartoffset + tonextblock;

  cycbufftab_free++;
  /* Done! */
}

void
CNFSparse_metapart_line(char *l)
{
  char		*p, type, *cycbuff;
  int		i = metacycbufftab_free; /* Because it's too long to type */
  int		rpi;
  char		mstring[16 * MAX_META_MEMBERS + 1];
  int		m_free = 0;
  CYCBUFF	*rp;

  /* Symbolic metacycbuff name */
  if ((p = strchr(l, ':')) == NULL) {
    syslog(LOG_ERR, "%s: bad partition name in line '%s', ignoring",
	   LocalLogName, l);
    return;
  }
  *p = '\0';
  strncpy(metacycbufftab[i].name, l, 15);
  metacycbufftab[i].name[15] = '\0';
  l = ++p;

  /* Metacycbuff type: interleaved or sequential */
  if ((p = strchr(l, ':')) == NULL) {
    syslog(LOG_ERR, "%s: bad metacycbuff type in line '%s', ignoring",
	   LocalLogName, l);
    return;
  }
  *p = '\0';
  type = *l;
  l = ++p;
  if (type == 'I') {
    metacycbufftab[i].type = METACYCBUFF_INTER;
  } else if (type == 'S') {
    metacycbufftab[i].type = METACYCBUFF_SEQ;
  } else {
    syslog(LOG_ERR, "%s: bogus metacycbuff type '%c' for '%s', ignoring",
	   LocalLogName, type, metacycbufftab[i].name);
    return;
  }

  /* Cycbuff list */
  while ((p = strchr(l, ',')) != NULL) {
    if (m_free == MAX_META_MEMBERS) {
      syslog(LOG_ERR,
	     "%s: max # of cycbuff members (%d) for metacycbuff '%s' reached",
	     LocalLogName, MAX_META_MEMBERS, metacycbufftab[i].name);
      break;
    }
    *p = '\0';
    cycbuff = l;
    l = ++p;
    if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
      syslog(LOG_ERR, "%s: bogus cycbuff '%s' (metacycbuff '%s'), skipping",
	     LocalLogName, cycbuff, metacycbufftab[i].name);
      continue;
    }
    metacycbufftab[i].members[m_free] = rp;
    metacycbufftab[i].members[m_free]->mymeta = &metacycbufftab[i];
    m_free++;
  }
  /* Gotta deal with the last cycbuff on the list */
  cycbuff = l;
  if ((rp = CNFSgetcycbuffbyname(cycbuff)) == NULL) {
    syslog(LOG_ERR, "%s: bogus cycbuff '%s' (metacycbuff '%s'), skipping",
	   LocalLogName, cycbuff, metacycbufftab[i].name);
  } else {
    metacycbufftab[i].members[m_free] = rp;
    metacycbufftab[i].members[m_free]->mymeta = &metacycbufftab[i];
    m_free++;
  }
  
  if (m_free == 0) {
    syslog(LOG_ERR, "%s: no cycbuffs assigned to cycbuff '%s', skipping",
	   LocalLogName, metacycbufftab[i].name);
    return;
  }
  metacycbufftab[i].count = m_free;

  /* DONE! */
  strcpy(mstring, metacycbufftab[i].members[0]->name);
  for (rpi = 1; rpi < m_free; rpi++) {
    strcat(mstring, ",");
    strcat(mstring, metacycbufftab[i].members[rpi]->name);
  }
  metacycbufftab_free++;
}

void
CNFSparse_groups_line(char *l)
{
  char		*p;
  int		i = metaexprulestab_free; /* Because it's too long to type */
  METACYCBUFF	*mrp;

  /* Pattern */
  if ((p = strchr(l, ':')) == NULL) {
    syslog(LOG_ERR, "%s: bad expiration pattern in line '%s', ignoring",
	   LocalLogName, l);
    return;
  }
  *p = '\0';
  metaexprulestab[i].pattern = strdup(l);
  l = ++p;

  if ((mrp = CNFSgetmetacycbuffbyname(l)) == NULL) {
    syslog(LOG_ERR, "%s: metacycbuff '%s' undefined, skipping pattern '%s'",
	   LocalLogName, l, metaexprulestab[i].pattern);
    return;
  }
  metaexprulestab[i].dest = mrp;

  /* DONE! */
  metaexprulestab_free++;
}

int
CNFSgetcycbuffindexbyname(char *name)
{
  int	i;

if (name == NULL)
    return -1;
  for (i = 0; i < cycbufftab_free; i++)
    if (strcmp(name, cycbufftab[i].name) == 0) 
      return i;
  return -1;
}

METACYCBUFF *
CNFSgetmetacycbuffbyname(char *name)
{
  int	i;

if (name == NULL)
    return NULL;
  for (i = 0; i < metacycbufftab_free; i++)
    if (strcmp(name, metacycbufftab[i].name) == 0) 
      return &metacycbufftab[i];
  return NULL;
}

void
CNFSpost_config_debug(void)
{
#ifdef	CNFS_DEBUG
#endif	/* CNFS_DEBUG */
}

void
CNFSpost_init_debug(void)
{
#if	0
#ifdef	CNFS_DEBUG
    ARTHANDLE	arth1, *arth1b;
    TOKEN	token1;
    int		i;

    arth1.data = "From: fritchie@mr.net\nNewsgroups: mn.test\n\
Message-ID: <foo@bar.com>\nDate: now\n\nThis is a test.\n\n-Scott\n.\n";
    arth1.len = strlen(arth1.data);
    token1 = SMstore(arth1);
    arth1b = SMretrieve(token1, RETR_ALL);
    SMfreearticle(arth1b);
    i = 0;
#endif	/* CNFS_DEBUG */
#endif	/* 0 */
}

void
CNFSflushhead(CYCBUFF *cycbuff)
{
  int			fd;
  int			b;
  static CYCBUFFEXTERN	rpx;

  if (__CNFS_Write_Allowed) {
      memset(&rpx, 0, sizeof(CYCBUFFEXTERN));
      if ((fd = open(cycbuff->path, O_WRONLY)) < 0) {
	  syslog(LOG_ERR, "%s: CNFSflushhead: open failed: %m", LocalLogName);
	  return;
      }
      if (cycbuff->magicver == 1) {
	  /*
	  ** No longer used ... don't rely on this code to do anything useful.
	  */
	  if ((b = write(fd, cycbuff, sizeof(CYCBUFF))) != sizeof(CYCBUFF)) {
	      syslog(LOG_ERR, "%s: CNFSflushhead: write failed: %m", LocalLogName);
	  }
      } else if (cycbuff->magicver == 2) {
	  /*
	  ** No longer used ... don't rely on this code to do anything useful.
	  */
	  cycbuff->updated = time(NULL);
	  strncpy(rpx.magic, CNFS_MAGICV2, strlen(CNFS_MAGICV2));
	  strncpy(rpx.name, cycbuff->name, CNFSNASIZ);
	  strncpy(rpx.path, cycbuff->path, CNFSPASIZ);
	  /* Don't use sprintf() directly ... the terminating '\0' causes grief */
	  strncpy(rpx.lena, CNFSofft2hex(cycbuff->len, 1), CNFSLASIZ);
	  strncpy(rpx.freea, CNFSofft2hex(cycbuff->free, 1), CNFSLASIZ);
	  strncpy(rpx.cyclenuma, CNFSofft2hex(cycbuff->cyclenum, 1), CNFSLASIZ);
	  strncpy(rpx.updateda, CNFSofft2hex(cycbuff->updated, 1), CNFSLASIZ);
	  if ((b = write(fd, &rpx, sizeof(CYCBUFFEXTERN))) !=
	      sizeof(CYCBUFFEXTERN)) {
	      syslog(LOG_ERR, "%s: CNFSflushhead: write failed: %m", LocalLogName);
	  }
      } else if (cycbuff->magicver == 3) {
	  cycbuff->updated = time(NULL);
	  strncpy(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3));
	  strncpy(rpx.name, cycbuff->name, CNFSNASIZ);
	  strncpy(rpx.path, cycbuff->path, CNFSPASIZ);
	  /* Don't use sprintf() directly ... the terminating '\0' causes grief */
	  strncpy(rpx.lena, CNFSofft2hex(cycbuff->len, 1), CNFSLASIZ);
	  strncpy(rpx.freea, CNFSofft2hex(cycbuff->free, 1), CNFSLASIZ);
	  strncpy(rpx.cyclenuma, CNFSofft2hex(cycbuff->cyclenum, 1), CNFSLASIZ);
	  strncpy(rpx.updateda, CNFSofft2hex(cycbuff->updated, 1), CNFSLASIZ);
	  if ((b = write(fd, &rpx, sizeof(CYCBUFFEXTERN))) !=
	      sizeof(CYCBUFFEXTERN)) {
	      syslog(LOG_ERR, "%s: CNFSflushhead: write failed: %m", LocalLogName);
	  }
      } else {
	  syslog(LOG_CRIT, "%s: CNFSflushhead: bogus magicver for %s: %d",
		 LocalLogName, cycbuff->name, cycbuff->magicver);
      }
      close(fd);
  } /* __CNFS_Write_Allowed */
}

void
CNFSflushallheads(void)
{
  int	 i;

  for (i = 0; i < cycbufftab_free; i++) {
    syslog(LOG_NOTICE, "CNFSflushallheads: flushing %s", cycbufftab[i].name);
    CNFSflushhead(&cycbufftab[i]);
  }
}

static char hextbl[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
			'a', 'b', 'c', 'd', 'e', 'f'};

/*
** CNFSofft2hex -- Given an argument of type CYCBUFF_OFF_T, return
**	a static ASCII string representing its value in hexadecimal.
**
**	If "leadingzeros" is true, the number returned will have leading 0's.
*/

char *
CNFSofft2hex(CYCBUFF_OFF_T offset, int leadingzeros)
{
    static char	buf[24];
    char	*p;

    if (sizeof(CYCBUFF_OFF_T) <= 4) {
	sprintf(buf, (leadingzeros) ? "%016lx" : "%lx", offset);
    } else { 
	int	i;

	for (i = 0; i < CNFSLASIZ; i++)
	    buf[i] = '0';	/* Pad with zeros to start */
	for (i = CNFSLASIZ - 1; i >= 0; i--) {
	    buf[i] = hextbl[offset & 0xf];
	    offset >>= 4;
	}
    }
    if (! leadingzeros) {
	for (p = buf; *p == '0'; p++)
	    ;
	if (*p != '\0')
		return p;
	else
		return p - 1;	/* We converted a "0" and then bypassed all
				   the zeros */
    } else 
	return buf;
}

/*
** CNFShex2offt -- Given an ASCII string containing a hexadecimal representation
**	of a CYCBUFF_OFF_T, return a CYCBUFF_OFF_T.
*/

CYCBUFF_OFF_T
CNFShex2offt(char *hex)
{
    if (sizeof(CYCBUFF_OFF_T) <= 4) {
	CYCBUFF_OFF_T	rpofft;
	/* I'm lazy */
	sscanf(hex, "%lx", &rpofft);
	return rpofft;
    } else {
	char		diff;
	CYCBUFF_OFF_T	n = (CYCBUFF_OFF_T) 0;

	for (; *hex != '\0'; hex++) {
	    if (*hex >= '0' && *hex <= '9')
		diff = '0';
	    else if (*hex >= 'a' && *hex <= 'f')
		diff = 'a' - 10;
	    else if (*hex >= 'A' && *hex <= 'F')
		diff = 'A' - 10;
	    else {
		/*
		** We used to have a syslog() message here, but the case
		** where we land here because of a ":" happens, er, often.
		*/
		break;
	    }
	    n += (*hex - diff);
	    if (isalnum(*(hex + 1)))
		n <<= 4;
	}
	return n;
    }
}

/*
**	Bit arithmetic by brute force.
**
**	XXXYYYXXX WARNING: the code below is not endian-neutral!
*/

int			uninitialized = 1;
typedef unsigned long	ULONG;
ULONG			bitlong, onarray[64], offarray[64], mask;

int
CNFSUsedBlock(CYCBUFF *cycbuff, CYCBUFF_OFF_T offset,
	      BOOL set_operation, BOOL setbitvalue)
{
    CYCBUFF_OFF_T	blocknum;
    CYCBUFF_OFF_T	longoffset;
    int			bitoffset;	/* From the 'left' side of the long */
    static int		uninitialized = 1;
    static int		longsize = sizeof(long);
    int	i;
    ULONG		on, off;

    if (set_operation && ! __CNFS_Cancel_Allowed) {
	SMseterror(SMERR_INTERNAL, "CNFSGetWriteFd: Process not authorized to write article");
	syslog(LOG_CRIT, "CNFSGetWriteFd: Process not authorized to write article");
	return -1;
    }
    if (uninitialized) {
	on = 1;
	off = on;
	off ^= ULONG_MAX;
	for (i = (longsize * 8) - 1; i >= 0; i--) {
	    onarray[i] = on;
	    offarray[i] = off;
	    on <<= 1;
	    off = on;
	    off ^= ULONG_MAX;
	}
	uninitialized = 0;
    }

    if (cycbuff == NULL)
	return 0;
    /* We allow bit-setting under minartoffset, but it better be FALSE */
    if ((offset < cycbuff->minartoffset && setbitvalue) ||
	offset > cycbuff->len) {
	char	bufoff[64], bufmin[64], bufmax[64];
	SMseterror(SMERR_INTERNAL, NULL);
	strcpy(bufoff, CNFSofft2hex(offset, 0));
	strcpy(bufmin, CNFSofft2hex(cycbuff->minartoffset, 0));
	strcpy(bufmax, CNFSofft2hex(cycbuff->len, 0));
	syslog(L_ERROR,
	       "%s: CNFSUsedBlock: invalid offset %s, min = %s, max = %s",
	       LocalLogName, bufoff, bufmin, bufmax);
	return 0;
    }
    if (offset % CNFS_BLOCKSIZE != 0) {
	SMseterror(SMERR_INTERNAL, NULL);
	syslog(LOG_ERR,
	       "%s: CNFSsetusedbitbyrp: offset %s not on %d-byte block boundary",
	       LocalLogName, CNFSofft2hex(offset, 0), CNFS_BLOCKSIZE);
	return 0;
    }
    blocknum = offset / CNFS_BLOCKSIZE;
    longoffset = blocknum / (longsize * 8);
    bitoffset = blocknum % (longsize * 8);
    bitlong = *((ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
		+ longoffset);
    if (set_operation) {
	if (setbitvalue) {
	    mask = onarray[bitoffset];
	    bitlong |= mask;
	} else {
	    mask = offarray[bitoffset];
	    bitlong &= mask;
	}
	*((ULONG *) cycbuff->bitfield + (CNFS_BEFOREBITF / longsize)
	  + longoffset) = bitlong;
	return 2;	/* XXX Clean up return semantics */
    }
    /* It's a read operation */
    mask = onarray[bitoffset];
    return bitlong & mask;
}

/*
** CNFSartnam2rpnam -- Given a "filename" for an article,
**	e.g. "FOO:1024", return the CYCBUFF symbolic name, e.g. "FOO",
**	in a static buffer.
*/

char *
CNFSartnam2rpnam(char *filename)
{
	static char	buf[20];	/* XXX Icky ... see CYCBUFF.name const
					   in cnfs.h */
	char		*p;

	strncpy(buf, filename, 16);
	buf[16] = '\0';
	if ((p = strchr(buf, ':')) == NULL)
		return NULL;
	*p = '\0';
	return buf;
}

/*
** CNFSartnam2offset -- Given a "filename" for an article,
**	e.g. "FOO:1024", return the byte offset e.g. 1024
**	else -1 on error.
*/

CYCBUFF_OFF_T
CNFSartnam2offset(char *filename)
{
    char		*p;
    CYCBUFF_OFF_T	offset;

    if ((p = strchr(filename, ':')) == NULL)
	return -1;
    offset = CNFShex2offt(++p);
    return (offset > 0) ? offset : -1;
}

/*
** CNFSartname2cyclenum -- Given a "filename for an article, 
**	e.g. "FOO:1234:55", return the cycle number e.g. 55
**	else 0 on error.
*/

INT32_T
CNFSartnam2cyclenum(char *filename)
{
    char	*p;

    if ((p = strchr(filename, ':')) == NULL)
	return 0;
    if ((p = strchr(++p, ':')) == NULL)
	return 0;
    return (INT32_T) CNFShex2offt(++p);
}

/*
** CNFSmunmapbitfields() -- Call munmap() on all of the bitfields we've
**	previously mmap()'ed.
*/

void
CNFSmunmapbitfields(void)
{
    int	i;

    for (i = 0; i < cycbufftab_free; i++) {
	if (cycbufftab[i].bitfield != NULL) {
	    munmap(cycbufftab[i].bitfield, cycbufftab[i].minartoffset);
	    cycbufftab[i].bitfield = NULL;
	}
    }
}

/*
** "cycbuff" or "cycbuffname" may be NULL, but not both.  "cycbuff" has
** precedence.  Return file descriptor or -1 on error.
*/

int
CNFSGetWriteFd(CYCBUFF *cycbuff, char *cycbuffname, CYCBUFF_OFF_T offset)
{
    if (! __CNFS_Write_Allowed) {
	SMseterror(SMERR_INTERNAL, "CNFSGetWriteFd: Process not authorized to write article");
	syslog(LOG_CRIT, "CNFSGetWriteFd: Process not authorized to write article");
	return -1;
    }
    if (cycbuff == NULL) {
	if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	    SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	    syslog(L_ERROR, "CNFSGetWriteFd: NULL CYCBUFF and NULL name");
	    return -1;
	}
    }
    if (cycbuff->fdrdwr_inuse) {
	SMseterror(SMERR_INTERNAL, "RDWR file descriptor already in use");
	syslog(L_ERROR,
	       "CNFSGetWriteFd: RDWR file descriptor %d for %s already in use",
	       cycbuff->fdrdwr, cycbuff->name);
	return -1;
    }
    if (CNFSseek(cycbuff->fdrdwr, offset, SEEK_SET) < 0) {
	SMseterror(SMERR_INTERNAL, "CNFSseek() failed");
	syslog(L_ERROR, "CNFSseek() failed for '%s' offset 0x%s: %m",
	       cycbuff->name, CNFSofft2hex(offset, 0));
	return -1;
    }
    cycbuff->fdrdwr_inuse = 1;
    return cycbuff->fdrdwr;
}

/*
** Return 0 if successful, not 0 if not successful.
*/

int
CNFSPutWriteFd(CYCBUFF *cycbuff, char *cycbuffname, int fd)
{
    if (! __CNFS_Write_Allowed) {
	SMseterror(SMERR_INTERNAL, "CNFSPutWriteFd: Process not authorized to write article");
	syslog(LOG_CRIT, "CNFSPutWriteFd: Process not authorized to write article");
	return -1;
    }
    if (cycbuff == NULL) {
	if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	    SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	    return -1;
	}
    }
    if (! cycbuff->fdrdwr_inuse) {
	SMseterror(SMERR_INTERNAL, "RDWR file descriptor NOT reserved/in use");
	return -1;
    }
    cycbuff->fdrdwr_inuse = 0;
    return 0;
}

CYCBUFF *
CNFSgetcycbuffbyname(char *name)
{
    int   i;
 
    if (name == NULL)
	return NULL;
    for (i = 0; i < cycbufftab_free; i++)
	if (strcmp(name, cycbufftab[i].name) == 0) 
	    return &cycbufftab[i];
    return NULL;
}

/*
** "cycbuff" or "cycbuffname" may be NULL, but not both.  "cycbuff" has
** precedence.  Return file descriptor or -1 on error.
*/

int
CNFSGetReadFd(CYCBUFF *cycbuff, char *cycbuffname, CYCBUFF_OFF_T offset,
	      INT32_T cycnum, BOOL do_seek)
{
    int	i, rpindex, fd;

    if (fdpooluninitialized) {
        /* Initialize the fdpool array */
        for (rpindex = 0; rpindex < MAX_CYCBUFFS; rpindex++) {
            for (i = 0; i < MAX_DESCRIPTORS; i++) {
                fdpool[rpindex][i].fd = -1;
                fdpool[rpindex][i].inuse = 0;
            }
        }
        fdpooluninitialized = 0;
    }
    if (cycbuff == NULL) {
	if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	    SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	    syslog(L_ERROR, "CNFSGetReadFd: NULL CYCBUFF and NULL name");
	    return -1;
	}
    }

    /*
    ** Before bothering to search for a file descriptor & trigger I/O,
    ** check first to see if the article has been overwritten or cancelled.
    */
    if (! CNFSArtMayBeHere(cycbuff, offset, cycnum)) {
	SMseterror(SMERR_NOENT, NULL);
	return -1;
    }

    rpindex = cycbuff->index;
    /* Search for an open but unused fd for that RAWPART */
    fd = -1;
    for (i = 0; i < MAX_DESCRIPTORS; i++) {
	if (fdpool[rpindex][i].fd >= 0 && ! fdpool[rpindex][i].inuse) {
	    fd = fdpool[rpindex][i].fd;
	    fdpool[rpindex][i].inuse = 1;
	    break;
	}
	if (fdpool[rpindex][i].fd < 0)
	    break;
    }
    if (fd < 0) {
	/* Couldn't find an open + idle fd, so we'll open one now at 'i' */
	if (i >= MAX_DESCRIPTORS) {
	    SMseterror(SMERR_INTERNAL, NULL);
	    syslog(L_ERROR, "Index for fdpool too big for cycbuff %s",
		   cycbuff->name);
	    return -1;
	}
	if ((fd = open(cycbuff->path, O_RDONLY)) < 0) {
	    SMseterror(SMERR_INTERNAL, "FD open failed");
	    syslog(L_ERROR, "CNFSGetReadFd() open of cycbuff %s failed: %m",
		   cycbuff->name);
	    return -1;
	}
	/* 'i' has already been set to the first emptly slot */
	fdpool[rpindex][i].fd = fd;
	fdpool[rpindex][i].inuse = 1;
    }
    if (do_seek && CNFSseek(fd, offset, SEEK_SET) < 0) {
	SMseterror(SMERR_INTERNAL, "FD seek failed");
	syslog(L_ERROR, "CNFSGetReadFd() seek of cycbuff %s fd %d failed: %m",
	       cycbuff->name, fd);
	return -1;
    }
    return fd;
}

/*
** Return 0 if successful, not 0 if not successful.
*/

int
CNFSPutReadFd(CYCBUFF *cycbuff, char *cycbuffname, int fd)
{
    int	i, rpindex;

    if (cycbuff == NULL) {
	if ((cycbuff = CNFSgetcycbuffbyname(cycbuffname)) == NULL) {
	    SMseterror(SMERR_INTERNAL, "bogus cycbuff name");
	    return -1;
	}
    }
    for (i = 0; i < MAX_DESCRIPTORS; i++) {
	if (fdpool[rpindex][i].fd == fd) {
	    fdpool[rpindex][i].inuse = 0;
	    break;
	}
    }
    if (i == MAX_DESCRIPTORS) {
	SMseterror(SMERR_INTERNAL, NULL);
	syslog(L_ERROR, "PutReadFd(%s, --, %d) not found in table",
	       cycbuff->name, fd);
	return 1;
    }
    return 0;
}

int
CNFSArtMayBeHere(CYCBUFF *cycbuff, CYCBUFF_OFF_T offset, INT32_T cycnum)
{
    static	count = 0;
    int		i;

    if (++count % 1000 == 0) {	/* XXX 1K articles is just a guess */
	for (i = 0; i < cycbufftab_free; i++) {
	    CNFSReadFreeAndCycle(&cycbufftab[i]);
	}
    }
    /*
    ** The current cycle number may have advanced since the last time we
    ** checked it, so use a ">=" check instead of "==".  Our intent is
    ** avoid a false negative response, *not* a false positive response.
    */
    if (! (
	cycnum >= cycbuff->cyclenum ||
	(cycnum == cycbuff->cyclenum - 1 && offset > cycbuff->free))) {
	/* We've been overwritten */
	return 0;
    }
    return CNFSUsedBlock(cycbuff, offset, FALSE, FALSE);
}

/*
** CNFSReadFreeAndCycle() -- Read from disk the current values of CYCBUFF's
**	free pointer and cycle number.  Return 1 on success, 0 otherwise.
*/

int
CNFSReadFreeAndCycle(CYCBUFF *cycbuff)
{
    CYCBUFFEXTERN	rpx;
    CYCBUFF_OFF_T	tmpo;
    int			bytes;
    char		buf[64];

    if ((tmpo = CNFSseek(cycbuff->fdrd, (CYCBUFF_OFF_T) 0, SEEK_SET)) < 0) {
	syslog(LOG_CRIT, "CNFSReadFreeAndCycle: magic lseek failed: %m");
	SMseterror(SMERR_UNDEFINED, NULL);
	return 0;
    }
    if ((bytes = read(cycbuff->fdrd, &rpx,
		      sizeof(CYCBUFFEXTERN))) != sizeof(rpx)) {
	syslog(LOG_CRIT, "CNFSReadFreeAndCycle: magic read failed: %m");
	SMseterror(SMERR_UNDEFINED, NULL);
	return 0;
    }
    if (strncmp(rpx.magic, CNFS_MAGICV3, strlen(CNFS_MAGICV3)) != 0) {
	syslog(LOG_CRIT, "CNFSReadFreeAndCycle: bad magic number!!!");
	SMseterror(SMERR_INTERNAL,
		   "Bad magic number in a cycbuff, should never happen");
	return 0;
    }
    if (strncmp(rpx.name, cycbuff->name, CNFSNASIZ) != 0) {
	syslog(LOG_CRIT,
	       "CNFSReadFreeAndCycle: name mismatch: read %s for cycbuff %s", 
	       rpx.name, cycbuff->name);
	SMseterror(SMERR_INTERNAL,
		   "Cycbuff name changed (file descriptor musical chairs?), should never happen");
	return 0;
    }
    if (strncmp(rpx.path, cycbuff->path, CNFSPASIZ) != 0) {
	syslog(LOG_CRIT,
	       "CNFSREadFreeAndCycle: path mismatch: read %s for cycbuff %s",
	       rpx.path, cycbufftab->path);
	SMseterror(SMERR_INTERNAL,
		   "Cycbuff path changed (file descriptor musical chairs?), should never happen");
	return 0;
    }
    /* Sanity checks are done, time to do what we're supposed to do */
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.freea, CNFSLASIZ);
    cycbuff->free = CNFShex2offt(buf);
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.updateda, CNFSLASIZ);
    cycbuff->updated = CNFShex2offt(buf);
    buf[CNFSLASIZ] = '\0';
    strncpy(buf, rpx.cyclenuma, CNFSLASIZ);
    cycbuff->cyclenum = CNFShex2offt(buf);
    return 1;
}
