/*  $Revision$
**
**  Article-related routines.
*/
#include <stdio.h>
#include <sys/types.h>
#include "configdata.h"
#include "clibrary.h"
#include "nnrpd.h"

#ifdef __FreeBSD__
#include <osreldate.h>
#endif



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
#ifdef ART_MMAP
STATIC char             *ARTmem;
STATIC int		WireFormat = 0;
#else
STATIC QIOSTATE		*ARTqp;
#endif
STATIC int              ARTlen;
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


/*
**  Overview state information.
*/
#ifdef OVER_MMAP
STATIC char *		OVERmem = NULL;		/* mmaped overview file */
STATIC int		OVERlen;		/* Length of the overview file */
#else
#define MAXOVERLINE	4096
STATIC FILE		*OVERfp = NULL;		/* Open overview file	*/
STATIC char		OVERline[MAXOVERLINE];	/* Current line		*/
STATIC int		OVERoffset;		/* Current offset */
STATIC char		OVERline[MAXOVERLINE];	/* Current line		*/
#endif
STATIC ARTNUM		OVERarticle;		/* Current article	*/
STATIC int		OVERopens;		/* Number of opens done	*/
STATIC OVERINDEX	*OVERindex;		/* Array of the overview index */
STATIC int		OVERicount;		/* Number of index entries */
STATIC int		OVERioff;		/* Current index pointer */

#if defined(OVER_MMAP) || defined(ART_MMAP)
#include <sys/uio.h>

#if defined(UIO_MAXIOV) && !defined(IOV_MAX)
#define IOV_MAX		UIO_MAXIOV
#endif

#ifndef IOV_MAX
/* Solaris */
#if defined(sun) && defined(__svr4__)
#define IOV_MAX 16
#endif
/* FreeBSD 3.0 or above */
#if defined(__FreeBSD__) && (__FreeBSD_version >= 222000)
#define	IOV_MAX	1024
#endif

#endif

STATIC struct iovec	iov[IOV_MAX];
STATIC int		queued_iov = 0;

BOOL PushIOv(void) {
    fflush(stdout);
    if (writev(STDOUT_FILENO, iov, queued_iov) <= 0) {
        queued_iov = 0;
	return FALSE;
    }
    queued_iov = 0;
    return TRUE;
}

BOOL SendIOv(void *p, int len) {
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

BOOL SendIOb(void *p, int len) {
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
}

#endif

/*
**  Read the overview schema.
*/
void
ARTreadschema()
{
    static char			SCHEMA[] = _PATH_SCHEMA;
    FILE			*F;
    char			*p;
    ARTOVERFIELD		*fp;
    int				i;
    char			buff[SMBUF];

    /* Open file, count lines. */
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
void
ARTclose()
{
#ifdef ART_MMAP
    if (ARTmem) {
#if defined(MC_ADVISE) && defined(MADV_DONTNEED)
        madvise(ARTmem, ARTlen, MADV_DONTNEED);
#endif
        munmap(ARTmem, ARTlen);
        ARTmem = NULL;
        WireFormat = 0;
    }
#else
    if (ARTqp) {
	QIOclose(ARTqp);
	ARTqp = NULL;
    }
#endif
}

/*
**  If the article name is valid, open it and stuff in the ID.
*/
STATIC BOOL
ARTopen(name)
    char		*name;
{
    static ARTNUM	save_artnum;
    struct stat		Sb;
#ifdef ART_MMAP
    int			fd;

    /* Re-use article if it's the same one. */
    if (ARTmem != NULL) {
        if (save_artnum == atol(name))
#else
    if (ARTqp != NULL) {
	if (save_artnum == atol(name) && QIOrewind(ARTqp) != -1)
#endif
            return TRUE;
        ARTclose();
    }

    /* Open it, make sure it's a regular file. */
#ifdef ART_MMAP
    if ((fd = open(name, O_RDONLY)) < 0)
        return FALSE;
    if ((fstat(fd, &Sb) < 0) || !S_ISREG(Sb.st_mode)) {
    	close(fd);
    	return FALSE;
    }
    ARTlen = Sb.st_size;
    if ((int)(ARTmem = mmap(0, ARTlen, PROT_READ, MAP_SHARED, fd, 0)) == -1) {
    	close(fd);
    	return FALSE;
    }
    close(fd);
#else
    if ((ARTqp = QIOopen(name, QIO_BUFFER)) == NULL)
	return FALSE;
    if (fstat(QIOfileno(ARTqp), &Sb) < 0 || !S_ISREG(Sb.st_mode)) {
	ARTclose();
	return FALSE;
    }
    ARTlen = Sb.st_size;
    CloseOnExec(QIOfileno(ARTqp), TRUE);
#endif

    save_artnum = atol(name);
    return TRUE;
}


/*
**  Open the article for a given Message-ID.
*/
STATIC BOOL
ARTopenbyid(msg_id, ap)
    char		*msg_id;
    ARTNUM		*ap;
{
    char		*p;
    char		*q;
    struct stat		Sb;
#ifdef ART_MMAP
    int			fd;
#endif

    *ap = 0;
    if ((p = HISgetent(msg_id, FALSE)) == NULL)
	return FALSE;
#ifdef ART_MMAP
    if ((fd = open(p, O_RDONLY)) < 0)
        return FALSE;
    if ((fstat(fd, &Sb) < 0) || !S_ISREG(Sb.st_mode)) {
    	close(fd);
    	return FALSE;
    }
    ARTlen = Sb.st_size;
    if ((int)(ARTmem = mmap(0, ARTlen, PROT_READ, MAP_SHARED, fd, 0)) == -1) {
    	close(fd);
    	return FALSE;
    }
    close(fd);
#else
    if ((ARTqp = QIOopen(p, QIO_BUFFER)) == NULL)
	return FALSE;
    if (fstat(QIOfileno(ARTqp), &Sb) < 0 || !S_ISREG(Sb.st_mode)) {
	ARTclose();
	return FALSE;
    }
    CloseOnExec(QIOfileno(ARTqp), TRUE);
#endif
    p += strlen(_PATH_SPOOL) + 1;
    if ((q = strrchr(p, '/')) != NULL)
	*q++ = '\0';
    if (GRPlast[0] && EQ(p, GRPlast))
	*ap = atol(q);
    return TRUE;
}


/*
**  Send a (part of) a file to stdout, doing newline and dot conversion.
*/
#ifdef ART_MMAP
STATIC void
ARTsend(what)
    SENDTYPE		what;
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
                WireFormat = 1;
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
       if (what == SThead) {
           SendIOv(".\r\n", 3);
           ARTgetsize += 3;
       }
       ARTgetsize += p - q;
       if (memcmp((ARTmem+ARTlen-3), ".\r\n", 3)) {
           SendIOv("\r\n.\r\n", 5);
           ARTgetsize += 5;
       }
       PushIOv();
    }
    
    gettimeofday(&etv, NULL);
    ARTget++;
    ARTgettime+=(etv.tv_sec - stv.tv_sec) * 1000;
    ARTgettime+=(etv.tv_usec - stv.tv_usec) / 1000;
}
#else
STATIC void
ARTsend(what)
    SENDTYPE		what;
{
    char		*p, *q;
    struct timeval	stv, etv;
    long		bytecount;
    char		lastchar;

    ARTcount++;
    GRParticles++;
    bytecount = 0;

#if defined (DONT_LIKE_PULLERS)
    {
      static int count ;

      count++ ;

      if (what == STarticle || count > 100)
        sleep (1) ;               /* slow down sucking readers */
    }
#endif

    gettimeofday(&stv, NULL);
    /* Get the headers. */
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
#endif /* ART_MMAP */

/*
**  Find an article number in the article array via a binary search;
**  return -1 if not found.  Cache last hit to make linear lookups
**  faster.
*/
STATIC int
ARTfind(i)
    ARTNUM		i;
{
    ARTNUM		*bottom;
    ARTNUM		*middle;
    ARTNUM		*top;

    if (ARTsize == 0)
	return -1;

    top = &ARTnumbers[ARTsize - 1];
    if (ARTcache && ++ARTcache <= top && *ARTcache <= i) {
	if (*ARTcache == i)
	    return ARTcache - ARTnumbers;
	bottom = ARTcache;
    }
    else {
	ARTcache = NULL;
	bottom = ARTnumbers;
    }

    for ( ; ; ) {
	if (i < *bottom || i > *top)
	    break;

	middle = bottom + (top - bottom) / 2;
	if (i == *middle) {
	    /* Found it; update cache. */
	    ARTcache = middle;
	    return middle - ARTnumbers;
	}

	if (i > *middle)
	    bottom = middle + 1;
	else
	    top = middle;
    }
    return -1;
}

/*
**  Return the header from the specified file, or NULL if not found.
**  We can estimate the Lines header, if that's what's wanted.
*/
char *
GetHeader(header, IsLines)
    char		*header;
    BOOL		IsLines;
{
    static char		buff[40];
    char		*p;
    char		*q;
#ifdef ART_MMAP
    char		lastchar;
    char		*limit;
    static char		*retval = NULL;
    static int		retlen = 0;

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
#else
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
#endif

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
FUNCTYPE
CMDfetch(ac, av)
    int			ac;
    char		*av[];
{
    char		buff[SMBUF];
    SENDDATA		*what;
    BOOL		ok;
    ARTNUM		art;
    char		*msgid;

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
	Reply("%d %ld %s %s\r\n", what->ReplyCode, art, what->Item, av[1]);
	if (what->Type != STstat)
	    ARTsend(what->Type);
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
	(void)sprintf(buff, "%ld", ARTnumbers[ARTindex]);
    }
    else {
	if (strspn(av[1], "0123456789") != strlen(av[1])) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	(void)strcpy(buff, av[1]);
    }

    /* Move forward until we can find one. */
    while (!ARTopen(buff)) {
	if (ac > 1 || ++ARTindex >= ARTsize) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumbers[ARTindex]);
    }
    if ((msgid = GetHeader("Message-ID", FALSE)) == NULL) {
        Reply("%s\r\n", ARTnoartingroup);
	return;
    }
    Reply("%d %s %s %s\r\n", what->ReplyCode, buff, GetHeader("Message-ID", FALSE), what->Item); 
    if (what->Type != STstat)
	ARTsend(what->Type);
    if (ac > 1)
	ARTindex = ARTfind((ARTNUM)atol(buff));
}


/*
**  Go to the next or last (really previous) article in the group.
*/
FUNCTYPE
CMDnextlast(ac, av)
    int		ac;
    char	*av[];
{
    char	buff[SPOOLNAMEBUFF];
    char	idbuff[SMBUF];
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

    (void)sprintf(buff, "%ld", ARTnumbers[ARTindex]);
    while (!ARTopen(buff, idbuff)) {
	ARTindex += delta;
	if (ARTindex < 0 || ARTindex >= ARTsize) {
	    Reply("%d No %s article to retrieve.\r\n", errcode, message);
	    ARTindex = save;
	    return;
	}
	(void)sprintf(buff, "%ld", ARTnumbers[ARTindex]);
    }

    Reply("%d %s %s Article retrieved; request text separately.\r\n",
	   NNTP_NOTHING_FOLLOWS_VAL, buff, idbuff);

    if (ac > 1)
	ARTindex = ARTfind((ARTNUM)atol(buff));
}


STATIC BOOL
CMDgetrange(ac, av, rp)
    int			ac;
    char		*av[];
    register ARTRANGE	*rp;
{
    register char	*p;

    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return FALSE;
    }

    if (ac == 1) {
	/* No argument, do only current article. */
	if (ARTindex < 0 || ARTindex >= ARTsize) {
	    Reply("%s\r\n", ARTnocurrart);
	    return FALSE;
	}
	rp->High = rp->Low = ARTnumbers[ARTindex];
	return TRUE;
    }

    /* Got just a single number? */
    if ((p = strchr(av[1], '-')) == NULL) {
	rp->Low = rp->High = atol(av[1]);
	return TRUE;
    }

    /* Parse range. */
    *p++ = '\0';
    rp->Low = atol(av[1]);
    if (ARTsize) {
	if (*p == '\0' || (rp->High = atol(p)) < rp->Low)
	    /* "XHDR 234-0 header" gives everything to the end. */
	    rp->High = ARTnumbers[ARTsize - 1];
	else if (rp->High > ARTnumbers[ARTsize - 1])
	    rp->High = ARTnumbers[ARTsize - 1];
	if (rp->Low < ARTnumbers[0])
	    rp->Low = ARTnumbers[0];
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
STATIC char *
OVERGetHeader(p, field)
    register char	*p;
    int			field;
{
    static char		*buff;
    static int		buffsize;
    register int	i;
    ARTOVERFIELD	*fp;
    char		*next;

    fp = &ARTfields[field - 1];

    if (fp->NeedsHeader) 		/* we're going to need an exact match */
      field = ARTfirstfullfield;

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
STATIC BOOL
OVERopen()
{
    char	name[SPOOLNAMEBUFF];
    char	index[SPOOLNAMEBUFF];
    int		fd;
    struct stat	sb;

    /* Already open? */
#ifdef OVER_MMAP
    if (OVERmem != NULL)
#else
    if (OVERfp != NULL)
#endif
	/* Don't rewind -- we are probably going forward via repeated
	 * NNTP commands. */
	return TRUE;

    /* Failed here before? */
    if (OVERopens++)
	return FALSE;

    OVERarticle = 0;
#ifndef OVER_MMAP
    OVERoffset = 0;
#endif
    OVERioff = 0;
    (void)sprintf(name, "%s/%s/%s", _PATH_OVERVIEWDIR, GRPlast, _PATH_OVERVIEW);
#ifdef OVER_MMAP
    if ((fd = open(name, O_RDONLY)) < 0)
    	return FALSE;
    if (fstat(fd, &sb) != 0) {
    	close(fd);
    	return FALSE;
    }
    OVERlen = sb.st_size;
    if ((int)(OVERmem = (char *)mmap(0, OVERlen, PROT_READ, MAP_SHARED, fd, 0)) == -1) {
        OVERmem = NULL;
    	close(fd);
    	return FALSE;
    }
    close(fd);
#else
    if ((OVERfp = fopen(name, "r")) == NULL)
    	return FALSE;
#endif
    (void)sprintf(index, "%s/%s/%s.index", _PATH_OVERVIEWDIR, GRPlast, _PATH_OVERVIEW);
    
    OVERindex = NULL;
    if ((fd = open(index, O_RDONLY)) < 0)
    	return TRUE;
    if (fstat(fd, &sb) != 0) {
    	close(fd);
    	return TRUE;
    }
    OVERicount = sb.st_size / sizeof(OVERINDEX);
    if ((OVERindex = malloc(OVERicount * sizeof(OVERINDEX))) == NULL) {
    	close(fd);
    	return TRUE;
    }
    if (read(fd, OVERindex, OVERicount * sizeof(OVERINDEX)) != (OVERicount * sizeof(OVERINDEX))) {
    	free(OVERindex);
    	OVERindex == NULL;
    } else 
        OVERread += OVERicount * sizeof(OVERINDEX);
    close(fd);
    return TRUE;
}


/*
**  Close the OVERVIEW file.
*/
void
OVERclose()
{
#ifdef OVER_MMAP
    if (OVERmem != NULL) {
    	munmap(OVERmem, OVERlen);
    	OVERmem = NULL;
    	OVERopens = 0;
    	OVERlen = 0;
    }
#else
    if (OVERfp != NULL) {
	fclose(OVERfp);
	OVERfp = NULL;
	OVERopens = 0;
    }
#endif
    if (OVERindex != NULL) {
    	free(OVERindex);
    	OVERindex = NULL;
    }
}


/*
**  Return the overview data for an article or NULL on failure.
**  Assumes that what we return is never modified.
*/
STATIC char *
OVERfind(artnum, linelen)
    ARTNUM	artnum;
    int		*linelen;
{
    int		i, j;
#ifdef OVER_MMAP
    char	*OVERline, *q;

    if (OVERmem == NULL)
#else
    if (OVERfp == NULL)
#endif
	return NULL;

    if (OVERindex != NULL) {
    	for (i = 0; i < OVERicount; i++) {
    	    j = (i + OVERioff) % OVERicount;
    	    if (OVERindex[j].artnum == artnum) {
#ifdef OVER_MMAP
		OVERline = (char *)OVERmem + OVERindex[j].offset;
                if ((OVERline >= (OVERmem + OVERlen)) || (OVERline < OVERmem))
                    return NULL;
                for (q = OVERline; q < (OVERmem+OVERlen); q++)
                    if ((*q == '\r') || (*q == '\n'))
                        break;

                *linelen = q - OVERline;
#else
    	        if (OVERoffset != OVERindex[j].offset) {
    	            fseek(OVERfp, OVERindex[j].offset, SEEK_SET);
    	            OVERoffset = OVERindex[j].offset;
    	        }
    	        if (fgets(OVERline, MAXOVERLINE, OVERfp) == NULL)
    	            return NULL;
    	        if ((*linelen = strlen(OVERline)) < 10)
    	            return NULL;
    	        if (*linelen  >= MAXOVERLINE) {
    	            OVERoffset = -1;
    	            *linelen = MAXOVERLINE;
    	        } else {
    	            OVERoffset += *linelen;
    	        }
   	        OVERline[*linelen-1] = '\0';
#endif    
    	        OVERread += *linelen;
    	        OVERioff = j;
    	        return OVERline;
            }
        }
        return NULL;
    }

#ifdef OVER_MMAP
    return NULL;
#else

    if (OVERarticle > artnum) {
	rewind(OVERfp);
	OVERarticle = 0;
	OVERline[0] = '\0';
	OVERoffset = 0;
    }
    for ( ; OVERarticle < artnum; OVERarticle = atol(OVERline)) {
        if (fgets(OVERline, MAXOVERLINE, OVERfp) == NULL)
	     return NULL;
	OVERread += strlen(OVERline);
    	while ((strlen(OVERline) == MAXOVERLINE) && (OVERline[MAXOVERLINE-1] == '\n'))
	    if (fgets(OVERline, MAXOVERLINE, OVERfp) == NULL)
	         return NULL;
	    else
	        OVERread += (*linelen = strlen(OVERline));
    }
    OVERoffset = -1;

    return OVERarticle == artnum ? OVERline : NULL;
#endif
}


/*
**  Read an article and create an overview line without the trailing
**  newline.  Returns pointer to static space or NULL on error.
*/
STATIC char *
OVERgen(name)
    char			*name;
{
    static ARTOVERFIELD		*Headers;
    static char			*buff;
    static int			buffsize;
    register ARTOVERFIELD	*fp;
    register ARTOVERFIELD	*hp;
    register QIOSTATE		*qp;
    register char		*colon;
    register char		*line;
    register char		*p;
    register int		i;
    register int		size;
    register int		ov_size;
    register long		lines;
    struct stat			Sb;
    long			t;
    char			value[10];

    lines = 0;
    /* Open article. */
    if ((qp = QIOopen(name, QIO_BUFFER)) == NULL)
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
FUNCTYPE
CMDxhdr(ac, av)
    int			ac;
    char		*av[];
{
    register ARTNUM	i;
    register char	*p;
    int			Overview;
    BOOL		IsLines;
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
    if (!CMDgetrange(ac - 1, av + 1, &range))
	return;

    /* Is this a header in our overview? */
    for (Overview = 0, i = 0; i < ARTfieldsize; i++)
	if (caseEQ(ARTfields[i].Header, av[1])) {
	    if (OVERopen())
		Overview = i + 1;
	    break;
	}

    Reply("%d %s fields follow\r\n", NNTP_HEAD_FOLLOWS_VAL, av[1]);
    for (i = range.Low; i <= range.High; i++) {
	if (ARTfind(i) < 0)
	    continue;

	/* Get it from the overview? */
	if (Overview && (p = OVERfind(i, &linelen)) != NULL) {
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
}


/*
**  XOVER another extension.  Dump parts of the overview database.
*/
FUNCTYPE
CMDxover(ac, av)
    int			ac;
    char		*av[];
{
    register char	*p;
    register ARTNUM	i;
    register BOOL	Opened;
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
    if (!CMDgetrange(ac, av, &range))
	return;

    OVERcount++;
    gettimeofday(&stv, NULL);
    Reply("%d data follows\r\n", NNTP_OVERVIEW_FOLLOWS_VAL);
    for (Opened = OVERopen(), i = range.Low; i <= range.High; i++) {
	if (ARTfind(i) < 0)
	    continue;

	if (Opened && (p = OVERfind(i, &linelen)) != NULL) {
	    OVERhit++;
	    OVERsize+=linelen;
#ifdef OVER_MMAP
	    SendIOb(p, linelen);
	    SendIOb("\r\n", 2); 
#else
            Printf("%s\r\n", p);
#endif
	    continue;
	}

	(void)sprintf(buff, "%ld", i);
	if ((p = OVERgen(buff)) != NULL) {
	    OVERmiss++;
	    linelen = strlen(p);
	    OVERsize+=linelen;
#ifdef OVER_MMAP
	    SendIOb(p, linelen);
	    SendIOb("\r\n", 2); 
#else
            Printf("%s\r\n", p);
#endif
	}
    }
#ifdef OVER_MMAP
    SendIOb(".\r\n", 3);
    PushIOb(); 
#else
    Printf(".\r\n");
#endif
    gettimeofday(&etv, NULL);
    OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
    OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;

}


/*
**  XPAT, an uncommon extension.  Print only headers that match the pattern.
*/
/* ARGSUSED */
FUNCTYPE
CMDxpat(ac, av)
    int			ac;
    char		*av[];
{
    register char	*p;
    register QIOSTATE	*qp;
    register ARTNUM	i;
    ARTRANGE		range;
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
    if (!CMDgetrange(ac - 1, av + 1, &range))
	return;

    /* In overview? */
    for (Overview = 0, i = 0; i < ARTfieldsize; i++)
	if (caseEQ(ARTfields[i].Header, av[1])) {
	    if (OVERopen())
		Overview = i + 1;
	    break;
	}

    Printf("%d %s matches follow.\r\n", NNTP_HEAD_FOLLOWS_VAL, header);
    for (pattern = Glom(&av[3]), i = range.Low; i <= range.High; i++) {
	if (ARTfind(i) < 0)
	    continue;

	/* Get it from the Overview? */
	if (Overview
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
}
