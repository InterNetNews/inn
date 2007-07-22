/*  $Id$
**
**  Timehash based storage method.
*/

#include "config.h"
#include "clibrary.h"
#include "portable/mmap.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <time.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "inn/libinn.h"
#include "methods.h"
#include "inn/paths.h"
#include "timehash.h"

typedef struct {
    char                *base;    /* Base of the mmaped file */
    int                 len;      /* Length of the file */
    DIR                 *top;     /* Open handle on the top level directory */
    DIR                 *sec;     /* Open handle on the 2nd level directory */
    DIR			*ter;     /* Open handle on the third level directory */
    DIR                 *artdir;  /* Open handle on the article directory */
    struct dirent       *topde;   /* dirent entry for the last entry retrieved in top */
    struct dirent       *secde;   /* dirent entry for the last entry retrieved in sec */
    struct dirent       *terde;   /* dirent entry for the last entry retrieved in ter */
} PRIV_TIMEHASH;

typedef enum {FIND_DIR, FIND_ART, FIND_TOPDIR} FINDTYPE;

static int SeqNum = 0;

static TOKEN MakeToken(time_t now, int seqnum, STORAGECLASS class, TOKEN *oldtoken) {
    TOKEN               token;
    unsigned int        i;
    unsigned short      s;

    if (oldtoken == (TOKEN *)NULL)
	memset(&token, '\0', sizeof(token));
    else 
	memcpy(&token, oldtoken, sizeof(token));
    token.type = TOKEN_TIMEHASH;
    token.class = class;
    i = htonl(now);
    memcpy(token.token, &i, sizeof(i));
    if (sizeof(i) > 4)
	memmove(token.token, &token.token[sizeof(i) - 4], 4);
    s = htons(seqnum);
    memcpy(&token.token[4], &s + (sizeof(s) - 2), 2);
    return token;
}

static void BreakToken(TOKEN token, time_t *now, int *seqnum) {
    unsigned int        i;
    unsigned short      s = 0;

    memcpy(&i, token.token, sizeof(i));
    memcpy(&s, &token.token[4], sizeof(s));
    *now = ntohl(i);
    *seqnum = (int)ntohs(s);
}

static char *MakePath(int now, int seqnum, const STORAGECLASS class) {
    char *path;
    size_t length;
    
    /* innconf->patharticles + '/time-zz/xx/xx/yyyy-xxxx' */
    length = strlen(innconf->patharticles) + 32;
    path = xmalloc(length);
    snprintf(path, length, "%s/time-%02x/%02x/%02x/%04x-%04x",
             innconf->patharticles, class,
             (now >> 16) & 0xff, (now >> 8) & 0xff, seqnum,
             (now & 0xff) | ((now >> 16 & 0xff00)));
    return path;
}

static TOKEN *PathToToken(char *path) {
    int			n;
    unsigned int	t1, t2, t3, seqnum, class;
    time_t		now;
    static TOKEN	token;

    n = sscanf(path, "time-%02x/%02x/%02x/%04x-%04x", &class, &t1, &t2, &seqnum, &t3);
    if (n != 5)
	return (TOKEN *)NULL;
    now = ((t1 << 16) & 0xff0000) | ((t2 << 8) & 0xff00) | ((t3 << 16) & 0xff000000) | (t3 & 0xff);
    token = MakeToken(now, seqnum, class, (TOKEN *)NULL);
    return &token;
}

bool timehash_init(SMATTRIBUTE *attr) {
    if (attr == NULL) {
        warn("timehash: attr is NULL");
	SMseterror(SMERR_INTERNAL, "attr is NULL");
	return false;
    }
    attr->selfexpire = false;
    attr->expensivestat = true;
    if (STORAGE_TOKEN_LENGTH < 6) {
        warn("timehash: token length is less than six bytes");
	SMseterror(SMERR_TOKENSHORT, NULL);
	return false;
    }
    return true;
}

TOKEN timehash_store(const ARTHANDLE article, const STORAGECLASS class) {
    char                *path;
    char                *p;
    time_t              now;
    TOKEN               token;
    int                 fd;
    ssize_t             result;
    int                 seq;
    int                 i;

    if (article.arrived == (time_t)0)
	now = time(NULL);
    else
	now = article.arrived;

    memset(&token, 0, sizeof(token));

    for (i = 0; i < 0x10000; i++) {
	seq = SeqNum;
	SeqNum = (SeqNum + 1) & 0xffff;
	path = MakePath(now, seq, class);

        if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
	    if (errno == EEXIST)
		continue;
	    p = strrchr(path, '/');
	    *p = '\0';
	    if (!MakeDirectory(path, true)) {
                syswarn("timehash: could not make directory %s", path);
	        token.type = TOKEN_EMPTY;
	        free(path);
	        SMseterror(SMERR_UNDEFINED, NULL);
	        return token;
	    } else {
	        *p = '/';
	        if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
		    SMseterror(SMERR_UNDEFINED, NULL);
                    syswarn("timehash: could not create %s", path);
		    token.type = TOKEN_EMPTY;
		    free(path);
		    return token;
	        }
	    }
        }
	break;
    }
    if (i == 0x10000) {
	SMseterror(SMERR_UNDEFINED, NULL);
        warn("timehash: all sequence numbers for time %lu and class %d are"
             " reserved", (unsigned long) now, class);
	token.type = TOKEN_EMPTY;
	free(path);
	return token;
    }

    result = xwritev(fd, article.iov, article.iovcnt);
    if (result != (ssize_t) article.len) {
	SMseterror(SMERR_UNDEFINED, NULL);
        syswarn("timehash: error writing %s", path);
	close(fd);
	token.type = TOKEN_EMPTY;
	unlink(path);
	free(path);
	return token;
    }
    close(fd);
    free(path);
    return MakeToken(now, seq, class, article.token);
}

static ARTHANDLE *OpenArticle(const char *path, RETRTYPE amount) {
    int                 fd;
    PRIV_TIMEHASH       *private;
    char                *p;
    struct stat         sb;
    ARTHANDLE           *art;

    if (amount == RETR_STAT) {
        if (access(path, R_OK) < 0) {
            SMseterror(SMERR_UNDEFINED, NULL);
            return NULL;
        }
        art = xmalloc(sizeof(ARTHANDLE));
        art->type = TOKEN_TIMEHASH;
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
    art->type = TOKEN_TIMEHASH;

    if (fstat(fd, &sb) < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
        syswarn("timehash: could not fstat article");
	free(art);
	return NULL;
    }
    
    private = xmalloc(sizeof(PRIV_TIMEHASH));
    art->private = (void *)private;
    private->len = sb.st_size;
    if (innconf->articlemmap) {
	if ((private->base = mmap(NULL, sb.st_size, PROT_READ, MAP_SHARED, fd, 0)) == MAP_FAILED) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("timehash: could not mmap article");
	    free(art->private);
	    free(art);
	    return NULL;
	}
        if (amount == RETR_ALL)
            madvise(private->base, sb.st_size, MADV_WILLNEED);
        else
            madvise(private->base, sb.st_size, MADV_SEQUENTIAL);
    } else {
	private->base = xmalloc(private->len);
	if (read(fd, private->base, private->len) < 0) {
	    SMseterror(SMERR_UNDEFINED, NULL);
            syswarn("timehash: could not read article");
	    free(private->base);
	    free(art->private);
	    free(art);
	    return NULL;
	}
    }
    close(fd);

    private->top = NULL;
    private->sec = NULL;
    private->ter = NULL;
    private->artdir = NULL;
    private->topde = NULL;
    private->secde = NULL;
    private->terde = NULL;
    
    if (amount == RETR_ALL) {
	art->data = private->base;
	art->len = private->len;
	return art;
    }
    
    if ((p = wire_findbody(private->base, private->len)) == NULL) {
	SMseterror(SMERR_NOBODY, NULL);
	if (innconf->articlemmap)
	    munmap(private->base, private->len);
	else
	    free(private->base);
	free(art->private);
	free(art);
	return NULL;
    }

    if (amount == RETR_HEAD) {
	art->data = private->base;
	art->len = p - private->base;
	return art;
    }

    if (amount == RETR_BODY) {
	art->data = p;
	art->len = private->len - (p - private->base);
	return art;
    }
    SMseterror(SMERR_UNDEFINED, "Invalid retrieve request");
    if (innconf->articlemmap)
	munmap(private->base, private->len);
    else
	free(private->base);
    free(art->private);
    free(art);
    return NULL;
}

ARTHANDLE *timehash_retrieve(const TOKEN token, const RETRTYPE amount) {
    time_t              now;
    int                 seqnum;
    char                *path;
    ARTHANDLE           *art;
    static TOKEN	ret_token;
    
    if (token.type != TOKEN_TIMEHASH) {
	SMseterror(SMERR_INTERNAL, NULL);
	return NULL;
    }

    BreakToken(token, &now, &seqnum);
    path = MakePath(now, seqnum, token.class);
    if ((art = OpenArticle(path, amount)) != (ARTHANDLE *)NULL) {
	art->arrived = now;
	ret_token = token;
	art->token = &ret_token;
    }
    free(path);
    return art;
}

void timehash_freearticle(ARTHANDLE *article) {
    PRIV_TIMEHASH       *private;

    if (!article)
	return;
    
    if (article->private) {
	private = (PRIV_TIMEHASH *)article->private;
	if (innconf->articlemmap)
	    munmap(private->base, private->len);
	else
	    free(private->base);
	if (private->top)
	    closedir(private->top);
	if (private->sec)
	    closedir(private->sec);
	if (private->ter)
	    closedir(private->ter);
	if (private->artdir)
	    closedir(private->artdir);
	free(private);
    }
    free(article);
}

bool timehash_cancel(TOKEN token) {
    time_t              now;
    int                 seqnum;
    char                *path;
    int                 result;

    BreakToken(token, &now, &seqnum);
    path = MakePath(now, seqnum, token.class);
    result = unlink(path);
    free(path);
    if (result < 0) {
	SMseterror(SMERR_UNDEFINED, NULL);
	return false;
    }
    return true;
}

static struct dirent *FindDir(DIR *dir, FINDTYPE type) {
    struct dirent       *de;
    
    while ((de = readdir(dir)) != NULL) {
        if (type == FIND_TOPDIR)
	    if ((strlen(de->d_name) == 7) &&
		(strncmp(de->d_name, "time-", 5) == 0) &&
		isxdigit((int)de->d_name[5]) &&
		isxdigit((int)de->d_name[6]))
	        return de;

	if (type == FIND_DIR)
	    if ((strlen(de->d_name) == 2) && isxdigit((int)de->d_name[0]) && isxdigit((int)de->d_name[1]))
		return de;

	if (type == FIND_ART)
	    if ((strlen(de->d_name) == 9) &&
		isxdigit((int)de->d_name[0]) &&
		isxdigit((int)de->d_name[1]) &&
		isxdigit((int)de->d_name[2]) &&
		isxdigit((int)de->d_name[3]) &&
		isxdigit((int)de->d_name[5]) &&
		isxdigit((int)de->d_name[6]) &&
		isxdigit((int)de->d_name[7]) &&
		isxdigit((int)de->d_name[8]) &&
		(de->d_name[4] == '-'))
		return de;
	}

    return NULL;
}

ARTHANDLE *
timehash_next(ARTHANDLE *article, const RETRTYPE amount)
{
    PRIV_TIMEHASH       priv;
    PRIV_TIMEHASH       *newpriv;
    char                *path;
    struct dirent       *de;
    ARTHANDLE           *art;
    int                 seqnum;
    size_t              length;

    length = strlen(innconf->patharticles) + 32;
    path = xmalloc(length);
    if (article == NULL) {
	priv.top = NULL;
	priv.sec = NULL;
	priv.ter = NULL;
	priv.artdir = NULL;
	priv.topde = NULL;
	priv.secde = NULL;
	priv.terde = NULL;
    } else {
	priv = *(PRIV_TIMEHASH *)article->private;
	free(article->private);
	free(article);
	if (priv.base != NULL) {
	    if (innconf->articlemmap)
		munmap(priv.base, priv.len);
	    else
		free(priv.base);
	}
    }

    while (!priv.artdir || ((de = FindDir(priv.artdir, FIND_ART)) == NULL)) {
	if (priv.artdir) {
	    closedir(priv.artdir);
	    priv.artdir = NULL;
	}
	while (!priv.ter || ((priv.terde = FindDir(priv.ter, FIND_DIR)) == NULL)) {
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
	if ((priv.artdir = opendir(path)) == NULL)
	    continue;
    }
    if (de == NULL)
	return NULL;
    snprintf(path, length, "%s/%s/%s/%s/%s", innconf->patharticles, priv.topde->d_name, priv.secde->d_name, priv.terde->d_name, de->d_name);

    art = OpenArticle(path, amount);
    if (art == (ARTHANDLE *)NULL) {
	art = xmalloc(sizeof(ARTHANDLE));
	art->type = TOKEN_TIMEHASH;
	art->data = NULL;
	art->len = 0;
	art->private = xmalloc(sizeof(PRIV_TIMEHASH));
	newpriv = (PRIV_TIMEHASH *)art->private;
	newpriv->base = NULL;
    }
    newpriv = (PRIV_TIMEHASH *)art->private;
    newpriv->top = priv.top;
    newpriv->sec = priv.sec;
    newpriv->ter = priv.ter;
    newpriv->artdir = priv.artdir;
    newpriv->topde = priv.topde;
    newpriv->secde = priv.secde;
    newpriv->terde = priv.terde;
    snprintf(path, length, "%s/%s/%s/%s", priv.topde->d_name, priv.secde->d_name, priv.terde->d_name, de->d_name);
    art->token = PathToToken(path);
    BreakToken(*art->token, &(art->arrived), &seqnum);
    free(path);
    return art;
}

bool timehash_ctl(PROBETYPE type, TOKEN *token UNUSED, void *value) {
    struct artngnum *ann;

    switch (type) {
    case SMARTNGNUM:
	if ((ann = (struct artngnum *)value) == NULL)
	    return false;
	/* make SMprobe() call timehash_retrieve() */
	ann->artnum = 0;
	return true;
    default:
	return false;
    }
}

bool
timehash_flushcacheddata(FLUSHTYPE type UNUSED)
{
    return true;
}

void
timehash_printfiles(FILE *file, TOKEN token, char **xref UNUSED,
                    int ngroups UNUSED)
{
    time_t now;
    int seqnum;
    char *path;
    
    BreakToken(token, &now, &seqnum);
    path = MakePath(now, seqnum, token.class);
    fprintf(file, "%s\n", path);
}

void timehash_shutdown(void) {
}
