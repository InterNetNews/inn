/*  $Revision$
**
**  Article-related routines.
*/
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <string.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include "configdata.h"
#if defined(HAVE_UNISTD_H)
# include <unistd.h>
#endif
#if	defined(DO_NEED_TIME)
#include <time.h>
#endif  /* defined(DO_NEED_TIME) */
#include <sys/time.h>
#include "clibrary.h"
#include "nnrpd.h"


/*
**  Data structures for use in ARTICLE/HEAD/BODY/STAT common code.
*/
typedef enum _SENDTYPE {
    STarticle,
    SThead,
    STbody,
    STstat
} SENDTYPE;

typedef struct _SENDDATA {
    SENDTYPE	Type;
    int		ReplyCode;
    STRING	Item;
} SENDDATA;

typedef struct _OVERDATA {
    int		index;
    char	*addr;
    long	size;
    FILE	*fp;
    int		offset;
} OVERDATA;


/*
**  Information about the schema of the news overview files.
*/
typedef struct _ARTOVERFIELD {
    char	*Header;
    int		Length;
    BOOL	HasHeader;
    BOOL	NeedsHeader;
} ARTOVERFIELD;


STATIC char		ARTnotingroup[] = NNTP_NOTINGROUP;
STATIC char		ARTnoartingroup[] = NNTP_NOARTINGRP;
STATIC char		ARTnocurrart[] = NNTP_NOCURRART;
STATIC char             *ARTmem = NULL;
STATIC int              ARTlen;
STATIC ARTHANDLE        *ARThandle = NULL;
STATIC BOOL		WireFormat = 0;
STATIC QIOSTATE		*ARTqp = NULL;
STATIC ARTOVERFIELD	*ARTfields;
STATIC int		ARTfieldsize;
STATIC int		ARTfirstfullfield = 0;
STATIC SENDDATA		SENDbody = {
    STbody,	NNTP_BODY_FOLLOWS_VAL,		"body"
};
STATIC SENDDATA		SENDarticle = {
    STarticle,	NNTP_ARTICLE_FOLLOWS_VAL,	"article"
};
STATIC SENDDATA		SENDstat = {
    STstat,	NNTP_NOTHING_FOLLOWS_VAL,	"status"
};
STATIC SENDDATA		SENDhead = {
    SThead,	NNTP_HEAD_FOLLOWS_VAL,		"head"
};

extern unsigned int RARTtable[];
extern int RARTcount;
extern int RARTenable;

/*
**  Overview state information.
*/
STATIC char *		OVERmem = NULL;		/* mmaped overview file */
STATIC int		OVERlen;		/* Length of the overview file */
STATIC FILE		*OVERfp = NULL;		/* Open overview file	*/
STATIC ARTNUM		OVERarticle;		/* Current article	*/
STATIC int		OVERopens;		/* Number of opens done	*/
STATIC char *		OVERmempos;		/* Current position in mmaped overview file */

STATIC struct iovec	iov[IOV_MAX];
STATIC int		queued_iov = 0;

/* Prototypes */
STATIC BOOL IsCancelled(int artnum);
STATIC BOOL IsCancelledByIndex(int i, int artnum);

BOOL PushIOv(void) {
    fflush(stdout);
    if (writev(STDOUT_FILENO, iov, queued_iov) <= 0) {
        queued_iov = 0;
	return FALSE;
    }
    queued_iov = 0;
    return TRUE;
}

BOOL SendIOv(char *p, int len) {
    iov[queued_iov].iov_base = p;
    iov[queued_iov++].iov_len = len;
    if (queued_iov == IOV_MAX)
        return PushIOv();
    return TRUE;
}

STATIC char		*_IO_buffer_ = NULL;
STATIC int		highwater = 0;

BOOL PushIOb(void) {
    fflush(stdout);
    if (write(STDOUT_FILENO, _IO_buffer_, highwater) != highwater) {
        highwater = 0;
        return FALSE;
    }
    highwater = 0;
    return TRUE;
}

BOOL SendIOb(char *p, int len) {
    int tocopy;
    
    if (_IO_buffer_ == NULL)
        _IO_buffer_ = NEW(char, BIG_BUFFER);

    while (len > 0) {
        tocopy = (len > (BIG_BUFFER - highwater)) ? (BIG_BUFFER - highwater) : len;
        memcpy(&_IO_buffer_[highwater], p, tocopy);
        p += tocopy;
        highwater += tocopy;
        len -= tocopy;
        if (highwater == BIG_BUFFER)
            PushIOb();
    }
    return TRUE;
}


/*
**  Read the overview schema.
*/
void ARTreadschema(void)
{
    static char			*SCHEMA = NULL;
    FILE			*F;
    char			*p;
    ARTOVERFIELD		*fp;
    int				i;
    char			buff[SMBUF];

    /* Open file, count lines. */
    if (SCHEMA == NULL)
	SCHEMA = COPY(cpcatpath(innconf->pathetc, _PATH_SCHEMA));
    if ((F = fopen(SCHEMA, "r")) == NULL)
	return;
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
	    fp->NeedsHeader = EQ(p, "full");
	    if (ARTfirstfullfield == 0)
	      ARTfirstfullfield = fp - ARTfields + 1;
	}
	else
	    fp->NeedsHeader = FALSE;
	fp->HasHeader = FALSE;
	fp->Header = COPY(buff);
	fp->Length = strlen(buff);
	fp++;
    }
    ARTfieldsize = fp - ARTfields;
    (void)fclose(F);
}


/*
**  If we have an article open, close it.
*/
void ARTclose(void)
{
    if (ARTmem) {
	if (ARThandle) {
	    SMfreearticle(ARThandle);
	} else {
#if defined(MADV_DONTNEED) && defined(HAVE_MADVISE)
	    madvise(ARTmem, ARTlen, MADV_DONTNEED);
#endif
	    munmap(ARTmem, ARTlen);
	}
	ARTmem = NULL;
	WireFormat = FALSE;
	ARTlen = 0;
	ARThandle = NULL;
    }
    if (ARTqp) {
	QIOclose(ARTqp);
	ARTqp = NULL;
    }
}

STATIC BOOL ARTinstore(int i)
{
    TOKEN token;
    char *tokentext;
    OVERINDEX index;
    ARTHANDLE *art;
    struct timeval	stv, etv;

    if (IsCancelledByIndex(i,0))
        return FALSE;

    if (ARTnumbers[i].Tokenretrieved)
	token = ARTnumbers[i].Token;
    else {
	if (!ARTnumbers[i].Index) {
	   ARTnumbers[i].Token.cancelled = TRUE;
	   return FALSE;
	}
	UnpackOverIndex(*(ARTnumbers[i].Index), &index);
	ARTnumbers[i].Tokenretrieved = TRUE;
	if ((tokentext = HISgetent(&index.hash, FALSE, NULL)) == (char *)NULL) {
	   ARTnumbers[i].Token.cancelled = TRUE;
	   return FALSE;
	}
	token = ARTnumbers[i].Token = TextToToken(tokentext);
	if (   token.type == TOKEN_EMPTY
	    || token.index == OVER_NONE
	    || token.cancelled )
	 return FALSE;
    }
    if (innconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);
    art = SMretrieve(token, RETR_STAT);
    if (innconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERartcheck+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERartcheck+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if (art) {
	SMfreearticle(art);
	return TRUE;
    } else return FALSE;
}

/*
**  Find an article number in the article array via a binary search;
**  return -1 if not found.  Cache last hit to make linear lookups
**  faster.
*/
STATIC int ARTfind(ARTNUM i, BOOL needcheck)
{
    ARTLIST		*bottom;
    ARTLIST		*middle;
    ARTLIST		*top;

    if (ARTsize == 0)
	return -1;

    top = &ARTnumbers[ARTsize - 1];
    if (ARTcache) {
        /* They're finding the same article again. */
        if (ARTcache->ArtNum == i) {
	    if (needcheck) {
		return ARTinstore(ARTcache - ARTnumbers)
		    ? (ARTcache - ARTnumbers) : -1;
	    }
            else
                return ARTcache - ARTnumbers;
        }
        else if ((++ARTcache <= top) && (ARTcache->ArtNum <= i)) {
            /* Next article. */
            if (ARTcache->ArtNum == i) {
                if (needcheck) {
                    return ARTinstore(ARTcache - ARTnumbers)
                        ? (ARTcache - ARTnumbers) : -1;
                }
                else
                  return ARTcache - ARTnumbers;
            }
            bottom = ARTcache;
        }
        else if ( (ARTcache->ArtNum > i) && ((--ARTcache)->ArtNum < i) ) {
            /* Missing article. */
            return -1;
        }
        else {
            ARTcache=NULL;
            bottom = ARTnumbers;
        }
    }
    else
        bottom=ARTnumbers;

    /* The first hit when looping through is always bottom.
     * This is a common case, and also happens to be the worst-case
     * for this binary search.
     * So take the gamble, and check it.
     */
    if (bottom->ArtNum >= i)
    {
        ARTcache=bottom;
        if (bottom->ArtNum == i)
        {
            if (needcheck) {
                return ARTinstore(bottom - ARTnumbers)
                    ? (bottom-ARTnumbers) : -1;
            }
            return bottom-ARTnumbers;
        }
        else
        {
            /* The first article requested has already been cancelled. */
            return -1;
        }
    }
    for ( ; ; ) {
	if ((i < bottom->ArtNum) || (i > top->ArtNum))
	    break;

	middle = bottom + (top - bottom) / 2;
	if (i == middle->ArtNum) {
	    /* Found it; update cache. */
	    ARTcache = middle;
	    if (needcheck) {
		return ARTinstore(middle - ARTnumbers)
		    ? (middle - ARTnumbers) : -1;
	    }
	    return middle- ARTnumbers;
	}

	if (i > middle->ArtNum)
	    bottom = middle + 1;
	else
	    top = middle;
    }
    return -1;
}

/*
**  If the article name is valid, open it and stuff in the ID.
*/
STATIC BOOL ARTopen(char *name)
{
    static ARTNUM	save_artnum;
    struct stat		Sb;
    int			fd;
    ARTNUM              artnum;
    int                 i;
    OVERINDEX           index;
    char		*tokentext;
    TOKEN		token;

    /* Re-use article if it's the same one. */
    if (save_artnum == (artnum = atol(name))) {
	if (ARTmem)
	    return TRUE;
	if (ARTqp && (QIOrewind(ARTqp) != -1)) 
	    return TRUE;
    }
    ARTclose();

    if ((i = ARTfind(artnum, FALSE)) < 0)
	return FALSE;

    if ((ARTnumbers[i].ArtNum == artnum) && innconf->storageapi) {
        if (IsCancelledByIndex(i,artnum))
            return FALSE;

	if (ARTnumbers[i].Tokenretrieved)
	    token = ARTnumbers[i].Token;
	else {
	    if (!ARTnumbers[i].Index) {
		/* index not found, the article is treat as cancelled */
		ARTnumbers[i].Token.cancelled = TRUE;
		return FALSE;
	    }
	    UnpackOverIndex(*(ARTnumbers[i].Index), &index);
	    ARTnumbers[i].Tokenretrieved = TRUE;
	    if ((tokentext = HISgetent(&index.hash, FALSE, NULL)) == (char *)NULL) {
		ARTnumbers[i].Token.cancelled = TRUE;
		return FALSE;
	    }
	    token = ARTnumbers[i].Token = TextToToken(tokentext);
	}
	if (token.type != TOKEN_EMPTY && token.index != OVER_NONE && !token.cancelled) {
	    if ((ARThandle = SMretrieve(token, RETR_ALL)) == NULL) {
		ARTnumbers[i].Token.cancelled = TRUE;
		return FALSE;
	    }
	    ARTmem = ARThandle->data;
	    ARTlen = ARThandle->len;
	    save_artnum = artnum;
	    return TRUE;
	} else {
	    ARTnumbers[i].Token.cancelled = TRUE;
	    return FALSE;
	}
    }
    /* Open it, make sure it's a regular file. */
    if (innconf->articlemmap) {
	if ((fd = open(name, O_RDONLY)) < 0)
	    return FALSE;
	if ((fstat(fd, &Sb) < 0) || !S_ISREG(Sb.st_mode)) {
	    close(fd);
	    return FALSE;
	}
	ARTlen = Sb.st_size;
	if ((ARTmem = mmap(0, ARTlen, PROT_READ, MAP_SHARED, fd, 0)) == (MMAP_PTR)-1) {
	    close(fd);
	    return FALSE;
	}
	close(fd);
    } else {
	if ((ARTqp = QIOopen(name)) == NULL)
	    return FALSE;
	if (fstat(QIOfileno(ARTqp), &Sb) < 0 || !S_ISREG(Sb.st_mode)) {
	    ARTclose();
	    return FALSE;
	}
	ARTlen = Sb.st_size;
	CloseOnExec(QIOfileno(ARTqp), TRUE);
    }

    save_artnum = artnum;
    return TRUE;
}


/*
**  Open the article for a given Message-ID.
*/
STATIC BOOL ARTopenbyid(char *msg_id, ARTNUM *ap)
{
    char		*p;
    char		*q;
    struct stat		Sb;
    int			fd;
    HASH		hash = HashMessageID(msg_id);
    TOKEN		token;
    int			pathlen;
    char		*path;

    *ap = 0;
    if ((p = HISgetent(&hash, innconf->storageapi ? FALSE : TRUE, NULL)) == NULL)
	return FALSE;

    if (IsToken(p)) {
	token = TextToToken(p);
	if (token.type == TOKEN_EMPTY || token.index == OVER_NONE || token.cancelled)
	    return FALSE;
	if ((ARThandle = SMretrieve(token, RETR_ALL)) == NULL) {
	    return FALSE;
	}
	ARTmem = ARThandle->data;
	ARTlen = ARThandle->len;
	*ap = 0;
    } else {
	pathlen = SPOOLlen + 1 + strlen(p) + 1;
	path = NEW(char, pathlen);
	(void)sprintf(path, "%s/%s", innconf->patharticles, p);
	if (innconf->articlemmap) {
	    if ((fd = open(path, O_RDONLY)) < 0) {
		DISPOSE(path);
		return FALSE;
	    }
	    if ((fstat(fd, &Sb) < 0) || !S_ISREG(Sb.st_mode)) {
		close(fd);
		DISPOSE(path);
		return FALSE;
	    }
	    ARTlen = Sb.st_size;
	    if ((ARTmem = mmap(0, ARTlen, PROT_READ, MAP_SHARED, fd, 0)) == (MMAP_PTR)-1) {
		close(fd);
		DISPOSE(path);
		return FALSE;
	    }
	    close(fd);
	} else {
	    if ((ARTqp = QIOopen(path)) == NULL) {
		DISPOSE(path);
		return FALSE;
	    }
	    if (fstat(QIOfileno(ARTqp), &Sb) < 0 || !S_ISREG(Sb.st_mode)) {
		ARTclose();
		DISPOSE(path);
		return FALSE;
	    }
	    CloseOnExec(QIOfileno(ARTqp), TRUE);
	}
	if ((q = strrchr(p, '/')) != NULL)
	    *q++ = '\0';
	if (GRPlast[0] && EQ(p, GRPlast))
	    *ap = atol(q);
	DISPOSE(path);
    }
    return TRUE;
}

/*
 * Figure out if an article has been cancelled.
 * As a side-effect (and a very important one), sets
 * the Token for the article.
 * IsCancelled uses the article number proper.
 * IsCancelledByIndex uses an index into ARTnumbers.
 */
STATIC BOOL IsCancelled(int artnum)
{
    int i;
    
    if (!innconf->storageapi)
        return FALSE;

    if ((i = ARTfind(artnum, FALSE)) < 0)
        return TRUE;

    
    return IsCancelledByIndex(i,artnum);
}

STATIC BOOL IsCancelledByIndex(int i, int artnum)
{
    OVERINDEX index;
    char *tokentext;

    if (!innconf->storageapi)
        return FALSE;

    if (ARTnumbers[i].Token.cancelled)
        return TRUE;
    if (ARTnumbers[i].Tokenretrieved) {
        if (ARTnumbers[i].Token.type == TOKEN_EMPTY)
            return TRUE;
        return FALSE;
    }

    if ((artnum && (ARTnumbers[i].ArtNum != artnum)) || !ARTnumbers[i].Index)
        return ARTnumbers[i].Token.cancelled = TRUE;
    
    UnpackOverIndex(*(ARTnumbers[i].Index), &index);
    if (artnum && (index.artnum != artnum))
        return ARTnumbers[i].Token.cancelled = TRUE;
    
#ifndef DO_TAGGED_HASH
    if (innconf->extendeddbz) {
        if (!OVERgetent(&index.hash, &ARTnumbers[i].Token))
            return ARTnumbers[i].Token.cancelled = TRUE;
    } else {
        if (HISgetent(&index.hash, FALSE, &ARTnumbers[i].Offset) == (char *)NULL)
            return ARTnumbers[i].Token.cancelled = TRUE;
    }
    if (innconf->extendeddbz)
	return FALSE;
#else
    /* need to call HISgetent here so ARTnumbers[i].Offset will be set for
       code below. */
    if (HISgetent(&index.hash, FALSE, &ARTnumbers[i].Offset) == (char *)NULL)
            return ARTnumbers[i].Token.cancelled = TRUE;
#endif

    ARTnumbers[i].Tokenretrieved = TRUE;
    if ((tokentext = HISgetent(&index.hash, TRUE, &ARTnumbers[i].Offset)) == (char *)NULL)
      return ARTnumbers[i].Token.cancelled = TRUE;
    
    ARTnumbers[i].Token = TextToToken(tokentext);

    return FALSE;
}
/*
**  Send a (part of) a file to stdout, doing newline and dot conversion.
*/
STATIC void ARTsendmmap(SENDTYPE what)
{
    char		*p, *q;
    struct timeval	stv, etv;
    long		bytecount;
    char		lastchar;
    int			InBody;
    int			FirstLine;

    ARTcount++;
    GRParticles++;
    bytecount = 0;
    InBody = 0;
    lastchar = -1;
    FirstLine = 1;

     gettimeofday(&stv, NULL);
    /* Get the headers and detect if wire format. */
    for (q = p = ARTmem; p < (ARTmem + ARTlen); p++) {
        if (*p == '\r') {
            if (FirstLine) {
                WireFormat = TRUE;
                if (what == STarticle) {
                    p = ARTmem + ARTlen;
                    break;
                    
                }
            }
            continue;
        }
        if (*p == '\n') {
            if (FirstLine)
                FirstLine = 0;
            if ((lastchar == '\n') && !InBody)
                if (what == SThead) {
                    if (*(p-1) == '\r')
                        p--;
                    break;
                } else {
                    if (WireFormat) {
                        q = p + 1;
                        p = ARTmem + ARTlen;
                        break;
                    } else {
                        InBody = 1;
                        if (what == STbody) {
                            q = ++p;
                            continue;
                        }
                    }
                }
            if (((what == STarticle) || 
                    ((what == SThead) && !InBody) || 
                    ((what == STbody) && InBody)) && 
                    !WireFormat) {
                if (*q == '.')
                    SendIOb(".", 1);
                SendIOb(q, p - q);
                SendIOb("\r\n", 2);
                ARTgetsize += p - q + 2;
                q = p + 1;
            }
        }
        lastchar = *p;
    }
    
    if (!WireFormat) {
        SendIOb(".\r\n", 3);
        PushIOb();
        ARTgetsize += 3;
    } else {
       SendIOv(q, p - q);
       ARTgetsize += p - q;
       if (what == SThead) {
           SendIOv(".\r\n", 3);
           ARTgetsize += 3;
       } else if (memcmp((ARTmem+ARTlen-5), "\r\n.\r\n", 5)) {
           if (memcmp((ARTmem+ARTlen-2), "\r\n", 2)) {
               SendIOv("\r\n.\r\n", 5);
               ARTgetsize += 5;
		   } else {
               SendIOv(".\r\n", 3);
               ARTgetsize += 3;
		   }
       }
       PushIOv();
    }
    
    gettimeofday(&etv, NULL);
    ARTget++;
    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
}

STATIC void ARTsendqio(SENDTYPE what)
{
    char		*p;
    struct timeval	stv, etv;
    long		bytecount;

    ARTcount++;
    GRParticles++;
    bytecount = 0;

    gettimeofday(&stv, NULL);
    /* Get the headers. */
    QIOrewind(ARTqp);
    for ( ; ; ) {
	p = QIOread(ARTqp);
	if (p == NULL) {
	    if (QIOtoolong(ARTqp))
		continue;
	    break;
	}
	if (*p == '\0')
	    break;
	if (what == STbody)
	    continue;
	ARTgetsize+=Printf("%s%s\r\n", *p == '.' ? "." : "", p);
    }
    if (what == SThead) {
	Printf(".\r\n");
	ARTgetsize+=3;
        gettimeofday(&etv, NULL);
        ARTget++;
        ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
        ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
	return;
    }

    if (what == STarticle)
	ARTgetsize+=Printf("\r\n");
    for ( ; ; ) {
	p = QIOread(ARTqp);
	if (p == NULL) {
	    if (QIOtoolong(ARTqp))
		continue;
	    break;
	}
	ARTgetsize+=Printf("%s%s\r\n", *p == '.' ? "." : "", p);
    }
    ARTgetsize+=Printf(".\r\n");
    gettimeofday(&etv, NULL);
    ARTget++;
    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
}


/*
**  Return the header from the specified file, or NULL if not found.
**  We can estimate the Lines header, if that's what's wanted.
*/
char *GetHeader(char *header, BOOL IsLines)
{
    static char		buff[40];
    char		*p;
    char		*q;
    /* Bogus value here to make sure that it isn't initialized to \n */
    char		lastchar = ' ';
    char		*limit;
    static char		*retval = NULL;
    static int		retlen = 0;

    if (ARTmem) {
	limit = ARTmem + ARTlen - strlen(header);
	for (p = ARTmem; p < limit; p++) {
	    if (*p == '\r')
		continue;
	    if ((lastchar == '\n') && (*p == '\n')) {
		return NULL;
	    }
	    if ((lastchar == '\n') || (p == ARTmem)) {
		if (!strncasecmp(p, header, strlen(header))) {
		    for (; (p < limit) && !isspace(*p) ; p++);
		    for (; (p < limit) && isspace(*p) ; p++);
		    for (q = p; q < limit; q++) 
			if ((*q == '\r') || (*q == '\n'))
			    break;
		    if (retval == NULL) {
			retval = NEW(char, q - p + 1);
		    } else {
			if ((q - p +1) > retlen) {
			    DISPOSE(retval);
			    retval = NEW(char, q - p + 1);
			}
		    }
		    retlen = q - p + 1;
		    memcpy(retval, p, retlen - 1);
		    *(retval + retlen - 1) = '\0';
		    return retval;
		}
	    }
	    lastchar = *p;
	}
    } else {
	for ( ; ; ) {
	    if ((p = QIOread(ARTqp)) == NULL) {
		if (QIOtoolong(ARTqp))
		    continue;
		break;
	    }
	    if (*p == '\0')
		/* End of headers. */
		break;
	    if (ISWHITE(*p) || (q = strchr(p, ':')) == NULL)
		/* Continuation or bogus (shouldn't happen) line; ignore. */
		continue;
	    *q = '\0';
	    if (caseEQ(header, p))
		return *++q ? q + 1 : NULL;
	}
    }

    if (IsLines) {
	/* Lines estimation taken from Tor Lillqvist <tml@tik.vtt.fi>'s
	 * posting <TML.92Jul10031233@hemuli.tik.vtt.fi> in
	 * news.sysadmin. */
	(void)sprintf(buff, "%d",
	    (int)(6.4e-8 * ARTlen * ARTlen + 0.023 * ARTlen - 12));
	return buff;
    }
    return NULL;
}

/*
**  Fetch part or all of an article and send it to the client.
*/
FUNCTYPE CMDfetch(int ac, char *av[])
{
    char		buff[SMBUF];
    SENDDATA		*what;
    BOOL		ok;
    ARTNUM		art;
    char		*msgid;
    ARTNUM		tart;

    /* Find what to send; get permissions. */
    ok = PERMcanread;
    switch (*av[0]) {
    default:
	what = &SENDbody;
	break;
    case 'a': case 'A':
	what = &SENDarticle;
	break;
    case 's': case 'S':
	what = &SENDstat;
	break;
    case 'h': case 'H':
	what = &SENDhead;
	/* Poster might do a "head" command to verify the article. */
	ok = PERMcanread || PERMcanpost;
	break;
    }

    if (!ok) {
	Reply("%s\r\n", NOACCESS);
	return;
    }

    /* Requesting by Message-ID? */
    if (ac == 2 && av[1][0] == '<') {
	if (!ARTopenbyid(av[1], &art)) {
	    Reply("%d No such article\r\n", NNTP_DONTHAVEIT_VAL);
	    return;
	}
	if (!PERMartok()) {
	    ARTclose();
	    Reply("%s\r\n", NOACCESS);
	    return;
	}
	tart=art;
	Reply("%d %ld %s %s\r\n", what->ReplyCode, art, what->Item, av[1]);
	if (what->Type != STstat) {
	    if (ARTmem) 
		ARTsendmmap(what->Type);
	    else
		ARTsendqio(what->Type);
    	    if (tart != 0 && RARTenable && RARTcount < ART_MAX)
	        RARTtable[RARTcount++]=tart;
	}
	ARTclose();
	return;
    }

    /* Trying to read. */
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }

    /* Default is to get current article, or specified article. */
    if (ac == 1) {
	if (ARTindex < 0 || ARTindex >= ARTsize) {
	    Reply("%s\r\n", ARTnocurrart);
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumbers[ARTindex].ArtNum);
	tart=ARTnumbers[ARTindex].ArtNum;
    }
    else {
	if (strspn(av[1], "0123456789") != strlen(av[1])) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	(void)strcpy(buff, av[1]);
	tart=(ARTNUM)atol(buff);
    }

    /* Move forward until we can find one. */
    while (!ARTopen(buff)) {
	if (ac > 1 || ++ARTindex >= ARTsize) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumbers[ARTindex].ArtNum);
	tart=ARTnumbers[ARTindex].ArtNum;
    }
    if ((msgid = GetHeader("Message-ID", FALSE)) == NULL) {
        Reply("%s\r\n", ARTnoartingroup);
	return;
    }
    Reply("%d %s %.512s %s\r\n", what->ReplyCode, buff, msgid, what->Item); 
    if (what->Type != STstat) {
	if (ARTmem)
	    ARTsendmmap(what->Type);
	else
	    ARTsendqio(what->Type);
    	    if (tart != 0 && RARTenable && RARTcount < ART_MAX)
	        RARTtable[RARTcount++]=tart;
    }
    if (ac > 1)
	ARTindex = ARTfind((ARTNUM)atol(buff), FALSE);
}


/*
**  Go to the next or last (really previous) article in the group.
*/
FUNCTYPE CMDnextlast(int ac, char *av[])
{
    char	buff[SPOOLNAMEBUFF];
    char	*msgid;
    int		save;
    BOOL	next;
    int		delta;
    int		errcode;
    STRING	message;

    if (!PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }
    if (ARTindex < 0 || ARTindex >= ARTsize) {
	Reply("%s\r\n", ARTnocurrart);
	return;
    }

    next = (av[0][0] == 'n' || av[0][0] == 'N');
    if (next) {
	delta = 1;
	errcode = NNTP_NONEXT_VAL;
	message = "next";
    }
    else {
	delta = -1;
	errcode = NNTP_NOPREV_VAL;
	message = "previous";
    }

    save = ARTindex;
    ARTindex += delta;
    if (ARTindex < 0 || ARTindex >= ARTsize) {
	Reply("%d No %s to retrieve.\r\n", errcode, message);
	ARTindex = save;
	return;
    }

    (void)sprintf(buff, "%ld", ARTnumbers[ARTindex].ArtNum);
    while (!ARTopen(buff)) {
	ARTindex += delta;
	if (ARTindex < 0 || ARTindex >= ARTsize) {
	    Reply("%d No %s article to retrieve.\r\n", errcode, message);
	    ARTindex = save;
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumbers[ARTindex].ArtNum);
    }

    if ((msgid = GetHeader("Message-ID", FALSE)) == NULL) {
        Reply("%s\r\n", ARTnoartingroup);
        return;
    }

    Reply("%d %s %s Article retrieved; request text separately.\r\n",
	   NNTP_NOTHING_FOLLOWS_VAL, buff, msgid);

    if (ac > 1)
	ARTindex = ARTfind((ARTNUM)atol(buff), FALSE);
}


STATIC BOOL CMDgetrange(int ac, char *av[], ARTRANGE *rp, BOOL *DidReply)
{
    char		*p, *tokentext;
    OVERINDEX		index;
    int			i;
    ARTNUM		artnum;

    *DidReply = FALSE;
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	*DidReply = TRUE;
	return FALSE;
    }

    if (ac == 1) {
	/* No argument, do only current article. */
	if (ARTindex < 0 || ARTindex >= ARTsize) {
	    Reply("%s\r\n", ARTnocurrart);
	    *DidReply = TRUE;
	    return FALSE;
	}
	rp->High = rp->Low = ARTnumbers[ARTindex].ArtNum;
        return !IsCancelled(rp->High);
    }

    /* Got just a single number? */
    if ((p = strchr(av[1], '-')) == NULL) {
	rp->Low = rp->High = atol(av[1]);
        return !IsCancelled(rp->Low);
    }

    /* Parse range. */
    *p++ = '\0';
    rp->Low = atol(av[1]);
    if (ARTsize) {
	if (*p == '\0' || (rp->High = atol(p)) < rp->Low)
	    /* "XHDR 234-0 header" gives everything to the end. */
	    rp->High = ARTnumbers[ARTsize - 1].ArtNum;
	else if (rp->High > ARTnumbers[ARTsize - 1].ArtNum)
	    rp->High = ARTnumbers[ARTsize - 1].ArtNum;
	if (rp->Low < ARTnumbers[0].ArtNum)
	    rp->Low = ARTnumbers[0].ArtNum;
    }
    else
	/* No articles; make sure loops don't run. */
	rp->High = rp->Low ? rp->Low - 1 : 0;

    return TRUE;
}


/*
**  Return a field from the overview line or NULL on error.  Return a copy
**  since we might be re-using the line later.
*/
STATIC char *OVERGetHeader(char *p, int field)
{
    static char		*buff;
    static int		buffsize;
    int	                i;
    ARTOVERFIELD	*fp;
    char		*next;

    fp = &ARTfields[field - 1];

    if (fp->NeedsHeader) 		/* we're going to need an exact match */
      field = ARTfirstfullfield;

    if (innconf->storageapi)
	field--;
    /* Skip leading headers. */
    for (; --field >= 0 && *p; p++)
	if ((p = strchr(p, '\t')) == NULL)
	    return NULL;
    if (*p == '\0')
	return NULL;

    if (fp->HasHeader)
        p += fp->Length + 2;

    if (fp->NeedsHeader) {		/* find an exact match */
	 while (strncmp(fp->Header, p, fp->Length) != 0) {
	      if ((p = strchr(p, '\t')) == NULL) 
		return NULL;
	      p++;
	 }
	 p += fp->Length + 2;
    }

    /* Figure out length; get space. */
    if ((next = strchr(p, '\t')) != NULL)
	i = next - p;
    else
	i = strlen(p);
    if (buffsize == 0) {
	buffsize = i;
	buff = NEW(char, buffsize + 1);
    }
    else if (buffsize < i) {
	buffsize = i;
	RENEW(buff, char, buffsize + 1);
    }

    (void)strncpy(buff, p, i);
    buff[i] = '\0';
    return buff;
}


/*
**  Open an OVERVIEW file.
*/
STATIC BOOL OVERopen(void)
{
    char	name[SPOOLNAMEBUFF];
    int		fd;
    struct stat	sb;

    /* Already open? */
    if ((OVERmem != NULL) || (OVERfp != NULL))
	return TRUE;

    if (innconf->storageapi) {
	if (!OVERinit()) {
	    syslog(L_ERROR, "%s cant initialize unified overview", ClientHost);
	    return FALSE;
	}
    } else {
	/* Failed here before? */
	if (OVERopens++)
	    return FALSE;

	OVERarticle = 0;
	(void)sprintf(name, "%s/%s/%s", innconf->pathoverview, GRPlast,
						innconf->overviewname);
	if (innconf->overviewmmap) {
	    if ((fd = open(name, O_RDONLY)) < 0)
		return FALSE;
	    if (fstat(fd, &sb) != 0) {
		close(fd);
		return FALSE;
	    }
	    OVERlen = sb.st_size;
	    if (OVERlen > 0) {
		if ((OVERmem = (char *)mmap(0, OVERlen, PROT_READ, MAP_SHARED, fd, 0)) == (MMAP_PTR)-1) {
		    OVERmem = NULL;
		    close(fd);
		    return FALSE;
		}
		else
			OVERmempos = OVERmem;
	    } else {
		OVERmem = NULL;
	    }
	    close(fd);
	} else {
	    if ((OVERfp = fopen(name, "r")) == NULL)
		return FALSE;
	}
    }
    return TRUE;
}


/*
**  Close the OVERVIEW file.
*/
void OVERclose(void)
{
    int	i;
    if (innconf->storageapi) {
	(void)OVERshutdown();
    } else {
	if (OVERmem != NULL) {
    	    munmap(OVERmem, OVERlen);
    	    OVERmem = NULL;
    	    OVERopens = 0;
    	    OVERlen = 0;
	}
	if (OVERfp != NULL) {
	    fclose(OVERfp);
	    OVERfp = NULL;
	    OVERopens = 0;
	}
    }
}


/*
**  Return the overview data for an article or NULL on failure.
**  Assumes that what we return is never modified.
*/
STATIC char *OVERfind(ARTNUM artnum, int *linelen)
{
    int		        i;
    char   	        *q;
    OVERINDEX           index;
    STATIC char         *OVERline = NULL;
    char		*tokentext, *nextline;
    TOKEN		token;

    if (innconf->storageapi) {
	if ((i = ARTfind(artnum, FALSE)) < 0)
	    return NULL;
	if ((ARTnumbers[i].Tokenretrieved && (ARTnumbers[i].Token.type == TOKEN_EMPTY)) || ARTnumbers[i].Token.index == OVER_NONE || ARTnumbers[i].Token.cancelled)
	    return NULL;
	if ((OVERline = OVERretrieve(&ARTnumbers[i].Token, linelen)) != (char *)NULL)
	    OVERread += *linelen;
	return OVERline;
    } else {
	if (!OVERmem && !OVERfp)
	    return NULL;

	i = ARTfind(artnum, FALSE);
	if (i < 0 || ARTnumbers[i].ArtNum != artnum)
	    return NULL;
	if (OVERmem) {
	    for (nextline = (OVERarticle > artnum)? OVERmem:OVERmempos; nextline < OVERmem + OVERlen; nextline++) {
		OVERmempos=nextline;
		OVERarticle = atol(nextline);
		if (!(OVERarticle < artnum)) {
		    OVERline = nextline;
		    break;
		}
		OVERline = nextline;
		if ((nextline = memchr(nextline, '\n', OVERmem + OVERlen - nextline)) == (char *)NULL)
		    return NULL;
	    }
	    if (OVERarticle == artnum && (nextline = memchr(OVERline, '\n', OVERmem + OVERlen - OVERline)) != (char *)NULL) {
		*linelen = nextline - OVERline;
		OVERread += *linelen;
		return OVERline;
	    }
	    return NULL;
	}
	if (OVERline == (char *)NULL)
	    OVERline = NEW(char, MAXOVERLINE);
	if (OVERarticle > artnum) {
	    rewind(OVERfp);
	    OVERarticle = 0;
	    OVERline[0] = '\0';
	}
	for ( ; OVERarticle < artnum; OVERarticle = atol(OVERline)) {
	    if (fgets(OVERline, MAXOVERLINE, OVERfp) == NULL)
		 return NULL;
	    OVERread += strlen(OVERline);
	    while ((strlen(OVERline) == MAXOVERLINE) && (OVERline[MAXOVERLINE-1] == '\n'))
		if (fgets(OVERline, MAXOVERLINE, OVERfp) == NULL)
		     return NULL;
		else
		    OVERread += strlen(OVERline);
	}

	if (OVERarticle == artnum) {
	    *linelen = strlen(OVERline) - 1;
	    OVERline[*linelen] = '\0';
	    return OVERline;
	}
	return NULL;
    }
}


/*
**  Read an article and create an overview line without the trailing
**  newline.  Returns pointer to static space or NULL on error.
*/
STATIC char *OVERgen(char *name)
{
    static ARTOVERFIELD		*Headers;
    static char			*buff;
    static int			buffsize;
    ARTOVERFIELD	        *fp;
    ARTOVERFIELD	        *hp;
    QIOSTATE		        *qp;
    char		        *colon;
    char		        *line;
    char		        *p;
    int		                i;
    int		                size;
    int		                ov_size;
    long		        lines;
    struct stat			Sb;
    long			t;
    char			value[10];

    lines = 0;
    /* Open article. */
    if ((qp = QIOopen(name)) == NULL)
	return NULL;
    if ((p = strrchr(name, '/')) != NULL)
	name = p + 1;

    /* Set up place to store headers. */
    if (Headers == NULL) {
	Headers = NEW(ARTOVERFIELD, ARTfieldsize);
	for (fp = ARTfields, hp = Headers, i = ARTfieldsize; --i >= 0; hp++, fp++) {
	    hp->Length = 0;
	    hp->NeedsHeader = fp->NeedsHeader;
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

       /* Is it a continuation line? */
       if (ISWHITE(*line) && (hp - Headers) < ARTfieldsize) {
           /* Skip whitespace but one. */
           for (p = line; *p && ISWHITE(*p); p++)
               continue;
           --p;
           /* Now append it. */
           hp->Length += strlen(p);
           RENEW(hp->Header, char, hp->Length + 1);
           (void)strcat(hp->Header, p);
           for (p = hp->Header; *p; p++)
               if (*p == '\t' || *p == '\n')
                   *p = ' ';
           continue;
       }

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
	    if (fp->NeedsHeader)
		p = line;
	    else
		/* Skip colon and whitespace, store value. */
		for (p = colon; *++p && ISWHITE(*p); )
		    continue;
	    size = strlen(p);
	    if (hp->Length == 0) {
		hp->Length = size;
		hp->Header = NEW(char, hp->Length + 1);
	    }
	    else if (hp->Length < size) {
		hp->Length = size;
		RENEW(hp->Header, char, hp->Length + 1);
	    }
	    (void)strcpy(hp->Header, p);
	    for (p = hp->Header; *p; p++)
		if (*p == '\t' || *p == '\n')
		    *p = ' ';
	    if (!strncmp(line, "Lines", 5)) {
	    	lines = atoi(hp->Header);
	    }
	    hp->HasHeader = TRUE;
	    break;
	}
    }

    /* Read body of article, just to get lines. */
    if (!lines)
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
    ov_size = strlen(name) + ARTfieldsize + 2;
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
    if (buffsize == 0) {
	buffsize = ov_size;
	buff = NEW(char, buffsize + 1);
    }
    else if (buffsize < ov_size) {
	buffsize = ov_size;
	RENEW(buff, char, buffsize + 1);
    }

    /* Glue all the fields together. */
    p = buff + strlen(strcpy(buff, name));
    for (hp = Headers, i = ARTfieldsize; --i >= 0; hp++) {
	 if (hp->NeedsHeader && !hp->HasHeader)
	   continue;
	*p++ = '\t';
	if (hp->HasHeader)
	    p += strlen(strcpy(p, hp->Header));
	    if (!strncmp(line, "Lines", 5)) {
	    	lines = atoi(hp->Header);
	    }
    }
    *p = '\0';

    QIOclose(qp);
    return buff;
}

/*
**  XHDR, a common extension.  Retrieve specified header from a
**  Message-ID or article range.
*/
FUNCTYPE CMDxhdr(int ac, char *av[])
{
    int		        i;
    char	        *p;
    int			Overview;
    BOOL		IsLines;
    BOOL		DidReply;
    ARTRANGE		range;
    char		buff[SPOOLNAMEBUFF];
    ARTNUM		art;
    int			linelen;

    if (!PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }
    IsLines = caseEQ(av[1], "lines");

    /* Message-ID specified? */
    if (ac == 3 && av[2][0] == '<') {
	if (!ARTopenbyid(av[2], &art)) {
	    Reply("%d No such article\r\n", NNTP_DONTHAVEIT_VAL);
	    return;
	}
	Reply("%d %ld %s header of article %s.\r\n",
	   NNTP_HEAD_FOLLOWS_VAL, art, av[1], av[2]);
	p = GetHeader(av[1], IsLines);
	Printf("%s %s\r\n", av[2], p ? p : "(none)");
	ARTclose();
	Printf(".\r\n");
	return;
    }

    /* Range specified. */
    if (!CMDgetrange(ac - 1, av + 1, &range, &DidReply)) {
	if (!DidReply) {
	    Reply("%d %s fields follow\r\n", NNTP_HEAD_FOLLOWS_VAL,
		av[1] ? av[1] : "\"\"");
	    Printf(".\r\n");
	}
	return;
    }

    /* Is this a header in our overview? */
    for (Overview = 0, i = 0; i < ARTfieldsize; i++)
	if (caseEQ(ARTfields[i].Header, av[1])) {
	    if (OVERopen())
		Overview = i + 1;
	    break;
	}

    Reply("%d %s fields follow\r\n", NNTP_HEAD_FOLLOWS_VAL, av[1]);
    for (i = range.Low; i <= range.High && range.High > 0; i++) {
	if (ARTfind(i, innconf->storageapi && innconf->nnrpdcheckart) < 0)
	    continue;

	/* Get it from the overview? */
	if (Overview && !IsCancelled(i) && (p = OVERfind(i, &linelen)) != NULL) {
	    p = OVERGetHeader(p, Overview);
	    Printf("%ld %s\r\n", i, p && *p ? p : "(none)");
	    continue;
	}

	(void)sprintf(buff, "%ld", i);
	if (!ARTopen(buff))
	    continue;
	p = GetHeader(av[1], IsLines);
	Printf("%ld %s\r\n", i, p ? p : "(none)");
	ARTclose();
    }
    Printf(".\r\n");
    OVERclose();
}


/*
**  XOVER another extension.  Dump parts of the overview database.
*/
FUNCTYPE CMDxover(int ac, char *av[])
{
    char	        *p;
    ARTNUM	        i;
    BOOL	        Opened;
    BOOL	        DidReply;
    ARTRANGE		range;
    char		buff[SPOOLNAMEBUFF];
    struct timeval	stv, etv;
    int			linelen;

    if (!PERMcanread) {
	Printf("%s\r\n", NOACCESS);
	return;
    }

    /* Trying to read. */
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }

    /* Parse range. */
    gettimeofday(&stv, NULL);
    if (!CMDgetrange(ac, av, &range, &DidReply)) {
	if (!DidReply) {
	    Reply("%d data follows\r\n", NNTP_OVERVIEW_FOLLOWS_VAL);
	    Printf(".\r\n");
	}
	return;
    }

    OVERcount++;
    Reply("%d data follows\r\n", NNTP_OVERVIEW_FOLLOWS_VAL);
    for (Opened = OVERopen(), i = range.Low; i <= range.High && range.High > 0; i++) {
	if (IsCancelled(i)) {
	    if (innconf->storageapi)
		OVERmiss++;
	    continue;
	}

	if (Opened && (p = OVERfind(i, &linelen)) != NULL) {
	    OVERhit++;
	    OVERsize+=linelen;
	    if ((innconf->storageapi && innconf->overviewmmap) || OVERmem) {
		if (innconf->storageapi && innconf->overviewmmap) {
		    (void)sprintf(buff, "%ld\t", i);
		    SendIOb(buff, strlen(buff));
		}
		SendIOb(p, linelen);
		SendIOb("\r\n", 2);
	    } else {
	        if (innconf->storageapi) {
		    Printf("%ld\t%s\r\n", i, p);
		} else {
		    Printf("%s\r\n", p);
		}
	    }
	    continue;
	}
	if (innconf->storageapi)
	    continue;

	/* This happens with traditional spool */
	(void)sprintf(buff, "%ld", i);
	if ((p = OVERgen(buff)) != NULL) {
	    OVERmiss++;
	    linelen = strlen(p);
	    OVERsize+=linelen;
	    if (OVERmem) {
                SendIOb(p, linelen);
                SendIOb("\r\n", 2);
	    } else {
                Printf("%s\r\n", p);
	    }
	}
    }
    if ((innconf->storageapi && innconf->overviewmmap) || OVERmem) {
	SendIOb(".\r\n", 3);
	PushIOb(); 
    } else {
	Printf(".\r\n");
    }
    OVERclose();
    gettimeofday(&etv, NULL);
    OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
    OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;

}


/*
**  XPAT, an uncommon extension.  Print only headers that match the pattern.
*/
/* ARGSUSED */
FUNCTYPE CMDxpat(int ac, char *av[])
{
    char	        *p;
    int	        	i;
    ARTRANGE		range;
    BOOL		DidReply;
    char		*header;
    char		*pattern;
    char		*text;
    int			Overview;
    char		buff[SPOOLNAMEBUFF];
    ARTNUM		art;
    int			linelen;

    if (!PERMcanread) {
	Printf("%s\r\n", NOACCESS);
	return;
    }

    header = av[1];

    /* Message-ID specified? */
    if (av[2][0] == '<') {
	p = av[2];
	if (!ARTopenbyid(p, &art)) {
	    Printf("%d No such article.\r\n", NNTP_DONTHAVEIT_VAL);
	    return;
	}

	Printf("%d %s matches follow.\r\n", NNTP_HEAD_FOLLOWS_VAL, header);
	pattern = Glom(&av[3]);
	if ((text = GetHeader(header, FALSE)) != NULL
	 && wildmat(text, pattern))
	    Printf("%s %s\r\n", p, text);

	ARTclose();
	Printf(".\r\n");
	DISPOSE(pattern);
	return;
    }

    /* Range specified. */
    if (!CMDgetrange(ac - 1, av + 1, &range, &DidReply)) {
	if (!DidReply) {
	    Reply("%d %s matches follow\r\n", NNTP_HEAD_FOLLOWS_VAL,
		av[1] ? av[1] : "\"\"");
	    Printf(".\r\n");
	}
	return;
    }

    /* In overview? */
    for (Overview = 0, i = 0; i < ARTfieldsize; i++)
	if (caseEQ(ARTfields[i].Header, av[1])) {
	    if (OVERopen())
		Overview = i + 1;
	    break;
	}

    Printf("%d %s matches follow.\r\n", NNTP_HEAD_FOLLOWS_VAL, header);
    for (pattern = Glom(&av[3]), i = range.Low; i <= range.High && range.High > 0; i++) {
	if (ARTfind(i, innconf->storageapi && innconf->nnrpdcheckart) < 0)
	    continue;

	/* Get it from the Overview? */
	if (Overview
         && !IsCancelled(i)
	 && (p = OVERfind(i, &linelen)) != NULL
	 && (p = OVERGetHeader(p, Overview)) != NULL) {
	    if (wildmat(p, pattern))
		Printf("%ld %s\r\n", i, p);
	    continue;
	}

	(void)sprintf(buff, "%ld", i);
	
	if (!ARTopen(buff))
	    continue;
	if ((p = GetHeader(av[1], FALSE)) == NULL)
	    p = "(none)";
	if (wildmat(p, pattern))
	    Printf("%ld %s\r\n", i, p);
	ARTclose();
    }

    Printf(".\r\n");
    DISPOSE(pattern);
    OVERclose();
}
