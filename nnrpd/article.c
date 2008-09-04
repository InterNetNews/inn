/*  $Id$
**
**  Article-related routines.
*/

#include "config.h"
#include "clibrary.h"
#include <assert.h>
#if HAVE_LIMITS_H
# include <limits.h>
#endif
#include <sys/uio.h>

#include "inn/innconf.h"
#include "inn/messages.h"
#include "inn/wire.h"
#include "nnrpd.h"
#include "ov.h"
#include "tls.h"
#include "cache.h"

#ifdef HAVE_SSL
extern SSL *tls_conn;
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
    SENDTYPE    Type;
    int         ReplyCode;
    const char *Item;
} SENDDATA;

static char		ARTnotingroup[] = NNTP_NOTINGROUP;
static char		ARTnoartingroup[] = NNTP_NOARTINGRP;
static char		ARTnocurrart[] = NNTP_NOCURRART;
static ARTHANDLE        *ARThandle = NULL;
static SENDDATA		SENDbody = {
    STbody,	NNTP_BODY_FOLLOWS_VAL,		"body"
};
static SENDDATA		SENDarticle = {
    STarticle,	NNTP_ARTICLE_FOLLOWS_VAL,	"article"
};
static SENDDATA		SENDstat = {
    STstat,	NNTP_NOTHING_FOLLOWS_VAL,	"status"
};
static SENDDATA		SENDhead = {
    SThead,	NNTP_HEAD_FOLLOWS_VAL,		"head"
};


static struct iovec	iov[IOV_MAX > 1024 ? 1024 : IOV_MAX];
static int		queued_iov = 0;

static void PushIOvHelper(struct iovec* vec, int* countp) {
    int result;
    TMRstart(TMR_NNTPWRITE);
#ifdef HAVE_SSL
    if (tls_conn) {
Again:
        result = SSL_writev(tls_conn, vec, *countp);
        switch (SSL_get_error(tls_conn, result)) {
        case SSL_ERROR_NONE:
        case SSL_ERROR_SYSCALL:
            break;
        case SSL_ERROR_WANT_WRITE:
            goto Again;
            break;
        case SSL_ERROR_SSL:
            SSL_shutdown(tls_conn);
            tls_conn = NULL;
            errno = ECONNRESET;
            break;
        case SSL_ERROR_ZERO_RETURN:
            break;
        }
    } else {
        result = xwritev(STDOUT_FILENO, vec, *countp);
    }
#else
    result = xwritev(STDOUT_FILENO, vec, *countp);
#endif
    TMRstop(TMR_NNTPWRITE);
    if (result == -1) {
	/* we can't recover, since we can't resynchronise with our
	 * peer */
	ExitWithStats(1, true);
    }
    *countp = 0;
}

static void
PushIOvRateLimited(void) {
    double              start, end, elapsed, target;
    struct iovec        newiov[IOV_MAX > 1024 ? 1024 : IOV_MAX];
    int                 newiov_len;
    int                 sentiov;
    int                 i;
    int                 bytesfound;
    int                 chunkbittenoff;
    struct timeval      waittime;

    while (queued_iov) {
	bytesfound = newiov_len = 0;
	sentiov = 0;
	for (i = 0; (i < queued_iov) && (bytesfound < MaxBytesPerSecond); i++) {
	    if ((signed)iov[i].iov_len + bytesfound > MaxBytesPerSecond) {
		chunkbittenoff = MaxBytesPerSecond - bytesfound;
		newiov[newiov_len].iov_base = iov[i].iov_base;
		newiov[newiov_len++].iov_len = chunkbittenoff;
		iov[i].iov_base = (char *)iov[i].iov_base + chunkbittenoff;
		iov[i].iov_len -= chunkbittenoff;
		bytesfound += chunkbittenoff;
	    } else {
		newiov[newiov_len++] = iov[i];
		sentiov++;
		bytesfound += iov[i].iov_len;
	    }
	}
	assert(sentiov <= queued_iov);
        start = TMRnow_double();
	PushIOvHelper(newiov, &newiov_len);
        end = TMRnow_double();
        target = (double) bytesfound / MaxBytesPerSecond;
        elapsed = end - start;
        if (elapsed < 1 && elapsed < target) {
            waittime.tv_sec = 0;
            waittime.tv_usec = (target - elapsed) * 1e6;
            start = TMRnow_double();
	    if (select(0, NULL, NULL, NULL, &waittime) != 0)
                syswarn("%s: select in PushIOvRateLimit failed", ClientHost);
            end = TMRnow_double();
            IDLEtime += end - start;
	}
	memmove(iov, &iov[sentiov], (queued_iov - sentiov) * sizeof(struct iovec));
	queued_iov -= sentiov;
    }
}

static void
PushIOv(void) {
    TMRstart(TMR_NNTPWRITE);
    fflush(stdout);
    TMRstop(TMR_NNTPWRITE);
    if (MaxBytesPerSecond != 0)
	PushIOvRateLimited();
    else
	PushIOvHelper(iov, &queued_iov);
}

static void
SendIOv(const char *p, int len) {
    char                *q;

    if (queued_iov) {
	q = (char *)iov[queued_iov - 1].iov_base + iov[queued_iov - 1].iov_len;
	if (p == q) {
	    iov[queued_iov - 1].iov_len += len;
	    return;
	}
    }
    iov[queued_iov].iov_base = (char*)p;
    iov[queued_iov++].iov_len = len;
    if (queued_iov == IOV_MAX)
	PushIOv();
}

static char		*_IO_buffer_ = NULL;
static int		highwater = 0;

static void
PushIOb(void) {
    TMRstart(TMR_NNTPWRITE);
    fflush(stdout);
#ifdef HAVE_SSL
    if (tls_conn) {
        int r;
Again:
        r = SSL_write(tls_conn, _IO_buffer_, highwater);
        switch (SSL_get_error(tls_conn, r)) {
        case SSL_ERROR_NONE:
        case SSL_ERROR_SYSCALL:
            break;
        case SSL_ERROR_WANT_WRITE:
            goto Again;
            break;
        case SSL_ERROR_SSL:
            SSL_shutdown(tls_conn);
            tls_conn = NULL;
            errno = ECONNRESET;
            break;
        case SSL_ERROR_ZERO_RETURN:
            break;
        }
        if (r != highwater) {
            TMRstop(TMR_NNTPWRITE);
            highwater = 0;
            return;
        }
    } else {
      if (xwrite(STDOUT_FILENO, _IO_buffer_, highwater) != highwater) {
	TMRstop(TMR_NNTPWRITE);
	highwater = 0;
	return;
      }
    }
#else
    if (xwrite(STDOUT_FILENO, _IO_buffer_, highwater) != highwater) {
	TMRstop(TMR_NNTPWRITE);
        highwater = 0;
        return;
    }
#endif
    TMRstop(TMR_NNTPWRITE);
    highwater = 0;
}

static void
SendIOb(const char *p, int len) {
    int tocopy;
    
    if (_IO_buffer_ == NULL)
        _IO_buffer_ = xmalloc(BIG_BUFFER);

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


/*
**  If we have an article open, close it.
*/
void ARTclose(void)
{
    if (ARThandle) {
	SMfreearticle(ARThandle);
	ARThandle = NULL;
    }
}

bool ARTinstorebytoken(TOKEN token)
{
    ARTHANDLE *art;
    struct timeval	stv, etv;

    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&stv, NULL);
    }
    art = SMretrieve(token, RETR_STAT); /* XXX This isn't really overstats, is it? */
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERartcheck+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERartcheck+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if (art) {
	SMfreearticle(art);
	return true;
    } 
    return false;
}

/*
**  If the article name is valid, open it and stuff in the ID.
*/
static bool ARTopen(ARTNUM artnum)
{
    static ARTNUM	save_artnum;
    TOKEN		token;

    /* Re-use article if it's the same one. */
    if (save_artnum == artnum) {
	if (ARThandle)
	    return true;
    }
    ARTclose();

    if (!OVgetartinfo(GRPcur, artnum, &token))
	return false;
  
    TMRstart(TMR_READART);
    ARThandle = SMretrieve(token, RETR_ALL);
    TMRstop(TMR_READART);
    if (ARThandle == NULL) {
	return false;
    }

    save_artnum = artnum;
    return true;
}


/*
**  Open the article for a given Message-ID.
*/
static bool
ARTopenbyid(char *msg_id, ARTNUM *ap, bool final)
{
    TOKEN token;

    *ap = 0;
    token = cache_get(HashMessageID(msg_id), final);
    if (token.type == TOKEN_EMPTY) {
	if (History == NULL) {
	    time_t statinterval;

	    /* Do lazy opens of the history file - lots of clients
	     * will never ask for anything by message id, so put off
	     * doing the work until we have to */
	    History = HISopen(HISTORY, innconf->hismethod, HIS_RDONLY);
	    if (!History) {
		syslog(L_NOTICE, "cant initialize history");
		Reply("%d NNTP server unavailable. Try later.\r\n",
		      NNTP_TEMPERR_VAL);
		ExitWithStats(1, true);
	    }
	    statinterval = 30;
	    HISctl(History, HISCTLS_STATINTERVAL, &statinterval);
	}
	if (!HISlookup(History, msg_id, NULL, NULL, NULL, &token))
	    return false;
    }
    if (token.type == TOKEN_EMPTY)
	return false;
    TMRstart(TMR_READART);
    ARThandle = SMretrieve(token, RETR_ALL);
    TMRstop(TMR_READART);
    if (ARThandle == NULL) {
	return false;
    }

    return true;
}

/*
**  Send a (part of) a file to stdout, doing newline and dot conversion.
*/
static void ARTsendmmap(SENDTYPE what)
{
    const char		*p, *q, *r;
    const char		*s, *path, *xref, *endofpath;
    long		bytecount;
    char		lastchar;

    ARTcount++;
    GRParticles++;
    bytecount = 0;
    lastchar = -1;

    /* Get the headers and detect if wire format. */
    if (what == STarticle) {
	q = ARThandle->data;
	p = ARThandle->data + ARThandle->len;
     } else {
	for (q = p = ARThandle->data; p < (ARThandle->data + ARThandle->len); p++) {
	    if (*p == '\r')
		continue;
	    if (*p == '\n') {
		if (lastchar == '\n') {
		    if (what == SThead) {
			if (*(p-1) == '\r')
			    p--;
			break;
		    } else {
			q = p + 1;
			p = ARThandle->data + ARThandle->len;
			break;
		    }
		}
	    }
	    lastchar = *p;
	}
    }

    /* q points to the start of the article buffer, p to the end of it */
    if (VirtualPathlen > 0 && (what != STbody)) {
        path = wire_findheader(ARThandle->data, ARThandle->len, "Path");
        if (path == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    ARTget++;
	    return;
	} else {
            xref = wire_findheader(ARThandle->data, ARThandle->len, "Xref");
            if (xref == NULL) {
                SendIOv(".\r\n", 3);
                ARTgetsize += 3;
                PushIOv();
                ARTget++;
                return;
            }
        }
        endofpath = wire_endheader(path, ARThandle->data + ARThandle->len - 1);
        if (endofpath == NULL) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    ARTget++;
	    return;
	}
	if ((r = memchr(xref, ' ', p - xref)) == NULL || r == p) {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	    PushIOv();
	    ARTget++;
	    return;
	}
	/* r points to the first space in the Xref header */
	for (s = path, lastchar = '\0';
	    s + VirtualPathlen + 1 < endofpath;
	    lastchar = *s++) {
	    if ((lastchar != '\0' && lastchar != '!') || *s != *VirtualPath ||
		strncmp(s, VirtualPath, VirtualPathlen - 1) != 0)
		continue;
	    if (*(s + VirtualPathlen - 1) != '\0' &&
		*(s + VirtualPathlen - 1) != '!')
		continue;
	    break;
	}
	if (s + VirtualPathlen + 1 < endofpath) {
	    if (xref > path) {
	        SendIOv(q, path - q);
	        SendIOv(s, xref - s);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, p - r);
	    } else {
	        SendIOv(q, xref - q);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, path - r);
	        SendIOv(s, p - s);
	    }
	} else {
	    if (xref > path) {
	        SendIOv(q, path - q);
	        SendIOv(VirtualPath, VirtualPathlen);
	        SendIOv(path, xref - path);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, p - r);
	    } else {
	        SendIOv(q, xref - q);
	        SendIOv(VirtualPath, VirtualPathlen - 1);
	        SendIOv(r, path - r);
	        SendIOv(VirtualPath, VirtualPathlen);
	        SendIOv(path, p - path);
	    }
	}
    } else
	SendIOv(q, p - q);
    ARTgetsize += p - q;
    if (what == SThead) {
	SendIOv(".\r\n", 3);
	ARTgetsize += 3;
    } else if (memcmp((ARThandle->data + ARThandle->len - 5), "\r\n.\r\n", 5)) {
	if (memcmp((ARThandle->data + ARThandle->len - 2), "\r\n", 2)) {
	    SendIOv("\r\n.\r\n", 5);
	    ARTgetsize += 5;
	} else {
	    SendIOv(".\r\n", 3);
	    ARTgetsize += 3;
	}
    }
    PushIOv();

    ARTget++;
}

/*
**  Return the header from the specified file, or NULL if not found.
*/
char *GetHeader(const char *header)
{
    const char		*p, *q, *r, *s, *t;
    char		*w, prevchar;
    /* Bogus value here to make sure that it isn't initialized to \n */
    char		lastchar = ' ';
    const char		*limit;
    const char		*cmplimit;
    static char		*retval = NULL;
    static int		retlen = 0;
    int			headerlen;
    bool		pathheader = false;
    bool		xrefheader = false;

    limit = ARThandle->data + ARThandle->len;
    cmplimit = ARThandle->data + ARThandle->len - strlen(header) - 1;
    for (p = ARThandle->data; p < cmplimit; p++) {
	if (*p == '\r')
	    continue;
	if ((lastchar == '\n') && (*p == '\n')) {
	    return NULL;
	}
	if ((lastchar == '\n') || (p == ARThandle->data)) {
	    headerlen = strlen(header);
	    if (strncasecmp(p, header, headerlen) == 0 && p[headerlen] == ':') {
		for (; (p < limit) && !isspace((int)*p) ; p++);
		for (; (p < limit) && isspace((int)*p) ; p++);
		for (q = p; q < limit; q++) 
		    if ((*q == '\r') || (*q == '\n')) {
			/* Check for continuation header lines */
			t = q + 1;
			if (t < limit) {
			    if ((*q == '\r' && *t == '\n')) {
				t++;
				if (t == limit)
				    break;
			    }
			    if ((*t == '\t' || *t == ' ')) {
				for (; (t < limit) && isspace((int)*t) ; t++);
				q = t;
			    } else {
				break;
			    }
			} else {
			    break;
			}
		    }
		if (q == limit)
		    return NULL;
		if (strncasecmp("Path", header, headerlen) == 0)
		    pathheader = true;
		else if (strncasecmp("Xref", header, headerlen) == 0)
		    xrefheader = true;
		if (retval == NULL) {
		    retlen = q - p + VirtualPathlen + 1;
		    retval = xmalloc(retlen);
		} else {
		    if ((q - p + VirtualPathlen + 1) > retlen) {
			retlen = q - p + VirtualPathlen + 1;
                        retval = xrealloc(retval, retlen);
		    }
		}
		if (pathheader && (VirtualPathlen > 0)) {
                    const char *endofpath;
                    const char *endofarticle;

                    endofarticle = ARThandle->data + ARThandle->len - 1;
                    endofpath = wire_endheader(p, endofarticle);
                    if (endofpath == NULL)
                        return NULL;
		    for (s = p, prevchar = '\0';
			s + VirtualPathlen + 1 < endofpath;
			prevchar = *s++) {
			if ((prevchar != '\0' && prevchar != '!') ||
			    *s != *VirtualPath ||
			    strncmp(s, VirtualPath, VirtualPathlen - 1) != 0)
			    continue;
			if (*(s + VirtualPathlen - 1) != '\0' &&
			    *(s + VirtualPathlen - 1) != '!')
			    continue;
			break;
		    }
		    if (s + VirtualPathlen + 1 < endofpath) {
			memcpy(retval, s, q - s);
			*(retval + (int)(q - s)) = '\0';
		    } else {
			memcpy(retval, VirtualPath, VirtualPathlen);
			memcpy(retval + VirtualPathlen, p, q - p);
			*(retval + (int)(q - p) + VirtualPathlen) = '\0';
		    }
		} else if (xrefheader && (VirtualPathlen > 0)) {
		    if ((r = memchr(p, ' ', q - p)) == NULL)
			return NULL;
		    for (; (r < q) && isspace((int)*r) ; r++);
		    if (r == q)
			return NULL;
		    memcpy(retval, VirtualPath, VirtualPathlen - 1);
		    memcpy(retval + VirtualPathlen - 1, r - 1, q - r + 1);
		    *(retval + (int)(q - r) + VirtualPathlen) = '\0';
		} else {
		    memcpy(retval, p, q - p);
		    *(retval + (int)(q - p)) = '\0';
		}
		for (w = retval; *w; w++)
		    if (*w == '\n' || *w == '\r')
			*w = ' ';
		return retval;
	    }
	}
	lastchar = *p;
    }
    return NULL;
}

/*
**  Fetch part or all of an article and send it to the client.
*/
void CMDfetch(int ac, char *av[])
{
    char		buff[SMBUF];
    SENDDATA		*what;
    bool		ok;
    ARTNUM		art;
    char		*msgid;
    ARTNUM		tart;
    bool final = false;

    /* Find what to send; get permissions. */
    ok = PERMcanread;
    switch (*av[0]) {
    default:
	what = &SENDbody;
	final = true;
	break;
    case 'a': case 'A':
	what = &SENDarticle;
	final = true;
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
	if (!ARTopenbyid(av[1], &art, final)) {
	    Reply("%d No such article\r\n", NNTP_DONTHAVEIT_VAL);
	    return;
	}
	if (!PERMartok()) {
	    ARTclose();
	    Reply("%s\r\n", NOACCESS);
	    return;
	}
	tart=art;
	Reply("%d %lu %s %s\r\n", what->ReplyCode, (unsigned long) art,
              av[1], what->Item);
	if (what->Type != STstat) {
	    ARTsendmmap(what->Type);
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
	if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	    Reply("%s\r\n", ARTnocurrart);
	    return;
	}
	snprintf(buff, sizeof(buff), "%d", ARTnumber);
	tart=ARTnumber;
    }
    else {
	if (strspn(av[1], "0123456789") != strlen(av[1])) {
	    Reply("%s\r\n", ARTnoartingroup);
	    return;
	}
	strlcpy(buff, av[1], sizeof(buff));
	tart=(ARTNUM)atol(buff);
    }

    /* Open the article and send the reply. */
    if (!ARTopen(atol(buff))) {
        Reply("%s\r\n", ARTnoartingroup);
        return;
    }
    if (ac > 1)
	ARTnumber = tart;
    if ((msgid = GetHeader("Message-ID")) == NULL) {
        Reply("%s\r\n", ARTnoartingroup);
	return;
    }
    Reply("%d %s %.512s %s\r\n", what->ReplyCode, buff, msgid, what->Item); 
    if (what->Type != STstat)
	ARTsendmmap(what->Type);
    ARTclose();
}


/*
**  Go to the next or last (really previous) article in the group.
*/
void CMDnextlast(int ac UNUSED, char *av[])
{
    char *msgid;
    int	save, delta, errcode;
    bool next;
    const char *message;

    if (!PERMcanread) {
	Reply("%s\r\n", NOACCESS);
	return;
    }
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	return;
    }
    if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
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

    save = ARTnumber;
    msgid = NULL;
    do {
        ARTnumber += delta;
        if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
            Reply("%d No %s to retrieve.\r\n", errcode, message);
            ARTnumber = save;
            return;
        }
        if (!ARTopen(ARTnumber))
            continue;
        msgid = GetHeader("Message-ID");
    } while (msgid == NULL);

    ARTclose();
    Reply("%d %d %s Article retrieved; request text separately.\r\n",
	   NNTP_NOTHING_FOLLOWS_VAL, ARTnumber, msgid);
}


static bool CMDgetrange(int ac, char *av[], ARTRANGE *rp, bool *DidReply)
{
    char		*p;

    *DidReply = false;
    if (GRPcount == 0) {
	Reply("%s\r\n", ARTnotingroup);
	*DidReply = true;
	return false;
    }

    if (ac == 1) {
	/* No argument, do only current article. */
	if (ARTnumber < ARTlow || ARTnumber > ARThigh) {
	    Reply("%s\r\n", ARTnocurrart);
	    *DidReply = true;
	    return false;
	}
	rp->High = rp->Low = ARTnumber;
        return true;
    }

    /* Got just a single number? */
    if ((p = strchr(av[1], '-')) == NULL) {
	rp->Low = rp->High = atol(av[1]);
        return true;
    }

    /* Parse range. */
    *p++ = '\0';
    rp->Low = atol(av[1]);
	if (*p == '\0' || (rp->High = atol(p)) < rp->Low)
	    /* "XHDR 234-0 header" gives everything to the end. */
	rp->High = ARThigh;
    else if (rp->High > ARThigh)
	rp->High = ARThigh;
    if (rp->Low < ARTlow)
	rp->Low = ARTlow;
    p--;
    *p = '-';

    return true;
}


/*
**  Apply virtual hosting to an Xref field.
*/
static char *
vhost_xref(char *p)
{
    char *space;
    size_t offset;
    char *field = NULL;

    space = strchr(p, ' ');
    if (space == NULL) {
	warn("malformed Xref `%s'", field);
	goto fail;
    }
    offset = space + 1 - p;
    space = strchr(p + offset, ' ');
    if (space == NULL) {
	warn("malformed Xref `%s'", field);
	goto fail;
    }
    field = concat(PERMaccessconf->domain, space, NULL);
 fail:
    free(p);
    return field;
}

/*
**  XOVER another extension.  Dump parts of the overview database.
*/
void CMDxover(int ac, char *av[])
{
    bool	        DidReply;
    ARTRANGE		range;
    struct timeval	stv, etv;
    ARTNUM		artnum;
    void		*handle;
    char		*data, *r;
    const char		*p, *q;
    int			len, useIOb = 0;
    TOKEN		token;
    struct cvector *vector = NULL;

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
    if (!CMDgetrange(ac, av, &range, &DidReply)) {
	if (DidReply) {
	    return;
	}
    }

    OVERcount++;
    gettimeofday(&stv, NULL);
    if ((handle = (void *)OVopensearch(GRPcur, range.Low, range.High)) == NULL) {
	if (av[1] != NULL)
	    Reply("%d %s fields follow\r\n.\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, av[1]);
	else
	    Reply("%d %d fields follow\r\n.\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, ARTnumber);
	return;
    }
    if (PERMaccessconf->nnrpdoverstats) {
	gettimeofday(&etv, NULL);
	OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
	OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

    if (av[1] != NULL)
	Reply("%d %s fields follow\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, av[1]);
    else
	Reply("%d %d fields follow\r\n", NNTP_OVERVIEW_FOLLOWS_VAL, ARTnumber);
    fflush(stdout);
    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);

    /* If OVSTATICSEARCH is true, then the data returned by OVsearch is only
       valid until the next call to OVsearch.  In this case, we must use
       SendIOb because it copies the data. */
    OVctl(OVSTATICSEARCH, &useIOb);

    while (OVsearch(handle, &artnum, &data, &len, &token, NULL)) {
	if (PERMaccessconf->nnrpdoverstats) {
	    gettimeofday(&etv, NULL);
	    OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
	    OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
	}
	if (len == 0 || (PERMaccessconf->nnrpdcheckart && !ARTinstorebytoken(token))) {
	    if (PERMaccessconf->nnrpdoverstats) {
		OVERmiss++;
		gettimeofday(&stv, NULL);
	    }
	    continue;
	}
	if (PERMaccessconf->nnrpdoverstats) {
	    OVERhit++;
	    OVERsize += len;
	}
	vector = overview_split(data, len, NULL, vector);
	r = overview_getheader(vector, OVERVIEW_MESSAGE_ID, OVextra);
	cache_add(HashMessageID(r), token);
	free(r);
	if (VirtualPathlen > 0 && overhdr_xref != -1) {
	    if ((overhdr_xref + 1) >= vector->count)
		continue;
	    p = vector->strings[overhdr_xref] + sizeof("Xref: ") - 1;
	    while ((p < data + len) && *p == ' ')
		++p;
	    q = memchr(p, ' ', data + len - p);
	    if (q == NULL)
		continue;
	    if(useIOb) {
		SendIOb(data, p - data);
		SendIOb(VirtualPath, VirtualPathlen - 1);
		SendIOb(q, len - (q - data));
	    } else {
		SendIOv(data, p - data);
		SendIOv(VirtualPath, VirtualPathlen - 1);
		SendIOv(q, len - (q - data));
	    }
	} else {
	    if(useIOb)
		SendIOb(data, len);
	    else
		SendIOv(data, len);
	}
	if (PERMaccessconf->nnrpdoverstats)
	    gettimeofday(&stv, NULL);
    }

    if (vector)
	cvector_free(vector);

    if (PERMaccessconf->nnrpdoverstats) {
        gettimeofday(&etv, NULL);
        OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
        OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }
    if(useIOb) {
	SendIOb(".\r\n", 3);
	PushIOb();
    } else {
	SendIOv(".\r\n", 3);
	PushIOv();
    }
    if (PERMaccessconf->nnrpdoverstats)
	gettimeofday(&stv, NULL);
    OVclosesearch(handle);
    if (PERMaccessconf->nnrpdoverstats) {
        gettimeofday(&etv, NULL);
        OVERtime+=(etv.tv_sec - stv.tv_sec) * 1000;
        OVERtime+=(etv.tv_usec - stv.tv_usec) / 1000;
    }

}

/*
**  XHDR and XPAT extensions.  Note that HDR as specified in the new NNTP
**  draft works differently than XHDR has historically, so don't just use this
**  function to implement it without reviewing the differences.
*/
/* ARGSUSED */
void CMDpat(int ac, char *av[])
{
    char	        *p;
    int	        	i;
    ARTRANGE		range;
    bool		IsLines;
    bool		DidReply;
    char		*header;
    char		*pattern;
    char		*text;
    int			Overview;
    ARTNUM		artnum;
    char		buff[SPOOLNAMEBUFF];
    void                *handle;
    char                *data;
    int                 len;
    TOKEN               token;
    struct cvector *vector = NULL;

    if (!PERMcanread) {
	Printf("%s\r\n", NOACCESS);
	return;
    }

    header = av[1];
    IsLines = (strcasecmp(header, "lines") == 0);

    if (ac > 3) /* XPAT */
	pattern = Glom(&av[3]);
    else
	pattern = NULL;

    do {
	/* Message-ID specified? */
	if (ac > 2 && av[2][0] == '<') {
	    p = av[2];
	    if (!ARTopenbyid(p, &artnum, false)) {
		Printf("%d No such article.\r\n", NNTP_DONTHAVEIT_VAL);
		break;
	    }
	    Printf("%d %s matches follow (ID)\r\n", NNTP_HEAD_FOLLOWS_VAL,
		   header);
	    if ((text = GetHeader(header)) != NULL
		&& (!pattern || uwildmat_simple(text, pattern)))
		Printf("%s %s\r\n", p, text);

	    ARTclose();
	    Printf(".\r\n");
	    break;
	}

	if (GRPcount == 0) {
	    Reply("%s\r\n", ARTnotingroup);
	    break;
	}

	/* Range specified. */
	if (!CMDgetrange(ac - 1, av + 1, &range, &DidReply)) {
	    if (DidReply) {
		break;
	    }
	}

	/* In overview? */
        Overview = overview_index(header, OVextra);

	/* Not in overview, we have to fish headers out from the articles */
	if (Overview < 0 ) {
	    Reply("%d %s matches follow (art)\r\n", NNTP_HEAD_FOLLOWS_VAL,
		  header);
	    for (i = range.Low; i <= range.High && range.High > 0; i++) {
		if (!ARTopen(i))
		    continue;
		p = GetHeader(header);
		if (p && (!pattern || uwildmat_simple(p, pattern))) {
		    snprintf(buff, sizeof(buff), "%u ", i);
		    SendIOb(buff, strlen(buff));
		    SendIOb(p, strlen(p));
		    SendIOb("\r\n", 2);
		    ARTclose();
		}
	    }
	    SendIOb(".\r\n", 3);
	    PushIOb();
	    break;
	}

	/* Okay then, we can grab values from overview. */
	handle = (void *)OVopensearch(GRPcur, range.Low, range.High);
	if (handle == NULL) {
	    Reply("%d %s no matches follow (NOV)\r\n.\r\n",
		  NNTP_HEAD_FOLLOWS_VAL, header);
	    break;
	}	
	
	Printf("%d %s matches follow (NOV)\r\n", NNTP_HEAD_FOLLOWS_VAL,
	       header);
	while (OVsearch(handle, &artnum, &data, &len, &token, NULL)) {
	    if (len == 0 || (PERMaccessconf->nnrpdcheckart
		&& !ARTinstorebytoken(token)))
		continue;
	    vector = overview_split(data, len, NULL, vector);
	    p = overview_getheader(vector, Overview, OVextra);
	    if (p != NULL) {
		if (PERMaccessconf->virtualhost &&
			   Overview == overhdr_xref) {
		    p = vhost_xref(p);
		    if (p == NULL)
			continue;
		}
		if (!pattern || uwildmat_simple(p, pattern)) {
		    snprintf(buff, sizeof(buff), "%lu ", artnum);
		    SendIOb(buff, strlen(buff));
		    SendIOb(p, strlen(p));
		    SendIOb("\r\n", 2);
		}
		free(p);
	    }
	}
	SendIOb(".\r\n", 3);
	PushIOb();
	OVclosesearch(handle);
    } while (0);

    if (vector)
	cvector_free(vector);

    if (pattern)
	free(pattern);
}
