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
#include <logging.h>
#include <macros.h>
#include <configdata.h>
#include <libinn.h>
#include <methods.h>
#include "timehash.h"
#include "paths.h"

typedef struct {
    char                *base;    /* Base of the mmaped file */
    int                 len;      /* Length of the file */
    DIR                 *top;     /* Open handle on the top level directory */
    DIR                 *sec;     /* Open handle on the 2nd level directory */
    DIR                 *artdir;  /* Open handle on the article directory */
    struct dirent       *topde;   /* dirent entry for the last entry retrieved in top */
    struct dirent       *secde;   /* dirent entry for the last entry retrieved in sec */
} PRIV_TIMEHASH;

typedef enum {FIND_DIR, FIND_ART} FINDTYPE;

static SeqNum = 0;

static TOKEN MakeToken(time_t time, int seqnum, STORAGECLASS class) {
    TOKEN               token;
    unsigned int        i;
    unsigned short      s;

    memset(&token, '\0', sizeof(token));
    token.type = TOKEN_TIMEHASH;
    token.class = class;
    i = htonl(time);
    memcpy(token.token, &i, sizeof(i));
    if (sizeof(i) > 4)
	memmove(token.token, &token.token[sizeof(i) - 4], 4);
    s = htons(seqnum);
    memcpy(&token.token[4], &s + (sizeof(s) - 2), 2);
    return token;
}

static void BreakToken(TOKEN token, int *time, int *seqnum) {
    unsigned int        i;
    unsigned short      s = 0;

    memcpy(&i, token.token, sizeof(i));
    memcpy(&s, &token.token[4], sizeof(s));
    *time = ntohl(i);
    *seqnum = (int)ntohs(s);
}

static char *MakePath(int time, int seqnum, const STORAGECLASS class) {
    char *path;
    
    /* _PATH_SPOOL + '/time/xx/xx/yyyy-xxxx-zz' */
    path = NEW(char, strlen(_PATH_SPOOL) + 32);
    sprintf(path, "%s/time/%02x/%02x/%04x-%04x-%02x", _PATH_SPOOL,
	    (time >> 16) & 0xff, (time >> 8) & 0xff, seqnum,
	    (time & 0xff) | ((time >> 16 & 0xff00)), class);
    return path;
}

BOOL timehash_init(void) {
    if (STORAGE_TOKEN_LENGTH < 6) {
	syslog(L_FATAL, "timehash: token length is less than 6 bytes");
	return FALSE;
    }
    return TRUE;
}

TOKEN timehash_store(const ARTHANDLE article, STORAGECLASS class) {
    char                *path;
    char                *p;
    time_t              now;
    TOKEN               token;
    int                 fd;
    int                 result;
    int                 seq;

    now = time(NULL);

    seq = SeqNum;
    SeqNum = (SeqNum + 1) & 0xffff;
    path = MakePath(now, seq, class);

    if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
	p = strrchr(path, '/');
	*p = '\0';
	if (!MakeDirectory(path, TRUE)) {
	    syslog(L_ERROR, "timehash: could not make directory %s %m", path);
	    token.type = TOKEN_EMPTY;
	    DISPOSE(path);
	    return token;
	} else {
	    *p = '/';
	    if ((fd = open(path, O_CREAT|O_EXCL|O_WRONLY, ARTFILE_MODE)) < 0) {
		syslog(L_ERROR, "timehash: could not open %s %m", path);
		token.type = TOKEN_EMPTY;
		DISPOSE(path);
		return token;
	    }
	}
    }

    if ((result = write(fd, article.data, article.len)) != article.len) {
	syslog(L_ERROR, "timehash error writing %s %m", path);
	close(fd);
	token.type = TOKEN_EMPTY;
	unlink(path);
	DISPOSE(path);
	return token;
    }
    close(fd);
    DISPOSE(path);
    return MakeToken(now, seq, class);
}

static ARTHANDLE *OpenArticle(const char *path, RETRTYPE amount) {
    int                 fd;
    PRIV_TIMEHASH       *private;
    char                *p;
    struct stat         sb;
    ARTHANDLE           *art;

    if ((fd = open(path, O_RDONLY)) < 0) {
	return NULL;
    }

    art = NEW(ARTHANDLE, 1);
    art->type = TOKEN_TIMEHASH;

    if (amount == RETR_STAT) {
	art->data = NULL;
	art->len = 0;
	art->private = NULL;
	return art;
    }

    if (fstat(fd, &sb) < 0) {
	syslog(L_ERROR, "timehash: could not fstat article: %m");
	DISPOSE(art);
	return NULL;
    }
    
    art->private = (void *)private = NEW(PRIV_TIMEHASH, 1);
    private->len = sb.st_size;
    if ((private->base = mmap((MMAP_PTR)0, sb.st_size, PROT_READ, MAP__ARG, fd, 0)) == (MMAP_PTR)-1) {
	syslog(L_ERROR, "timehash: could not mmap article: %m");
	DISPOSE(art->private);
	DISPOSE(art);
	return NULL;
    }
    close(fd);

    private->top = NULL;
    private->sec = NULL;
    private->artdir = NULL;
    private->topde = NULL;
    private->secde = NULL;
    
    if (amount == RETR_ALL) {
	art->data = private->base;
	art->len = private->len;
	return art;
    }
    
    if ((p = SMFindBody(private->base, private->len)) == NULL)
	return NULL;

    if (amount == RETR_HEAD) {
	art->data = private->base;
	art->len = p - private->base;
	return art;
    }

    if (amount == RETR_BODY) {
	art->data = p + 4;
	art->len = art->len - (private->base - p - 4);
    }
    return NULL;
}

ARTHANDLE *timehash_retrieve(const TOKEN token, RETRTYPE amount) {
    int                 time;
    int                 seqnum;
    char                *path;
    ARTHANDLE           *art;
    
    if (token.type != TOKEN_TIMEHASH)
	return NULL;

    BreakToken(token, &time, &seqnum);
    path = MakePath(time, seqnum, token.class);
    art = OpenArticle(path, amount);
    DISPOSE(path);
    return art;
}

void timehash_freearticle(ARTHANDLE *article) {
    PRIV_TIMEHASH       *private;

    if (article->private) {
	private = (PRIV_TIMEHASH *)article->private;
#ifdef MADV_DONTNEED
	madvise(private->base, private->len, MADV_DONTNEED);
#endif
	munmap(private->base, private->len);
    }
    if (private->top)
	closedir(private->top);
    if (private->sec)
	closedir(private->sec);
    if (private->artdir)
	closedir(private->artdir);
    DISPOSE(private);
    DISPOSE(article);
}

BOOL timehash_cancel(TOKEN token) {
    int                 time;
    int                 seqnum;
    char                *path;
    int                 result;

    BreakToken(token, &time, &seqnum);
    path = MakePath(time, seqnum, token.class);
    result = unlink(path);
    DISPOSE(path);
    if (result < 0)
	return FALSE;
    else
	return TRUE;
}

static struct dirent *FindDir(DIR *dir, FINDTYPE type) {
    struct dirent       *de;
    
    while ((de = readdir(dir)) != NULL) {
	if (type == FIND_DIR)
	    if ((strlen(de->d_name) == 2) && isxdigit(de->d_name[0]) && isxdigit(de->d_name[1]))
		return de;
	if (type == FIND_ART)
	    if ((strlen(de->d_name) == 9) &&
		isxdigit(de->d_name[0]) &&
		isxdigit(de->d_name[8]) &&
		(de->d_name[4] == '-'))
		return de;
	}

    return NULL;
}

ARTHANDLE *timehash_next(const ARTHANDLE *article, RETRTYPE amount) {
    PRIV_TIMEHASH       priv;
    PRIV_TIMEHASH       *newpriv;
    char                *path;
    struct dirent       *de;
    ARTHANDLE           *art;

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
		    DISPOSE(path);
		    return NULL;
		}
		if ((priv.topde = FindDir(priv.top, FIND_DIR)) == NULL) {
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
    return art;
}

void timehash_shutdown(void) {
}
